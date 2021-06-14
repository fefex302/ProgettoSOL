#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdarg.h>
#include <dirent.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>

#include <pwd.h>
#include <grp.h>
#include <stddef.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "api.h"
#include "utils.h"

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

#define MINUS_ONE_RETURN(X,str)\
  if ((X)==-1) {			\
    perror(#str);			\
    return -1;			\
  }

#define MINUS_ONE_RETURN_NULL(X,str)\
  if ((X)==-1) {			\
    perror(#str);			\
    return NULL;			\
  }

static int openwrite = 0;
static int client_fd;
static int connection_active = 0;
static struct sockaddr_un server_address;
int print_flag = 0;

//*************************************FUNZIONI STATICHE DI UTILITÀ***************************************************************
static char* read_file(int fd_file,const char *file_path, size_t *size){		//legge un file e restituisce nel campo file_return 
																				//una stringa contenente tutti i bit del file
	if((fd_file = open(file_path,O_RDONLY)) == -1)								
		return NULL;															
	
	struct stat buf;
	MINUS_ONE_RETURN_NULL(fstat(fd_file, &buf),"fstat");

	*size = buf.st_size;
	char *file_return;

	if((file_return = malloc(buf.st_size+1)) == NULL)
		return NULL;
	
	if(readn(fd_file, file_return, buf.st_size+1) == -1)
		return NULL;
	close(fd_file);
	return file_return;

}


static int open_file(const char *file_to_read,char *path_to_dir){		//legge il file contenuto nella stringa e se una cartella
	if(path_to_dir == NULL)									//di salvataggio è settata allora crea un file in quella cartella
		return 0;		

	size_t size = strlen(file_to_read);

	int fd_file;

	if((fd_file = open(path_to_dir, O_CREAT|O_WRONLY, 0666)) == -1)
		return -1;

	return writen(fd_file,(void * )file_to_read,size);

}
//*************************************FUNZIONI DELL'API********************************************************************************


int openConnection( const char* sockname, int msec,const struct timespec abstime ){

    if(!sockname || (msec < 0)){
        errno = EINVAL;
        return -1;
    }

    if((client_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
        return -1;

    memset(&server_address, '0', sizeof(server_address));
    server_address.sun_family = AF_UNIX;
    strncpy(server_address.sun_path, sockname, strlen(sockname)+1);

    struct timespec sleeptime;
    sleeptime.tv_sec = 0;
    sleeptime.tv_nsec = msec * 1000000;
    struct timespec time_request;
    time_request.tv_sec = 0;
    time_request.tv_nsec = msec * 1000000;

    struct timespec current_time;
    memset(&current_time, '0', sizeof(current_time));

    do{
        if(connect(client_fd, (struct sockaddr*) &server_address, sizeof(server_address)) != -1){
        	connection_active = 1;
            return 0;
        }

        nanosleep(&sleeptime, &time_request);

        if(clock_gettime(CLOCK_REALTIME, &current_time) == -1){
        	//close(client_fd);
            return -1;
        }

    }while(current_time.tv_sec < abstime.tv_sec ||
        current_time.tv_nsec < abstime.tv_nsec);

    return -1;
}

//EINVAL: sto passando un sockname nullo
//EFAULT: sto chiudendo una connessione inesistente
int closeConnection( const char* sockname ){
    if(!sockname){
        errno = EINVAL;
        return -1;
    }

    if(strncmp(server_address.sun_path, sockname, strlen(sockname)+1) ==  0){
    	int req = CLOSE;
    	if(writen(client_fd, &req, sizeof(int)) == -1)
    		return -1;
    	if(readn(client_fd, &req, sizeof(int)) == -1)
    		return -1;
        return close(client_fd);
    }else{
        errno = EFAULT;
        return -1;
    }
}

//EPERM: connessione non attiva
//EINVAL: sto passando un pathname nullo

int openFile(const char* pathname, int flags){

	if(!connection_active){
		errno = EPERM;
		printf("eperm\n");
		return -1;
	}

	if(pathname == NULL){
		errno = EINVAL;
		printf("einval\n");
		return -1;
	}

	int request;
	if(flags == 0){
		if(print_flag)
			printf("richiesta di apertura in sola lettura del file <%s>\n",pathname);
		request = OPN;
	}
	else if(flags == O_CREATE){
		if(print_flag)
			printf("richiesta di apertura in modalità creazione del file <%s>\n",pathname);
		request = OPNC;
	}
	else if(flags == O_LOCK){
		if(print_flag)
			printf("richiesta di apertura in modalità locked del file <%s>\n",pathname);
		request = OPNL;
	}
	else if(flags == O_LOCK_CREATE){
		if(print_flag)
			printf("richiesta di apertura in modalità locked e creazione del file <%s>\n",pathname);
		request = OPNCL;
	}

	int answer;

	if(openwrite == 0){
		if(writen(client_fd, &request, sizeof(request)) == -1)		//mando il tipo di richiesta
			return -1;
	}
	else 
		openwrite = 0;

	size_t lenght = strlen(pathname)+1;
	if(writen(client_fd, &lenght, sizeof(size_t)) == -1)		//mando la lunghezza del pathname
		return -1;

	if(writen(client_fd, (void * )pathname, lenght) == -1)				//mando il pathname
		return -1;

	if(readn(client_fd, &answer, sizeof(answer)) == -1)		//ricevo la risposta dal server
		return -1;

	if(answer == -1){
		errno = ECANCELED;
		return -1;
	}

	if(print_flag)
			printf("la richiesta di apertura del file <%s> ha avuto successo\n",pathname);
	return 0;


}

int writeFile(const char* pathname, const char*dirname){
	int request = WRT;
	openwrite = 1;
	int retval = 0;
	int fd_file;
	char* file_to_send = NULL;
	size_t size;

	if((file_to_send = read_file(fd_file, pathname, &size)) == NULL){
		if(print_flag)
			printf("file <%s> inesistente\n",pathname);
		return -1;
	}

	if(writen(client_fd, &request, sizeof(request)) == -1)		//mando il tipo di richiesta
		return -1;

	if((retval = openFile(pathname,O_LOCK_CREATE)) == -1){
		if(print_flag)
			printf("richiesta di apertura in modalità creazione e lock del file <%s> fallita\n",pathname);
		errno = ECANCELED;
		return -1;
	}

	if(writen(client_fd, &size, sizeof(size)) == -1)			//mando la lunghezza del file
		return -1;

	if(writen(client_fd, file_to_send, size) == -1)				//mando il file
		return -1;
	free(file_to_send);
	int answer;

	int stop = 0;
	while(!stop){
		if(readn(client_fd, &answer, sizeof(answer)) == -1)		//ricevo la risposta dal server
			return -1;
		if(answer == 0)
			stop = 1;
	}

	if(readn(client_fd, &answer, sizeof(answer)) == -1)		//ricevo la risposta dal server
		return -1;

	if(answer == -1){
		if(print_flag)
			printf("la richiesta di scrittura del file <%s> è fallita\n",pathname);
		errno = ECANCELED;
		return -1;
	}

	//protocollo di risposta del server: se va tutto bene ritorno il numero di file
	//rimpiazzati nel server per fare spazio al file spedito, che può essere un numero che
	//va da 0 in poi, quindi ciclerò un numero di volte pari al valore di risposta e memorizzerò
	//i file nella cartella indicata se questa è diversa da NULL
	if(dirname != NULL){			
		printf("ancora da fare\n");
	}

	if(print_flag){
		printf("la richiesta di scrittura del file <%s> ha avuto successo\n",pathname);
		printf("bytes scritti: %zu\n",size);
	}
	return retval;

}



int readFile(const char* pathname, void** buf, size_t*size){
	int request = RD;
	if(print_flag)
			printf("richiesta di lettura del file <%s>\n",pathname);

	if(writen(client_fd, &request, sizeof(request)) == -1){		//mando il tipo di richiesta
		errno = ECANCELED;
		return -1;
	}
	int lenght = strlen(pathname)+1;
	if(writen(client_fd, &lenght, sizeof(int)) == -1){		//mando la lunghezza del pathname
		errno = ECANCELED;
		return -1;
	}
	if(writen(client_fd, (void * )pathname, lenght) == -1){				//mando il pathname
		errno = ECANCELED;
		return -1;
	}

	size_t answer = -1;
	if(readn(client_fd, &answer, sizeof(answer)) == -1){		//ricevo la risposta dal server che equivale al size del file
		errno = ECANCELED;
		return -1;
	}
	if(answer == -1){
		errno = ECANCELED;
		return -1;
	}

	*buf = malloc(answer);
		if(!*buf){
			errno = ENOMEM;
			return -1;
		}
	memset(*buf,'\0',answer);
	int read;
	if((read = readn(client_fd, *buf, answer)) == -1){		//scrivo il file mandato dal server nel buffer
		errno = ECANCELED;
		return -1;
	}
	*size = answer;
	if(print_flag)
			printf("la richiesta di lettura del file <%s> ha avuto successo\n",pathname);
	return 0;
}


int removeFile(const char* pathname){

	int request = RM;
	if(print_flag)
			printf("richiesta di rimozione del file <%s>\n",pathname);

	if(writen(client_fd, &request, sizeof(request)) == -1){		//mando il tipo di richiesta
		errno = ECANCELED;
		return -1;
	}

	size_t lenght = strlen(pathname)+1;
	printf("len %d\n",lenght);
	if(writen(client_fd,&lenght, sizeof(size_t)) == -1){		//mando la lunghezza del pathname
		errno = ECANCELED;
		return -1;
	}
	if(writen(client_fd, (void * )pathname, lenght) == -1){				//mando il pathname
		errno = ECANCELED;
		return -1;
	}

	int answer = 1;
	if(readn(client_fd, &answer, sizeof(answer)) == -1){		//ricevo la risposta dal server 
		errno = ECANCELED;
		return -1;
	}
	if(answer == -1){
		errno = ECANCELED;
		return -1;
	}
	if(print_flag)
			printf("la richiesta di rimozione del file <%s> ha avuto successo\n",pathname);
	return 0;
}


int lockFile(const char* pathname){
	int request = LO;
	if(print_flag)
			printf("richiesta di applicazione della LOCK sul file <%s>\n",pathname);

	if(writen(client_fd, &request, sizeof(request)) == -1){		//mando il tipo di richiesta
		errno = ECANCELED;
		return -1;
	}

	size_t lenght = strlen(pathname)+1;
	printf("len %d\n",lenght);
	if(writen(client_fd, &lenght, sizeof(size_t)) == -1){		//mando la lunghezza del pathname
		errno = ECANCELED;
		return -1;
	}
	if(writen(client_fd, (void * )pathname, lenght) == -1){				//mando il pathname
		errno = ECANCELED;
		return -1;
	}

	int answer = 1;
	if(readn(client_fd, &answer, sizeof(answer)) == -1){		//ricevo la risposta dal server 
		return -1;
	}
	if(answer == -1){
		errno = ECANCELED;
		return -1;
	}
	if(print_flag)
			printf("la richiesta di applicazione della LOCK sul file <%s> ha avuto successo\n",pathname);
	return 0;
}
