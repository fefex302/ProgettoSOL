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

#include <pwd.h>
#include <grp.h>
#include <stddef.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "api.h"

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

static int client_fd;
static struct sockaddr_un server_address;


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
        if(connect(client_fd, (struct sockaddr*) &server_address, sizeof(server_address)) != -1)
            return 0;

        nanosleep(&sleeptime, &time_request);

        if(clock_gettime(CLOCK_REALTIME, &current_time) == -1){
        	//close(client_fd);
            return -1;
        }

    }while(current_time.tv_sec < abstime.tv_sec ||
        current_time.tv_nsec < abstime.tv_nsec);

    return -1;
}


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

static int read_file(int fd_file, char *file_path, char *file_return){		//legge un file e restituisce nel campo file_return 
	fd_file = open(file_path,O_RDONLY);								//una stringa contenente tutti i bit del file
	struct stat buf;
	MINUS_ONE_EXIT(fstat(fd_file, &buf),"fstat");

	size_t size = buf.st_size;

	NULL_EXIT(file_return = malloc(size),"malloc");
	
	return readn(fd_file,file_return,size);
}


static int open_file(char *file_to_read,char *path_to_dir){		//legge il file contenuto nella stringa e se una cartella
	if(path_to_dir == NULL)									//di salvataggio è settata allora crea un file in quella cartella
		return 0;		

	size_t size = strlen(file_to_read);

	int fd_file;

	fd_file = open(path_to_dir, O_CREAT|O_WRONLY, 0666);

	return writen(fd_file,file_to_read,size);

}
