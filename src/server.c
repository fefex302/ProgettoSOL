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

//***********************************VARIABILI GLOBALI*************************************************************

static config_parameters configs;		//parametri di configurazione passati dal file config.txt
static hashtable *file_server;			//hashtable dove andrò a memorizzare tutti i file
static list *requests = NULL;			//lista fifo delle richieste dei client
static list *last = NULL;				//puntatore all'ultimo elemento della queue
static int empty = 1;					//variabile che indica se il buffer delle richieste è vuoto o meno
static int quit = 0;					//variabile che dice se bisogna uscire dal programma
static pthread_mutex_t	bufmtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t	bufcond = PTHREAD_COND_INITIALIZER;
static int pfd[2];
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

    fd_set set, tmpset;
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
	// cerchiamo di capire da quale fd abbiamo ricevuto una richiesta
		for(int i=0; i <= fdmax; i++) {
		    if (FD_ISSET(i, &tmpset)) {
				long connfd;
				if (i == listenfd) { // e' una nuova richiesta di connessione 
					MINUS_ONE_EXIT(accept(listenfd, (struct sockaddr*)NULL ,NULL), "accept");
					FD_SET(connfd, &set);  // aggiungo il descrittore al master set
					if(active_threads < configs.NUM_THREADS){
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
				while ((res = insert_node(connfd,last)) < 0 && try < 3)
					try ++;
				if(res < 0){
					close(connfd); 
					FD_CLR(connfd, &set); 
		 			// controllo se deve aggiornare il massimo
		 			if (connfd == fdmax) 
		 				fdmax = updatemax(set, fdmax);
				}
				else{
					empty = 0;
					SIGNAL(&bufcond);	//sveglio eventuali thread in attesa di richieste
				}
		    }
		}
    }

    free(tids);

    return 0;

}









/*       
	long connfd;
	connfd = accept(listenfd, (struct sockaddr*)NULL ,NULL);
	printf("connection accepted\n");    	

	do {
        int dimpath;
        size_t dimfile;
        char* path = NULL;
        char* file_client = NULL;
        int n;
        int re;
        int answer = 0;

        n = readn(connfd, &re, sizeof(int));
        if (n<=0) break;

        printf("richiesta: %d\n",re);
        
        n = readn(connfd, &dimpath, sizeof(dimpath));
        if (n<=0) break;

        printf("dimpath: %zu\n",dimpath);

        path = malloc(dimpath);
        n = readn(connfd, path, dimpath);
        if (n<=0) break;

        printf("path: %s\n", path);
         writen(connfd, &answer, sizeof(answer));
        n = readn(connfd, &dimfile, sizeof(dimfile));
        if (n<=0) break;

        printf("dimfile: %zu\n",dimfile);

        file_client = malloc(dimfile);

        n = readn(connfd, file_client, dimfile);
        if (n<=0) break;
  
  
        writen(connfd, &answer, sizeof(answer));
		fileT *toput = malloc(sizeof(fileT));

        toput->key = (void *)path;
        toput->data = (void *)file_client;
        toput->size = dimfile;
        toput->next = NULL;
        free(toput->key);
        free(toput->data);
        free(toput);
        close(connfd);
        
    } while(1);

    //close(connfd);

	return 0;
}*/


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
		while(empty){
			WAIT(&bufcond,&bufmtx);
		}


	

	}
}
