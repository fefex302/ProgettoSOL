#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <fcntl.h>

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
    if (pthread_mutex_lock(l)!=0){		\
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

void cleanup() {
    unlink(SOCKNAME);
}

// ritorno l'indice massimo tra i descrittori attivi
int updatemax(fd_set set, int fdmax) {
    for(int i=(fdmax-1);i>=0;--i)
	if (FD_ISSET(i, &set)) return i;
    
    return -1;
}


int config_file_parser(char *stringa, config_parameters* cfg);
int config_values_correctly_initialized(config_parameters *cfg);
void *worker_t(void *args);
int wrt(int connfd, int *error);
fileT* opn(int flag,int connfd, int *error);
int cls(char* pathname);
int rm(int connfd, int *error);

//***********************************VARIABILI GLOBALI*************************************************************
static output_info out_put;				//dati da stampare a schermo all'uscita del programma
static config_parameters configs;		//parametri di configurazione passati dal file config.txt
static list *requests = NULL;			//lista fifo delle richieste dei client
static list *last = NULL;				//puntatore all'ultimo elemento della queue
static int numreq = 0;					//variabile che indica se il buffer delle richieste è vuoto o meno
static int quit = 0;					//variabile che dice se bisogna uscire dal programma
static pthread_mutex_t	bufmtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t	servermtx = PTHREAD_MUTEX_INITIALIZER;
static hashtable *file_server;			//hashtable dove andrò a memorizzare tutti i file
static pthread_cond_t	bufcond = PTHREAD_COND_INITIALIZER;
static int pfd[2];
static fd_set set;
static pthread_mutex_t	serverstatsmtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t	filesopenedmtx = PTHREAD_MUTEX_INITIALIZER;
static long file_slots;
static long free_bytes;
static listfiles *filesopened = NULL;
//***********************************MAIN****************************************************************************

int main(int argc,char *argv[]){
	cleanup();
	if(argc != 2){
		printf("you must use a config file\n");
		return EXIT_FAILURE;
	}

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

	if(config_values_correctly_initialized(&configs) != 1)
		exit(EXIT_FAILURE);

	fclose(config_file);

	file_slots = configs.MAX_FILE_NUMBER;
	free_bytes = configs.SERVER_CAPACITY_MBYTES * 1000000;
	configs.SERVER_CAPACITY_BYTES = free_bytes;
	file_server = icl_hash_create(50, NULL, NULL);
	pthread_t *tids;
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
  	printf("listenfd %d\n",listenfd);

    while(!quit){
    	tmpset = set;
    	if (select(fdmax+1, &tmpset, NULL, NULL, NULL) == -1) { // attenzione al +1
	    	perror("select");
	   		return -1;
		}
		printf("qua1\n");
	// cerchiamo di capire da quale fd abbiamo ricevuto una richiesta
		for(int i=0; i <= fdmax; i++) {
		    if (FD_ISSET(i, &tmpset)) {
		    	printf("i %d\n",i);
		    
				long connfd;
				printf("qua2\n");
				if (i == listenfd) { // e' una nuova richiesta di connessione 
					printf("qua3\n");

					if((connfd = accept(listenfd, (struct sockaddr*)NULL ,NULL)) == -1)
						MINUS_ONE_EXIT(connfd = accept(listenfd, (struct sockaddr*)NULL ,NULL), "accept");

					FD_SET(connfd, &set);  // aggiungo il descrittore al master set
					if(active_threads < configs.NUM_THREADS){
						printf("qua4\n");
						if (pthread_create(&tids[active_threads], NULL, worker_t, NULL) != 0) {
							if (pthread_create(&tids[active_threads], NULL, worker_t, NULL) != 0){
						    	fprintf(stderr, "pthread_create failed\n");
		    					exit(EXIT_FAILURE);
		    				}
	    				}
	    				else
	    					active_threads++;
					}
					if(connfd > fdmax) fdmax = connfd;  // ricalcolo il massimo
				}


				else if(i == pfd[0]){// è una scrittura sulla pipe
					int fdfrompipe;
					printf("maxfd %d\n",fdmax);
					if((read(pfd[0], &fdfrompipe, sizeof(int))) == -1)
						MINUS_ONE_EXIT(read(pfd[0], &fdfrompipe, sizeof(int)), "readn");

					printf("fd %d\n",fdfrompipe);
					FD_SET(fdfrompipe, &set);
					if (fdfrompipe > fdmax) 
		 				fdmax = fdfrompipe;
		 			printf("maxfd %d\n",fdmax);
		 			continue;
		 		}
		 		else{
		 			printf("nuova richiesta\n");
					connfd = i;  // e' una nuova richiesta da un client già connesso
				
					// eseguo il comando e se c'e' un errore lo tolgo dal master set
					int try = 0;
					int res;
					FD_CLR(connfd, &set);

					if (connfd == fdmax) 
			 				fdmax = updatemax(set, fdmax);

			 		LOCK(&bufmtx);
			 		printf("connfd: %d\n",connfd);
					while ((res = insert_node(connfd,&last,&requests)) < 0 && try < 3)
						try ++;
					UNLOCK(&bufmtx);
					if(res < 0){
						close(connfd); 
			 		}
					else{
						numreq ++;
						SIGNAL(&bufcond);	//sveglio eventuali thread in attesa di richieste
					}
				}
		    }
		}

    }
    int status;

    for(i = 0; i < active_threads; i++){
    	if (pthread_join(tids[i], &status) == -1) {
			fprintf(stderr, "pthread_join failed\n");
	    	exit(EXIT_FAILURE);
	    }
    }
    free(tids);

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
*il numero passato è negativo o zero.
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
		LOCK(&bufmtx);
		while(numreq == 0){
			WAIT(&bufcond, &bufmtx);
		}
		numreq --;
		int answer = 0;
		int error = 0;		//in caso una delle chiamate come readn,writen,malloc ecc. fallisca error = 1 e chiudo la connessione
		int curfd;
		if((curfd = remove_node(&requests, &last)) == -1)
			MINUS_ONE_EXIT(curfd = remove_node(&requests, &last),"remove_node");

		UNLOCK(&bufmtx);
		int codreq = -1;//codice richiesta

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
				rd(curfd);
				break;	
			case RDN: 
				break;	
			case APP:
				break;	
			case LO:
				break; 		
			case UN:	
				break;	
			case CLS:
				break;
			case RM:
				answer = rm(curfd, &error);
				break;	
			case CLOSE: 
				LOCK(&filesopenedmtx);
				while((tmp = remove_if_equal(curfd, &filesopened)) != NULL){
					tmp->open_create = 0;
					tmp->open_lock = 0;
				}
				UNLOCK(&filesopenedmtx);
				writen(curfd, &answer, sizeof(int));	//se fallisco non esco tanto non cambia niente per il server
				close(curfd);
				break;
			case WRT:
				wrt(curfd, &error);
				break;
			default://in caso di fallimento di lettura della richiesta o richiesta non valida chiudo la connessione
				close(curfd);
				break;

		}
		if(error == 1){
			close(curfd);
		}
		else if((int)codreq != CLOSE){
			printf("ret: %d\n",curfd);
			if(write(pfd[1], &curfd, sizeof(int)) == -1){
				if(write(pfd[1], &curfd, sizeof(int)) == -1){
					printf("fail\n");
					close(curfd);
				}
			}
			printf("ret: %d\n",curfd);
		}
		error = 0;
	}
	return NULL;
}



fileT* opn(int flag, int connfd, int *error){

		int dimpath;
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
        
        n = readn(connfd, path, dimpath);						//leggo il nome
        if (n<=0){
        	*error = 1;
        	return NULL;
        }

        if(answer != -1){
        	switch(flag){
        		case OPN:
        			if(icl_hash_find(file_server, path) == NULL);
        				answer = -1;
        			break;

        		case OPNC:
        			LOCK(&servermtx);
        			if((tmp = icl_hash_find(file_server, path)) == NULL)
        				answer = -1;
	        			    			
	    			else{
	    				if(tmp->open_lock)
	    					answer = -1;
	    				if(tmp->open_create)
	    					answer = -1;
	    				else{
    						tmp->open_create = 1;
	    					tmp->user = connfd;
	    				}
	    			}
    				UNLOCK(&servermtx);
    				if(answer != -1){
	    				LOCK(&filesopenedmtx);
	    				insert_listfiles(connfd, &tmp, &filesopened);
	    				UNLOCK(&filesopenedmtx);
	    			}
        			break;

        		case OPNL:
           			LOCK(&servermtx);
        			if((tmp = icl_hash_find(file_server, path)) == NULL)
        				answer = -1;
	        			    			
	    			else{
	    				if(tmp->open_lock)
	    					answer = -1;
	    				if(tmp->open_create)
	    					answer = -1;
	    				else{
    						tmp->open_lock = 1;
	    					tmp->user = connfd;
	    				}
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
        			if((tmp = icl_hash_find(file_server, path)) == NULL && file_slots != 0){
	        			if((tmp = icl_hash_insert(file_server, path, NULL)) == NULL)
	    					answer = -1;
	    				tmp->open_lock = 1;
	    				tmp->open_create = 1;
	    				int con = connfd;
	    				tmp->user = con;
	    			}
	    			else 
    					answer = -1;
    				UNLOCK(&servermtx);


    				if(answer != -1){
	    				LOCK(&filesopenedmtx);
	    				insert_listfiles(connfd, &tmp, &filesopened);
	    				UNLOCK(&filesopenedmtx);
	    			}
        			break;
        	}
   		}
   		if(writen(connfd, &answer, sizeof(int)) == -1){
   			*error = 1;
   			return NULL;
   		}

   		if (tmp != NULL && answer == -1)
   			return NULL;
   		else 
   			return tmp;
}

int wrt(int connfd, int *error){
	int answer = 0;
	fileT *tmp;
	char *contenuto;
	size_t dim;

	tmp = opn(OPNCL,connfd, error);

	if(*error == 1){
		*error = 1;
		return -1;
	}

	if(readn(connfd, &dim, sizeof(size_t)) == -1){
		*error = 1;
		return -1;
	}

	if((contenuto = malloc(sizeof(char) * dim)) == NULL){
		*error = 1;
		return -1;
	}

	if(readn(connfd, contenuto, dim)== -1){
		*error = 1;
		return -1;
	}

	if(dim >= configs.SERVER_CAPACITY_BYTES)
		answer = -1;

	//if(file_slots == 0){
		//fifo_replace(-1);
	//}
	char *path = tmp->key;

	if(answer != -1){
		printf("path: %s\n",path);
		printf("path: %d\n",strlen(path));
	   	if(tmp->open_lock == 1 && tmp->open_create == 1){
	    	//if(dim > free_bytes)
	    		//answer = fifo_replace(dim);
	    	if(answer != -1){
	    		tmp->size = dim;
	    		tmp->data = contenuto;
	    	}
	    }
	}
	//printf("answer: %d\n",answer);
	LOCK(&serverstatsmtx);
	file_slots--;
	free_bytes = free_bytes - (long)dim;
	UNLOCK(&serverstatsmtx);
	printf("esco\n");
	if(writen(connfd, &answer, sizeof(int)) == -1){
		*error = 1;
		return -1;
	}

	if(cls(tmp->key) == -1){
		*error = 1;
		return -1;
	}
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

	if(readn(connfd, pathname, dim)==-1){
		*error = 1;
		return -1;
	}
	

	LOCK(&servermtx);
		if((tmp = icl_hash_find(file_server, pathname)) != NULL){
			if(tmp->open_lock == 1 && tmp->user == connfd){
				answer = icl_hash_delete(file_server, pathname, NULL, NULL);
			}
			else
				answer = -1;
		}
		else 
			answer = -1;
	UNLOCK(&servermtx);

	if(writen(connfd, &answer, sizeof(int)) == -1){
		*error = 1;
		return -1;
	}
	return answer;
}

//int fifo_replace

int cls(char* pathname){
	int answer = 0;
	fileT *tmp;
	LOCK(&servermtx);
	if((tmp = icl_hash_find(file_server, pathname)) != NULL){
		tmp->open_create = 0;
		tmp->open_lock = 0;
	}
	else 
		answer = -1;
	UNLOCK(&servermtx);

	LOCK(&filesopenedmtx);
	if(remove_if_equalpath(pathname, &filesopened) == NULL)
		answer = -1;
	UNLOCK(&filesopenedmtx);
	return answer;
}

int rd(int connfd, int *error){
	char* pathname;
	int dim;
	fileT *tmp;
	int answer = 0;
	if(readn(connfd, &dim, sizeof(int)) == -1){		//leggo dimensione del pathname
		*error = 1;
		return -1;
	}

	if((pathname = malloc(sizeof(char) * dim)) == NULL){		//alloco spazio pathname
		*error = 1;
		return -1;
	}

	if(readn(connfd, pathname, dim) == -1){			//leggo pathname
		*error = 1;
		return -1;
	}

	LOCK(&servermtx);
	if((tmp = icl_hash_find(file_server, pathname)) == NULL)
		answer = -1;
	UNLOCK(&servermtx);

	int dimfile;
	if(answer != -1)
		dimfile = strlen(tmp->data) + 1;

	if(writen(connfd, &answer, sizeof(int)) == -1){	//mando la size del file, in caso il file non esista mando -1
		*error = 1;
		return -1;
	}

	if(answer == -1)
		return -1;


	if(writen(connfd, tmp->data, dimfile) == -1){	//mando la size del file, in caso il file non esista mando -1
		*error = 1;
		return -1;
	}

	return answer;

}
