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

#define OPN 	1;	//open
#define OPNC 	2;	//open create mode
#define OPL 	3;	//open lock mode
#define OPNL 	4;	//open create and lock mode
#define RD 		5;	
#define RDN 	4;
#define APP 	5;
#define LO 		6;
#define UN 		7;
#define CLS 	8;
#define RM 		9;

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

static int client_fd;
static int connection_active = 0;
static struct sockaddr_un server_address;
size_t gvar = 0;

//*************************************FUNZIONI STATICHE DI UTILITÀ***************************************************************
static int read_file(int fd_file,const char *file_path, char *file_return, size_t *size){		//legge un file e restituisce nel campo file_return 
	fd_file = open(file_path,O_RDONLY);															//una stringa contenente tutti i bit del file
	
	struct stat buf;
	MINUS_ONE_RETURN(fstat(fd_file, &buf),"fstat");

	*size = buf.st_size;

	file_return = malloc(buf.st_size);
	
	return readn(fd_file, file_return, buf.st_size);
}


static int open_file(const char *file_to_read,char *path_to_dir){		//legge il file contenuto nella stringa e se una cartella
	if(path_to_dir == NULL)									//di salvataggio è settata allora crea un file in quella cartella
		return 0;		

	size_t size = strlen(file_to_read);

	int fd_file;

	fd_file = open(path_to_dir, O_CREAT|O_WRONLY, 0666);

	return writen(fd_file,file_to_read,size);

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

	if(flags == O_CREAT){
		request = OPNC;
		printf("request\n");
		if(writen(client_fd, &request, sizeof(request)) == -1)		//mando il tipo di richiesta
			return -1;

		int lenght = strlen(pathname)+1;
		if(writen(client_fd, &lenght, sizeof(int)) == -1)		//mando la lunghezza del pathname
			return -1;

		if(writen(client_fd, pathname, lenght) == -1)				//mando il pathname
			return -1;

		int fd_file;
		char* file_to_send;
		size_t size;

		if(read_file(fd_file, pathname, file_to_send, &size) == -1)
			return -1;

		printf("dim file client %s\n",strlen(file_to_send));
		if(writen(client_fd, &size, sizeof(size)) == -1)			//mando la lunghezza del file
			return -1;


		if(writen(client_fd, file_to_send, size) == -1)				//mando il file
			return -1;

		gvar = size;
		return 0;

	}
}
