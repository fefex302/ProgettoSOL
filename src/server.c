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

#define LOCK(l)      if (pthread_mutex_lock(l)!=0)        { \
    fprintf(stderr, "ERRORE FATALE lock\n");		    \
    pthread_exit((void*)EXIT_FAILURE);			    \
  }   

#define UNLOCK(l)    if (pthread_mutex_unlock(l)!=0)      { \
  fprintf(stderr, "ERRORE FATALE unlock\n");		    \
  pthread_exit((void*)EXIT_FAILURE);				    \
  }

#define WAIT(c,l)    if (pthread_cond_wait(c,l)!=0)       { \
    fprintf(stderr, "ERRORE FATALE wait\n");		    \
    pthread_exit((void*)EXIT_FAILURE);				    \
}

#define SIGNAL(c)    if (pthread_cond_signal(c)!=0)       {	\
    fprintf(stderr, "ERRORE FATALE signal\n");			\
    pthread_exit((void*)EXIT_FAILURE);					\
  }

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

typedef struct _clientfiles{
	int *fdclient;
	char *filesopened;
} clientfiles;

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
int wrt(int connfd);
fileT* opn(int flag,int connfd);
//***********************************VARIABILI GLOBALI*************************************************************
static output_info out_put;
static config_parameters configs;		//parametri di configurazione passati dal file config.txt
static hashtable *file_server;			//hashtable dove andrò a memorizzare tutti i file
static list *requests = NULL;			//lista fifo delle richieste dei client
static list *last = NULL;				//puntatore all'ultimo elemento della queue
static int numreq = 0;					//variabile che indica se il buffer delle richieste è vuoto o meno
static int quit = 0;					//variabile che dice se bisogna uscire dal programma
static pthread_mutex_t	bufmtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t	servermtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t	bufcond = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t	fdsetmtx = PTHREAD_MUTEX_INITIALIZER;
static int pfd[2];
static fd_set set;
static long file_slots;
static long free_bytes;
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
	list *last = NULL;
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

	pthread_t *tids = malloc(configs.NUM_THREADS * (sizeof(pthread_t)));
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

    fd_set tmpset;
    // azzero sia il master set che il set temporaneo usato per la select
    FD_ZERO(&set);
    FD_ZERO(&tmpset);

    // aggiungo il listener fd al master set
    FD_SET(listenfd, &set);

    // tengo traccia del file descriptor con id piu' grande
    int fdmax = listenfd; 
    MINUS_ONE_EXIT(pipe(pfd),"pipe");

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
				long connfd;
				printf("qua2\n");
				if (i == listenfd) { // e' una nuova richiesta di connessione 
					printf("qua3\n");
					MINUS_ONE_EXIT(connfd = accept(listenfd, (struct sockaddr*)NULL ,NULL), "accept");
					FD_SET(connfd, &set);  // aggiungo il descrittore al master set
					if(active_threads < configs.NUM_THREADS){
						printf("qua4\n");
						if (pthread_create(&tids[active_threads], NULL, worker_t, NULL) != 0) {
					    	fprintf(stderr, "pthread_create failed\n");
	    					exit(EXIT_FAILURE);
	    				}
	    				else
	    					active_threads++;
					}
					if(connfd > fdmax) fdmax = connfd;  // ricalcolo il massimo
						continue;
				} 
				connfd = i;  // e' una nuova richiesta da un client già connesso
			
				// eseguo il comando e se c'e' un errore lo tolgo dal master set
				int try = 0;
				int res;
				FD_CLR(connfd, &set);
				if (connfd == fdmax) 
		 				fdmax = updatemax(set, fdmax);
				while ((res = insert_node(connfd,&last,&requests)) < 0 && try < 3)
					try ++;
				if(res < 0){
					close(connfd); 
					// controllo se deve aggiornare il massimo
		 		}
				else{
					numreq ++;
					SIGNAL(&bufcond);	//sveglio eventuali thread in attesa di richieste
				}
		    }
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
		list *reqst = remove_node(&requests);
		
		UNLOCK(&bufmtx);
		int codreq = -1;//codice richiesta

		readn(reqst->fd, &codreq, sizeof(int));

		if((int)codreq == CLOSE){
			close(reqst->fd);
			return NULL;
			}
		fileT *tmp;
		switch((int)codreq){
			case OPN:
				opn(OPN,reqst->fd);
				break;
			case OPNC:
				opn(OPNC,reqst->fd);
				break;
			case OPNL:
				opn(OPNL,reqst->fd);
				break;		
			case OPNCL:
				opn(OPNCL,reqst->fd); 
				break;		
			case RD:	
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
				break;	
			case CLOSE: 
				close(reqst->fd);
				break;
			case WRT:
				wrt(reqst->fd);
				break;
			default://in caso di fallimento di lettura della richiesta o richiesta non valida chiudo la connessione
				close(reqst->fd);
				break;

		}

		

	}
	return NULL;

}


fileT* opn(int flag, int connfd){

		int dimpath;
	    char* path = NULL;
	    fileT *tmp = NULL;
	    int n;
	    int answer = 0;	//if 0 richiesta andata a buon fine, -1 fallita
        
        n = readn(connfd, &dimpath, sizeof(dimpath));		//leggo la dimensione del nome
        if (n<=0) 
        	answer = -1;

        path = malloc(dimpath);
        if(!path)
        	answer = -1;
        
        n = readn(connfd, path, dimpath);						//leggo il nome
        if (n<=0) 
        	answer = -1;

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
        			break;
        	}
   		}
   		writen(connfd, &answer, sizeof(int));
   		return tmp;
}

int wrt(int connfd){
	int answer = 0;
	fileT *tmp;
	char *contenuto;
	size_t dim;
	//printf("answer: %d\n",answer);
	if((tmp = opn(OPNCL,connfd)) == NULL)
		answer = -1;
	//printf("answer: %d\n",answer);

	if(readn(connfd, &dim, sizeof(size_t)) == -1)
		answer = -1;
	//printf("answer: %d\n",answer);

	if((contenuto = malloc(sizeof(char) * dim)) == NULL)
		answer = -1;
	//printf("answer: %d\n",answer);

	if(readn(connfd, contenuto, dim) == -1)
		answer = -1;
	//printf("answer: %d\n",answer);

	if(dim >= configs.SERVER_CAPACITY_BYTES)
		answer = -1;

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
	writen(connfd, &answer, sizeof(int));
	return tmp;
}


