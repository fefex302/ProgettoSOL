#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <fcntl.h>
#include <time.h>

#include "utils.h"
#include "hash.h"
#include "api.h"
#include "list.h"

#define BUF_LEN 100

#define NULL_EXIT(X,str)	\
  if ((X)==NULL) {			\
    perror(#str);			\
    exit(EXIT_FAILURE);			\
  }

#define ZERO_EXIT(X,str)	\
  if ((X)==0) {			\
    perror(#str);			\
    exit(EXIT_FAILURE);			\
  }

#define MINUS_ONE_EXIT(X,str)\
  if ((X)==-1) {			\
    perror(#str);			\
    exit(EXIT_FAILURE);			\
  }

#define LOCK(l)      if (pthread_mutex_lock(l)!=0)     { \
    if (pthread_mutex_lock(l)!=0){ ;		\
    	fprintf(stderr, "ERRORE FATALE lock\n");		    \
    	pthread_exit((void*)EXIT_FAILURE);			    \
  }}   

#define UNLOCK(l)    if (pthread_mutex_unlock(l)!=0)     { \
    if (pthread_mutex_lock(l)!=0){		\
    	fprintf(stderr, "ERRORE FATALE unlock\n");		    \
    	pthread_exit((void*)EXIT_FAILURE);			    \
  }}   

#define WAIT(c,l)    if (pthread_cond_wait(c,l)!=0)       { \
  if (pthread_cond_wait(c,l)!=0)       {	\
   		fprintf(stderr, "ERRORE FATALE wait\n");		    \
    	pthread_exit((void*)EXIT_FAILURE);				    \
}}

#define SIGNAL(c)    if (pthread_cond_signal(c)!=0)       {	\
	if (pthread_cond_signal(c)!=0)       {\
    	fprintf(stderr, "ERRORE FATALE signal\n");			\
    	pthread_exit((void*)EXIT_FAILURE);					\
  }}

//struttura che mi contiene tutti i parametri di configurazione
typedef struct _configs{
	long NUM_THREADS;
	long MAX_FILE_NUMBER;
	long SERVER_CAPACITY_MBYTES;
	long SERVER_CAPACITY_BYTES;
}config_parameters;

typedef struct _output{
	int max_file_stored;
	int max_size_reached;
	int replace_algo;
}output_info;

typedef struct _fifo{
	fileT *fileInServer;
	struct _fifo *next;
} fifoStruct;


void cleanup() {
    unlink(SOCKNAME);
}

// ritorno l'indice massimo tra i descrittori attivi
int updatemax(fd_set set, int fdmax) {
    for(int i=(fdmax-1);i>=0;--i)
	if (FD_ISSET(i, &set)) return i;
    
    return -1;
}


int fifo_insert(fileT *fileToInsert);
int fifo_remove(int connfd);
int fifo_remove_equal(char *pathname);
int config_file_parser(char *stringa, config_parameters* cfg);
int config_values_correctly_initialized(config_parameters *cfg);
void *worker_t(void *args);
int wrt(int connfd, int *error);
fileT* opn(int flag,int connfd, int *error);
int cls(int connfd,char* pathname);
int rm(int connfd, int *error);
int rd(int connfd, int *error);
int lo(int connfd, int *error);
int rdn(int connfd, int *error);
int app(int connfd, int *error);
//***********************************VARIABILI GLOBALI*************************************************************
static output_info out_put;				//dati da stampare a schermo all'uscita del programma
static config_parameters configs;		//parametri di configurazione passati dal file config.txt
static list *requests = NULL;			//lista fifo delle richieste dei client
static list *last = NULL;				//puntatore all'ultimo elemento della queue
static int numreq = 0;					//variabile che indica se il buffer delle richieste ?? vuoto o meno
volatile sig_atomic_t quit = 0;			//variabile che dice se bisogna uscire dal programma velocemente
volatile sig_atomic_t stop = 0;			//variabile che dice se bisogna uscire	dal programma finendo prima le richieste ricevute 
static hashtable *file_server;			//hashtable dove andr?? a memorizzare tutti i file
static int pfd[2];
static fd_set set;
static pthread_cond_t	bufcond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t	bufmtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t	servermtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t	serverstatsmtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t	filesopenedmtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t	fifoqueuemtx = PTHREAD_MUTEX_INITIALIZER;
static long file_stored = 0;
static long bytes_used = 0;
static listfiles *filesopened = NULL;
static fifoStruct *fifoqueue = NULL;
static fifoStruct *fifoqueueLast = NULL;

void sigterm(int signo) {
    switch(signo){
    	case SIGPIPE:
    		break;
    	case SIGINT:
    		quit = 1;
    		break;
    	case SIGQUIT:
    		quit = 1;
    		break;
    	case SIGHUP:
    		stop = 1;
    		break;
 
    }
}
//***********************************MAIN****************************************************************************

int main(int argc,char *argv[]){
	cleanup();
	if(argc != 2){
		printf("you must use a config file\n");
		return EXIT_FAILURE;
	}
	//********SIGHANDLER******
	struct sigaction s;
    s.sa_handler = sigterm;
    sigemptyset(&s.sa_mask);
    s.sa_flags = SA_RESTART;
    sigaction(SIGINT, &s, NULL);
    sigaction(SIGQUIT, &s, NULL);
    sigaction(SIGHUP, &s, NULL);
    sigaction(SIGPIPE, &s, NULL);

	FILE *config_file;

	NULL_EXIT((config_file = fopen(argv[1],"r")),"config file opening");

	char buffer[BUF_LEN];

	configs.NUM_THREADS = 0;
	configs.MAX_FILE_NUMBER = 0;
	configs.SERVER_CAPACITY_MBYTES = 0;

	int active_threads = 0;
	int i;

	for(i=0; i<3; i++){	//mi aspetto n valori da config file
		memset(buffer,'\0',BUF_LEN);

		if(fgets(buffer,BUF_LEN,config_file) == NULL){
			if(feof(config_file) != 0)
				break;
			else{
				printf("fgets error\n");
				return EXIT_FAILURE;
			}
		}

		if(config_file_parser(buffer,&configs) == -1){
			printf("config file: error\n");
			exit(EXIT_FAILURE);
		}
	}

	//controllo che i valori siano inizializzati correttamente
	if(config_values_correctly_initialized(&configs) != 1)
		exit(EXIT_FAILURE);

	fclose(config_file);

	configs.SERVER_CAPACITY_BYTES = configs.SERVER_CAPACITY_MBYTES * 1000000;

	//creo la tabella hash che memorizzer?? i files
	file_server = icl_hash_create(50, NULL, NULL);
	pthread_t *tids;

	//inizializzo i dati di output
	out_put.max_file_stored = 0;
	out_put.max_size_reached = 0;
	out_put.replace_algo = 0;

	//creo una struttura dove andr?? a memorizzare i tid dei thread lanciati
	if((tids = malloc(configs.NUM_THREADS * (sizeof(pthread_t)))) == NULL)
		NULL_EXIT(tids = malloc(configs.NUM_THREADS * (sizeof(pthread_t))),"malloc");
	//inizializzo i tids a 0
	for(i=0; i<configs.NUM_THREADS; i++){
		tids[i] = 0;
	}



    int listenfd;
    MINUS_ONE_EXIT(listenfd = socket(AF_UNIX, SOCK_STREAM, 0),"socket");

    struct sockaddr_un serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sun_family = AF_UNIX;    
    strncpy(serv_addr.sun_path, SOCKNAME, strlen(SOCKNAME)+1);

    int notused;
    MINUS_ONE_EXIT(notused = bind(listenfd, (struct sockaddr*)&serv_addr,sizeof(serv_addr)),"bind");
    MINUS_ONE_EXIT(notused = listen(listenfd, MAXBACKLOG),"listen");
    MINUS_ONE_EXIT(pipe(pfd),"pipe");

    fd_set tmpset;
    // azzero sia il master set che il set temporaneo usato per la select
    FD_ZERO(&set);
    FD_ZERO(&tmpset);
	FD_SET(pfd[0], &set);

    // aggiungo il listener fd al master set
    FD_SET(listenfd, &set);

    // tengo traccia del file descriptor con id piu' grande
    int fdmax = listenfd; 

    while(!quit && !stop){
    	tmpset = set;
    	if(!quit){
    		if(!stop){
		    	if (select(fdmax+1, &tmpset, NULL, NULL, NULL) == -1) { 
		    		if(errno == EINTR)
		    			continue;
		    		else{
				    	perror("select");
				   		return -1;
				   	}
				}
			}
		}
		if(quit)
			break;
		if(stop){
			break;
		}


	// cerchiamo di capire da quale fd abbiamo ricevuto una richiesta
		for(int i=0; i <= fdmax; i++) {
		    if (FD_ISSET(i, &tmpset)) {
		    
				long connfd;

				if (i == listenfd) { // e' una nuova richiesta di connessione 


					if((connfd = accept(listenfd, (struct sockaddr*)NULL ,NULL)) == -1)
						MINUS_ONE_EXIT(connfd = accept(listenfd, (struct sockaddr*)NULL ,NULL), "accept");

					FD_SET(connfd, &set);  // aggiungo il descrittore al master set
					if(active_threads < configs.NUM_THREADS){

						if (pthread_create(&tids[active_threads], NULL, worker_t, NULL) != 0) {
							if (pthread_create(&tids[active_threads], NULL, worker_t, NULL) != 0){
						    	fprintf(stderr, "pthread_create failed\n");
		    					exit(EXIT_FAILURE);
		    				}
	    				}
	    				active_threads++;
					}
					if(connfd > fdmax) fdmax = connfd;  // ricalcolo il massimo
				}


				else if(i == pfd[0]){// ?? una scrittura sulla pipe
					int fdfrompipe;
					if((read(pfd[0], &fdfrompipe, sizeof(int))) == -1)
						MINUS_ONE_EXIT(read(pfd[0], &fdfrompipe, sizeof(int)), "readn");

					FD_SET(fdfrompipe, &set);
					if (fdfrompipe > fdmax) 
		 				fdmax = fdfrompipe;
		 			continue;
		 		}
		 		else{
		 			printf("nuova richiesta\n");
					connfd = i;  // e' una nuova richiesta da un client gi?? connesso
				
					// eseguo il comando e se c'e' un errore lo tolgo dal master set
					int try = 0;
					int res;
					FD_CLR(connfd, &set);

					if (connfd == fdmax) 
			 				fdmax = updatemax(set, fdmax);

			 		LOCK(&bufmtx);
			 		
					while ((res = insert_node(connfd,&last,&requests)) < 0 && try < 3)
						try ++;
					if(res < 0){
						UNLOCK(&bufmtx);
						close(connfd); 
			 		}
					else{
						UNLOCK(&bufmtx);
						numreq ++;
						SIGNAL(&bufcond);	//sveglio eventuali thread in attesa di richieste
					}
				}
		    }
		}

    }

    if(stop == 1){
    	LOCK(&bufmtx);
    	for(i = 0; i < active_threads; i ++){
    		if(insert_node(-2, &last, &requests) == -1)
    			if(insert_node(-2, &last, &requests) == -1){

    				exit(1);
    			}
    		numreq ++;

		}

		UNLOCK(&bufmtx);
	}

	SIGNAL(&bufcond);

    for(i = 0; i < active_threads; i++){
    	if (pthread_join(tids[i], NULL) != 0) {
			fprintf(stderr, "pthread_join failed\n");
	    	exit(EXIT_FAILURE);
	    }
	    printf("thread joinato\n");
    }

    free(tids);
    fifoStruct *tmp;
    while(fifoqueue != NULL){
    	tmp = fifoqueue;
    	fifoqueue = fifoqueue->next;
    	free(tmp);
    }

    icl_hash_destroy(file_server, NULL, NULL);
    printf("il numero massimo di file memorizzati nel server ?? stato %d \n",out_put.max_file_stored);
    printf("il numero massimo di spazio utilizzato nel server ?? stato %d \n",out_put.max_size_reached);
    printf("l'algoritmo di rimpiazzamento ?? stato usato %d volte\n",out_put.replace_algo);
    return 0;

}



int config_values_correctly_initialized(config_parameters *cfg){
	if(cfg->NUM_THREADS <= 0 ){
		printf("NUM_THREADS wrongly initialized\n");
		return -1;
	}

	if(cfg->MAX_FILE_NUMBER <= 0 ){
		printf("MAX_FILE_NUMBER wrongly initialized\n");
		return -1;
	}

	if(cfg->SERVER_CAPACITY_MBYTES <= 0 ){
		printf("SERVER_CAPACITY_MBYTES wrongly initialized\n");
		return -1;
	}
	return 1;
}

/*funzione che parsa gli argomenti del config file e ritorna -1
*in caso il valore associato a una variabile non sia un numero oppure
*il numero passato ?? negativo o zero.
*/
int config_file_parser(char *stringa, config_parameters* cfg) {		
	char *tmpstr;													
	char *token = strtok_r(stringa, "=", &tmpstr);

	while (token) {
		if(strncmp("NUM_THREADS",token,12) == 0){
			token = strtok_r(NULL, "=", &tmpstr);
			token[strcspn(token, "\n")] = '\0';
			if(isNumber(token,&(cfg->NUM_THREADS)) != 0)
				return -1;
		}

		else if(strncmp("MAX_FILE_NUMBER",token,16) == 0){
			token = strtok_r(NULL, "=", &tmpstr);
			token[strcspn(token, "\n")] = '\0';
			if(isNumber(token,&(cfg->MAX_FILE_NUMBER)) != 0)
				return -1;
		}
		

		else if(strncmp("SERVER_CAPACITY_MBYTES",token,23) == 0){
			token = strtok_r(NULL, "=", &tmpstr);
			token[strcspn(token, "\n")] = '\0';
			if(isNumber(token,&(cfg->SERVER_CAPACITY_MBYTES)) != 0)
				return -1;
		}
		
	token = strtok_r(NULL, "=", &tmpstr);
	}
	return 1;
}



void *worker_t(void *args){
	while(!quit){
		int answer = 0;
		int error = 0;		//in caso una delle chiamate come readn,writen,malloc ecc. fallisca error = 1 e chiudo la connessione
		int curfd;
		//printf("start\n");
		LOCK(&bufmtx);
		if(quit){
				UNLOCK(&bufmtx);
				return NULL;
			}
		while(numreq == 0){
			WAIT(&bufcond, &bufmtx);
			if(quit){
				UNLOCK(&bufmtx);
				SIGNAL(&bufcond);
				return NULL;
			}
		}
		numreq --;
		if((curfd = remove_node(&requests, &last)) == -1){
			if((curfd = remove_node(&requests, &last)) == -1){
				UNLOCK(&bufmtx);
				exit(EXIT_FAILURE);
			}
		}
		UNLOCK(&bufmtx);
		SIGNAL(&bufcond);
		if(curfd == -2){
			return NULL;
		}
		int codreq = -1;//codice richiesta
		//printf("READN\n");
		if(readn(curfd, &codreq, sizeof(int)) == -1) //non esco in caso di errore, ma chiudo la connessione successivamente
			codreq = -1;

		fileT *tmp;
		switch((int)codreq){
			case OPN:
				opn(OPN,curfd,&error);
				break;

			case OPNC:
				opn(OPNC,curfd,&error);
				break;

			case OPNL:
				opn(OPNL,curfd,&error);
				break;	

			case OPNCL:
				opn(OPNCL,curfd,&error); 
				break;	

			case RD:	
				rd(curfd, &error);
				break;	

			case RDN: 
				rdn(curfd, &error);
				break;	

			case APP:
				app(curfd, &error);
				break;	

			case LO:
				lo(curfd, &error);
				break; 	

			case UN:	
				break;

			case CLS:
				break;

			case RM:
				rm(curfd, &error);
				break;	

			case CLOSE: 
				LOCK(&filesopenedmtx);
				LOCK(&servermtx);
				while((tmp = remove_if_equal(curfd, &filesopened)) != NULL){
					LOCK(&tmp->filemtx);
					tmp->open_create = 0;
					tmp->open_lock = 0;
					UNLOCK(&tmp->filemtx);
				}
				UNLOCK(&servermtx);
				UNLOCK(&filesopenedmtx);

				writen(curfd, &answer, sizeof(int));	//se fallisco non esco tanto non cambia niente per il server
				
				close(curfd);
				break;

			case WRT:
				wrt(curfd, &error);
				break;

			case -2:
				return NULL;//richiesta di chiusura del thread

			default://in caso di fallimento di lettura della richiesta o richiesta non valida chiudo la connessione
				error = 1;
				break;

		}
		if(error == 1){
			close(curfd);
		}
		else if((int)codreq != CLOSE){

			if(write(pfd[1], &curfd, sizeof(int)) == -1){
				if(write(pfd[1], &curfd, sizeof(int)) == -1){
					printf("fail\n");
					close(curfd);
				}
			}

		}
		error = 0;
	}
	return NULL;
}


fileT* opn(int flag, int connfd, int *error){

		size_t dimpath;
	    char* path = NULL;
	    fileT *tmp = NULL;
	    int n;
	    int answer = 0;	//if 0 richiesta andata a buon fine, -1 fallita
        
        n = readn(connfd, &dimpath, sizeof(dimpath));		//leggo la dimensione del nome
        if (n<=0){
        	*error = 1;
        	return NULL;
        }

        path = malloc(dimpath);
        if(!path){
        	*error = 1;
        	return NULL;
        }

        memset(path,'\0',dimpath);

        n = readn(connfd, path, dimpath);						//leggo il nome
        if (n<=0){
        	*error = 1;
        	return NULL;
        }

        if(answer != -1){
        	switch(flag){
        		case OPN:
        			if(icl_hash_find(file_server, path) == NULL)
        				answer = -1;
        			free(path);
        			break;

        		case OPNC:
        			LOCK(&servermtx);
        			if((tmp = icl_hash_find(file_server, path)) == NULL){
        				UNLOCK(&servermtx);
        				free(path);
        				answer = -1;
        			}
	        			    			
	    			else{
	    				LOCK(&tmp->filemtx);
	    				UNLOCK(&servermtx);
	    				if(tmp->open_lock)
	    					answer = -1;
	    				if(tmp->open_create)
	    					answer = -1;
	    				else{
    						tmp->open_create = 1;
	    					tmp->user = connfd;
	    				}
	    				UNLOCK(&tmp->filemtx);
	    				free(path);
	    			}

	    			if(answer != -1){
	    				LOCK(&filesopenedmtx);
	    				insert_listfiles(connfd, &tmp, &filesopened);
	    				UNLOCK(&filesopenedmtx);
	    			}

        			break;

        		case OPNL:
           			LOCK(&servermtx);
        			if((tmp = icl_hash_find(file_server, path)) == NULL){
        				UNLOCK(&servermtx);
        				free(path);
        				answer = -1;
        			}
	        			    			
	    			else{
	    				LOCK(&tmp->filemtx);
	    				UNLOCK(&servermtx);
	    				if(tmp->open_lock)
	    					answer = -1;
	    				if(tmp->open_create)
	    					answer = -1;
	    				else{
    						tmp->open_lock = 1;
	    					tmp->user = connfd;
	    				}
	    				UNLOCK(&tmp->filemtx);
	    			}
    				UNLOCK(&servermtx);


    				if(answer != -1){
	    				LOCK(&filesopenedmtx);
	    				insert_listfiles(connfd, &tmp, &filesopened);
	    				UNLOCK(&filesopenedmtx);
	    			}
        			break;

        		case OPNCL:
        			LOCK(&servermtx);
 
        			if((tmp = icl_hash_find(file_server, path)) == NULL){

	        			if((tmp = icl_hash_insert(file_server, path, NULL)) == NULL){
	        				UNLOCK(&servermtx);
	        				answer = -1;
	        				break;
	        			}
	    				tmp->open_lock = 1;
	    				tmp->open_create = 1;
	    				pthread_mutex_init(&tmp->filemtx,NULL);
	    				int con = connfd;
	    				tmp->user = con;
	    				UNLOCK(&servermtx);
	    			}
	    			else {
	    				UNLOCK(&servermtx);
	    				*error = 1;
	    				answer = -1;	
	    			}

    				if(answer != -1){
	    				LOCK(&filesopenedmtx);
	    				insert_listfiles(connfd, &tmp, &filesopened);
	    				UNLOCK(&filesopenedmtx);
	    			}
        			break;
        	}
   		}

   		if(answer == -1)
   			free(path);
   		if(writen(connfd, &answer, sizeof(int)) == -1){
   			*error = 1;
   			return NULL;
   		}
 
   		return tmp;
}

int wrt(int connfd, int *error){
	int answer = 0;
	size_t answer2 = 0;
	fileT *tmp;
	char *contenuto;
	size_t dim;

	tmp = opn(OPNCL,connfd, error);
	if(*error == 1){
		*error = 0;
		return -1;
	}
	LOCK(&servermtx);
	LOCK(&tmp->filemtx);

	if(readn(connfd, &dim, sizeof(size_t)) == -1){			//leggo dimensione del file
		*error = 1;
		icl_hash_delete(file_server, tmp->key, NULL, NULL);
		UNLOCK(&servermtx);
		return -1;
	}

	if((contenuto = malloc(sizeof(char) * dim)) == NULL){	//alloco dimensione file
		*error = 1;
		icl_hash_delete(file_server, tmp->key, NULL, NULL);
		UNLOCK(&servermtx);
		return -1;
	}

	memset(contenuto,'\0',dim);

	if(readn(connfd, contenuto, dim)== -1){				//leggo contenuto del file
		*error = 1;
		icl_hash_delete(file_server, tmp->key, NULL, NULL);
		free(contenuto);
		UNLOCK(&servermtx);
		return -1;
	}

	//controllo che il file abbia dimensione minore della dimensione massima del server, se cos?? non fosse allora non lo memorizzo e invio risposta negativa al client
	if(dim >= configs.SERVER_CAPACITY_BYTES){
		answer2 = -1;
		icl_hash_delete(file_server, tmp->key, NULL, NULL);
		UNLOCK(&servermtx);
		answer2 = -1;
		answer = -1;
		if(writen(connfd, &answer2, sizeof(size_t)) == -1){	// mando risposta che non ci sono pi?? file rimpiazzati per sbloccare il while nell'api
			*error = 1;
			free(contenuto);
			return -1;
		}
		if(writen(connfd, &answer, sizeof(int)) == -1){	// dico al client che la richiesta ?? fallita
			*error = 1;
			free(contenuto);
			return -1;
		}
		return -1;
	}

	size_t dimfreed;
	LOCK(&serverstatsmtx);

	//se non ho spazio a sufficienza nel server allora applico il rimpiazzamento
	while((configs.MAX_FILE_NUMBER - file_stored) <= 0 || (configs.SERVER_CAPACITY_BYTES - bytes_used) < dim){
		if((dimfreed = fifo_remove(connfd)) == -1){				//il rimpiazzamento pu?? fallire perch?? tutti i file o alcuni sono lockati e non posso toglierli
			answer2 = -1;
			icl_hash_delete(file_server, tmp->key, NULL, NULL);
			if(writen(connfd, &answer2, sizeof(size_t)) == -1){
				*error = 1;
				UNLOCK(&servermtx);
				UNLOCK(&serverstatsmtx);
				return -1;
			}
		}
		else{
			file_stored --;
			bytes_used = bytes_used - dimfreed; 
		}

	}

	answer2 = 0; //mando risposta al server dicendo che non ci sono pi?? file rimpiazzati spediti
	if(writen(connfd, &answer2, sizeof(size_t)) == -1){
		*error = 1;
		UNLOCK(&servermtx);
		UNLOCK(&serverstatsmtx);
		return -1;
	}

	UNLOCK(&serverstatsmtx);
	UNLOCK(&servermtx);

	fifo_insert(tmp);


	//aggiungo la size e il contenuto del file nel server
	if(answer != -1){

	   	if(tmp->open_lock == 1 && tmp->open_create == 1){
	    	if(answer != -1){
	    		tmp->size = dim;
	    		tmp->data = contenuto;
	    	}
	    }
	}

	//aggiorno le stats del server
	LOCK(&serverstatsmtx);
	file_stored ++;
	bytes_used = bytes_used + (long)dim;
	if(bytes_used > out_put.max_size_reached)
		out_put.max_size_reached = bytes_used;
	if(file_stored > out_put.max_file_stored)
		out_put.max_file_stored = file_stored;
	UNLOCK(&serverstatsmtx);

	//mando risposta al client
	if(writen(connfd, &answer2, sizeof(size_t)) == -1){
		*error = 1;
		return -1;
	}
	UNLOCK(&tmp->filemtx);

	//chiudo il file una volta che ci ho scritto
	if(cls(connfd,tmp->key) == -1){
		*error = 1;
		return -1;
	}

	answer = (int)answer2;
	return answer;
}


int rm(int connfd, int *error){

	char* pathname;
	size_t dim;
	int answer = 0;
	fileT *tmp;

	if(readn(connfd, &dim, sizeof(size_t)) == -1){
		*error = 1;
		return -1;
	}

	if((pathname = malloc(sizeof(char) * dim)) == NULL){
		*error = 1;
		return -1;
	}
	memset(pathname,'\0',dim);

	if(readn(connfd, pathname, dim)==-1){
		*error = 1;
		free(pathname);
		return -1;
	}

	LOCK(&servermtx);
	if((tmp = icl_hash_find(file_server, pathname)) != NULL){
		if(tmp->open_lock == 1 && tmp->user == connfd){
			LOCK(&tmp->filemtx);
			dim = tmp->size;
			//devo togliere il file anche dalla coda fifo
			fifo_remove_equal(pathname);

			answer = icl_hash_delete(file_server, pathname, NULL, NULL);

			}
		else{
			UNLOCK(&tmp->filemtx);

			answer = -1;
			}
		}
		else {

			answer = -1;
		}
	LOCK(&serverstatsmtx);
	file_stored --;
	bytes_used = bytes_used - (long)dim;
	UNLOCK(&serverstatsmtx);
	UNLOCK(&servermtx);
	LOCK(&filesopenedmtx);
	remove_if_equalpath(pathname, &filesopened);
	UNLOCK(&filesopenedmtx);

	if(writen(connfd, &answer, sizeof(int)) == -1){
		*error = 1;
		free(pathname);
		return -1;
	}
	free(pathname);
	return answer;
	}


int cls(int connfd,char* pathname){
	int answer = 0;
	fileT *tmp;
	LOCK(&servermtx);
	if((tmp = icl_hash_find(file_server, pathname)) != NULL){
		if(connfd == tmp->user){

			LOCK(&tmp->filemtx);
			UNLOCK(&servermtx);
			tmp->open_create = 0;
			tmp->open_lock = 0;
			LOCK(&filesopenedmtx);
			if(remove_if_equalpath(pathname, &filesopened) == NULL)
				answer = -1;
			UNLOCK(&filesopenedmtx);
			UNLOCK(&tmp->filemtx);
		}
	}
	else {
		UNLOCK(&tmp->filemtx);
		answer = -1;
	}

	return answer;
}

int rd(int connfd, int *error){
	char* pathname;
	size_t dim;
	fileT *tmp;
	int answer = 0;
	if(readn(connfd, &dim, sizeof(size_t)) == -1){		//leggo dimensione del pathname
		*error = 1;
		return -1;
	}

	if((pathname = malloc(sizeof(char) * dim)) == NULL){		//alloco spazio pathname
		*error = 1;
		return -1;
	}

	memset(pathname,'\0',dim);

	if(readn(connfd, pathname, dim) == -1){			//leggo pathname
		*error = 1;
		return -1;
	}

	LOCK(&servermtx);
	if((tmp = icl_hash_find(file_server, pathname)) == NULL){ //non trovo il file
		answer = -1;
		UNLOCK(&servermtx);
	}
	else{
		if(tmp->open_create){	//non posso leggerlo perch?? il file ?? in fase di scrittura
			answer = -1;
			UNLOCK(&servermtx);
		}
		else{
			LOCK(&tmp->filemtx);
			UNLOCK(&servermtx);
		}
	}

	free(pathname);
	
	size_t dimfile;
	if(answer != -1)
		dimfile = tmp->size;
	else
		dimfile = -1;


	if(writen(connfd, &dimfile, sizeof(size_t)) == -1){	//mando la size del file, in caso il file non esista mando -1
		*error = 1;
		UNLOCK(&tmp->filemtx);
		return -1;
	}

	if(answer == -1){
		return -1;
	}

	if(writen(connfd, tmp->data, tmp->size) == -1){	//mando il file
		*error = 1;
		UNLOCK(&tmp->filemtx);
		return -1;
	}
	UNLOCK(&tmp->filemtx);
	return answer;

}

int lo(int connfd, int *error){
	char* pathname;
	size_t dim;
	fileT *tmp;
	int answer = 0;
	if(readn(connfd, &dim, sizeof(size_t)) == -1){		//leggo dimensione del pathname
		*error = 1;
		return -1;
	}


	if((pathname = malloc(sizeof(char) * dim)) == NULL){		//alloco spazio pathname
		*error = 1;
		return -1;
	}

	memset(pathname,'\0',dim);

	if(readn(connfd, pathname, dim) == -1){			//leggo pathname
		*error = 1;
		return -1;
	}

	LOCK(&servermtx);
	if((tmp = icl_hash_find(file_server, pathname)) == NULL){
		answer = -1;
		free(pathname);
		UNLOCK(&servermtx);
	}
	else{
		LOCK(&tmp->filemtx);
		UNLOCK(&servermtx);
		if(tmp->open_lock == 0 && tmp->open_create == 0){
			tmp->open_lock = 1;
			tmp->user = connfd;
		}
		else if(tmp->open_lock == 1){
			if(tmp->user == connfd)
				answer = 1;
		}

		else
			answer = -1;
		UNLOCK(&tmp->filemtx);
	}
	if(writen(connfd, &answer, sizeof(int)) == -1){
		*error = 1;
		free(pathname);
		return -1;
	}
	if(answer == -1)
		return answer;
	free(pathname);
	return answer;
}

int rdn(int connfd, int *error){

	int numfile;
	if(readn(connfd, &numfile, sizeof(int)) == -1){
		*error = 1;
		return -1;
	}




	size_t filesize = 0;
	LOCK(&servermtx);
	LOCK(&fifoqueuemtx);

	fifoStruct *tmp = fifoqueue;
	while(numfile != 0){
		if(tmp == NULL)
			break;
		LOCK(&tmp->fileInServer->filemtx);
		//non voglio inviare un file che deve essere ancora scritto
		if(tmp->fileInServer->open_create != 1){
			filesize = tmp->fileInServer->size;

			if(writen(connfd, &filesize, sizeof(size_t)) == -1){
				UNLOCK(&tmp->fileInServer->filemtx);
				UNLOCK(&fifoqueuemtx);
				UNLOCK(&servermtx);
				*error = 1;
				return -1;
			}

			if(writen(connfd, tmp->fileInServer->data, filesize) == -1){
				UNLOCK(&tmp->fileInServer->filemtx);
				UNLOCK(&fifoqueuemtx);
				UNLOCK(&servermtx);
				*error = 1;
				return -1;
			}
			numfile --;
		}
		UNLOCK(&tmp->fileInServer->filemtx);
		tmp=tmp->next;
	}

	UNLOCK(&fifoqueuemtx);
	UNLOCK(&servermtx);

	filesize = 0;										//invio un messaggio di fine di file da spedire
	return writen(connfd, &filesize, sizeof(size_t)); //non mi interessa se fallisce, il server non avr?? ripercussioni

}


int fifo_insert(fileT *fileToInsert){
	LOCK(&fifoqueuemtx);
	fifoStruct *new = malloc(sizeof(fifoStruct));
	if(!new){
		perror("malloc");
		return -1;
	}
	new->fileInServer = fileToInsert;
	new->next = NULL;
	if(fifoqueueLast == NULL){
		fifoqueueLast = new;
		fifoqueue = new;
		UNLOCK(&fifoqueuemtx);
		return 0;
	}
	fifoqueueLast->next = new;
	fifoqueueLast = new;
	UNLOCK(&fifoqueuemtx);
	return 0;
}

int fifo_remove(int connfd){
	out_put.replace_algo ++;
	size_t DimFreed = 0;
	fifoStruct *tmp = NULL;
	fifoStruct *curr = fifoqueue;
	int retval = 0;

	LOCK(&fifoqueuemtx);

	while(curr != NULL ){
		if(curr->fileInServer->open_lock == 0 && curr->fileInServer->open_create == 0){

			LOCK(&curr->fileInServer->filemtx);
			DimFreed = curr->fileInServer->size;
			if(tmp != NULL){
				tmp->next = curr->next;
			}
			else
				fifoqueue = fifoqueue->next;

			if(fifoqueue == NULL)
				fifoqueueLast = NULL;


			if(writen(connfd, &DimFreed, sizeof(size_t)) == -1){
				UNLOCK(&curr->fileInServer->filemtx);
				UNLOCK(&fifoqueuemtx);			//mando dim del file
				return -1;
			}

			if(writen(connfd, curr->fileInServer->data, DimFreed) == -1){
				UNLOCK(&curr->fileInServer->filemtx);
				UNLOCK(&fifoqueuemtx);			//mando il file
				return -1;
			}


			retval = icl_hash_delete(file_server, curr->fileInServer->key, NULL, NULL);
			if(retval == -1){
				UNLOCK(&curr->fileInServer->filemtx);
				UNLOCK(&fifoqueuemtx);
				return -1;
			}


			free(curr);
			break;
		}

		tmp = curr;
		curr = curr->next;
	}

	UNLOCK(&fifoqueuemtx);

	return DimFreed;
}

int fifo_remove_equal(char *pathname){
	fifoStruct *tmp = NULL;
	fifoStruct *curr = fifoqueue;
	LOCK(&fifoqueuemtx);
	while(curr != NULL ){
		if(strcmp(curr->fileInServer->key, pathname) == 0 ){
			if(tmp != NULL){
				tmp->next = curr->next;
			}
			else
				fifoqueue = fifoqueue->next;
			free(curr);
			break;
		}
		tmp = curr;
		curr = curr->next;
	}
	UNLOCK(&fifoqueuemtx);
	return 0;
}



int app(int connfd, int *error){
	int answer = 0;
	size_t answer2 = 0;
	fileT *tmp;
	char *contenuto;
	size_t dim;

	tmp = opn(OPNC,connfd, error);
	if(*error == 1){
		printf("error %d\n",*error);
		*error = 0;
		return -1;
	}

	LOCK(&servermtx);
	LOCK(&tmp->filemtx);
	UNLOCK(&servermtx);

	if(readn(connfd, &dim, sizeof(size_t)) == -1){			//leggo dimensione dell'append
		*error = 1;
		UNLOCK(&tmp->filemtx);
		return -1;
	}

	if((contenuto = malloc(sizeof(char) * dim)) == NULL){	//alloco dimensione dell'append
		*error = 1;
		UNLOCK(&tmp->filemtx);
		return -1;
	}

	memset(contenuto,'\0',dim);

	if(readn(connfd, (void*)contenuto, dim)== -1){				//leggo contenuto del dell'append
		*error = 1;
		free(contenuto);
		UNLOCK(&tmp->filemtx);
		return -1;
	}

	//controllo che il file abbia dimensione minore della dimensione massima del server, se cos?? non fosse allora non lo memorizzo e invio risposta negativa al client
	if(dim >= configs.SERVER_CAPACITY_BYTES){
		answer2 = -1;
		answer = -1;
		UNLOCK(&tmp->filemtx);
		if(writen(connfd, &answer2, sizeof(size_t)) == -1){	// mando risposta che non ci sono pi?? file rimpiazzati per sbloccare il while nell'api
			*error = 1;
			free(contenuto);
			return -1;
		}
		if(writen(connfd, &answer, sizeof(int)) == -1){	// dico al client che la richiesta ?? fallita
			*error = 1;
			free(contenuto);
			return -1;
		}
		return -1;
	}


	size_t dimfreed;
	//se non ho spazio a sufficienza nel server allora applico il rimpiazzamento
	LOCK(&servermtx);
	LOCK(&serverstatsmtx);

	while((configs.SERVER_CAPACITY_BYTES - bytes_used) < dim){
		if((dimfreed = fifo_remove(connfd)) == -1){				//il rimpiazzamento pu?? fallire perch?? tutti i file o alcuni sono lockati e non posso toglierli
			answer2 = -1;
			answer = -1;
			UNLOCK(&tmp->filemtx);
			UNLOCK(&servermtx);
			UNLOCK(&serverstatsmtx);
			if(writen(connfd, &answer2, sizeof(size_t)) == -1){
				*error = 1;
				free(contenuto);
				return -1;
			}
			if(writen(connfd, &answer, sizeof(int)) == -1){	// dico al client che la richiesta ?? fallita
				*error = 1;
				free(contenuto);
				return -1;
			}
			free(contenuto);
			return -1;
		}
		else{
			bytes_used = bytes_used - dimfreed; 
		}

	}

	UNLOCK(&serverstatsmtx);
	UNLOCK(&servermtx);

	answer2 = 0; //mando risposta al server dicendo che non ci sono pi?? file rimpiazzati spediti
	if(writen(connfd, &answer2, sizeof(size_t)) == -1){
		*error = 1;
		free(contenuto);
		UNLOCK(&tmp->filemtx);
		return -1;
	}


	//aggiungo la size e il contenuto del file nel server
	if(answer != -1){
	   	tmp->data = realloc(tmp->data, tmp->size+dim);
	   	char *memtmp = (char*)tmp->data;
	   	memcpy((void*)&memtmp[tmp->size], contenuto, dim);
		//memcpy(&(tmp->data[tmp->size]), contenuto, dim);
	   	tmp->size = tmp->size + dim;
	}

	//aggiorno le stats del server
	if(answer != -1){
		LOCK(&serverstatsmtx);
		bytes_used = bytes_used + (long)dim;
		if(bytes_used > out_put.max_size_reached)
			out_put.max_size_reached = bytes_used;
		UNLOCK(&serverstatsmtx);
	}


	//mando risposta al client
	if(writen(connfd, &answer, sizeof(int)) == -1){
		*error = 1;
		free(contenuto);
		UNLOCK(&tmp->filemtx);
		return -1;
	}
	UNLOCK(&tmp->filemtx);

	//chiudo il file una volta che ci ho scritto
	if(cls(connfd,tmp->key) == -1){
		*error = 1;
		free(contenuto);
		return -1;
	}

	answer = (int)answer2;
	free(contenuto);
	return answer;
}
