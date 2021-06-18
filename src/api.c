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
static int number = 0;
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


static int save_file(const char *fileToStore,char *dirname, size_t filesize){		//legge il file contenuto nella stringa e se una cartella
	if(dirname == NULL)												//di salvataggio è settata allora crea un file in quella cartella
		return 0;
	int len = strlen(dirname) + 1;		
	int fd_file;
	char str[10];
			
	char namefile[40] = {'f','i','l','e','E','s','p'};
	if(sprintf(str, "%d", number) <0){
		return -1;
	}

	strncat(namefile,str,strlen(str));

	char* dirfile = malloc(len+strlen(namefile));
	if(!dirfile){
		free(dirfile);
		return -1;
	}
	memset(dirfile,'\0',len);

	strncpy(dirfile,dirname,len);

	strncat(dirfile, namefile, strlen(namefile));
		

	if((fd_file = open(dirfile, O_CREAT|O_WRONLY, 0666)) == -1){
		perror("open");
		free(dirfile);
		return -1;
	}

	if(writen(fd_file, (void*)fileToStore, filesize) == -1){
		perror("writen");
		free(dirfile);
		close(fd_file);
		return -1;
	}
	close(fd_file);
	free(dirfile);	
	return 0;
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
    errno = ETIMEDOUT;
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
		if(writen(client_fd, &request, sizeof(request)) == -1){		//mando il tipo di richiesta
			errno = ECANCELED;
			return -1;
		}
	}
	else 
		openwrite = 0;

	size_t lenght = strlen(pathname)+1;
	if(writen(client_fd, &lenght, sizeof(size_t)) == -1){		//mando la lunghezza del pathname
		errno = ECANCELED;
		return -1;
	}
	if(writen(client_fd, (void * )pathname, lenght) == -1){				//mando il pathname
		errno = ECANCELED;
		return -1;
	}
	if(readn(client_fd, &answer, sizeof(answer)) == -1){		//ricevo la risposta dal server
		errno = ECANCELED;
		return -1;
	}

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
		errno = EINVAL;
		return -1;
	}

	if(writen(client_fd, &request, sizeof(request)) == -1){		//mando il tipo di richiesta
		errno = ECANCELED;
		return -1;
	}
	if((retval = openFile(pathname,O_LOCK_CREATE)) == -1){
		if(print_flag)
			printf("richiesta di apertura in modalità creazione e lock del file <%s> fallita\n",pathname);
		errno = ECANCELED;
		free(file_to_send);
		return -1;
	}

	if(writen(client_fd, &size, sizeof(size)) == -1){			//mando la lunghezza del file
		errno = ECANCELED;
		free(file_to_send);
		return -1;
	}

	
	if(writen(client_fd, file_to_send, size) == -1){
		errno = ECANCELED;										//mando il file
		free(file_to_send);
		return -1;
	}

	free(file_to_send);
	size_t answer;
	char *fileRimpiazzato = NULL;
	int stop = 0;
	
	//protocollo di risposta del server: se va tutto bene ritorno la size dei file
	//rimpiazzati nel server per fare spazio al file spedito, che può essere un numero che
	//va da 1 in poi, quindi ciclerò finchè non leggo un valore 0 o -1(per l'errore) e memorizzerò
	//i file nella cartella indicata se questa è diversa da NULL
	while(!stop){
		if(readn(client_fd, &answer, sizeof(answer)) == -1){		//ricevo la risposta dal server, che se = 0 significa che non ho più file rimpiazzati ricevuti
			errno = ECANCELED;
			return -1;
		}
		if(answer == 0 || answer == -1){
			stop = 1;
			break;
		}

		fileRimpiazzato = malloc(answer);
		if(!fileRimpiazzato)
			return -1;
		if(readn(client_fd, fileRimpiazzato, answer) == -1){		
			errno = ECANCELED;
			return -1;
		}

		//funzione che salva il file nella cartella dirname
		save_file(fileRimpiazzato, (char*)dirname, answer);
		free(fileRimpiazzato);
		number++;
	}

	if(readn(client_fd, &answer, sizeof(size_t)) == -1){		//ricevo la risposta dal server
		errno = ECANCELED;
		return -1;
	}

	if(answer == -1){
		if(print_flag)
			printf("la richiesta di scrittura del file <%s> è fallita\n",pathname);
		errno = ECANCELED;
		return -1;
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
	size_t lenght = strlen(pathname)+1;
	if(writen(client_fd, &lenght, sizeof(size_t)) == -1){		//mando la lunghezza del pathname
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


int readNFiles(int N, const char* dirname){
	int request = RDN;

	if(writen(client_fd, &request, sizeof(request)) == -1){		//mando il tipo di richiesta
		errno = ECANCELED;

		return -1;
	}
	int numerofiles = N;

	if(writen(client_fd, &numerofiles, sizeof(numerofiles)) == -1){		//mando il numero di file qualsiasi da leggere
		errno = ECANCELED;
		return -1;
	}
	int n = 0;
	char *file_to_read = NULL;

	int len = 0;
	if(dirname != NULL)
		len = strlen(dirname) + 1;

	size_t dimfile = 1;
	while(dimfile > 0){
		if(readn(client_fd, &dimfile, sizeof(size_t)) == -1){		//ricevo la size del file dal server
			errno = ECANCELED;
			return -1;
		}
		if(dimfile > 0){
			file_to_read = malloc(dimfile);
			memset(file_to_read, '\0', dimfile);

			if(readn(client_fd, file_to_read, dimfile) == -1){		//ricevo il file dal server
				errno = ECANCELED;
				return -1;
			}
			if(dirname != NULL){
				int fd_file;
				char str[10];
				char namefile[40] = {'f','i','l','e'};
				if(sprintf(str, "%d", n) <0){
					free(file_to_read);
					continue;
				}

				strncat(namefile,str,strlen(str));

				char* dirfile = malloc(len+strlen(namefile));
				if(!dirfile){
					free(file_to_read);
					free(dirfile);
					continue;
				}
				memset(dirfile,'\0',len);

				strncpy(dirfile,dirname,len);

				strncat(dirfile, namefile, strlen(namefile));
				n++;

				if((fd_file = open(dirfile, O_CREAT|O_WRONLY, 0666)) == -1){
					perror("open");
					free(dirfile);
					free(file_to_read);
					continue;
				}

				if(writen(fd_file, file_to_read, dimfile) == -1){
					perror("writen");
					free(dirfile);
					close(fd_file);
					free(file_to_read);
					continue;
				}	
				close(fd_file);
				free(dirfile);
				free(file_to_read);
			}
		}
	}

	return 0;

}

int appendToFile(const char* pathname, void* buf, size_t size, const char* dirname){
	int request = APP;
	int answer = 0;
	size_t answer2 = 0;
	if(print_flag)
			printf("richiesta di append per il file <%s>\n",pathname);

	if(writen(client_fd, &request, sizeof(request)) == -1){		//mando il tipo di richiesta
		errno = ECANCELED;

		return -1;
	}

	size_t lenght = strlen(pathname)+1;

	if(writen(client_fd, &lenght, sizeof(size_t)) == -1){		//mando la lunghezza del pathname
		errno = ECANCELED;
		return -1;
	}

	if(writen(client_fd, (void * )pathname, lenght) == -1){				//mando il pathname
		errno = ECANCELED;
		return -1;
	}

	if(readn(client_fd, &answer, sizeof(int)) == -1){				//leggo la risposta, sarà 0 se il file è stato aperto in modalìtà create
		errno = ECANCELED;
		return -1;
	}

	if(answer != 0){
		if(print_flag)
			printf("non è stato possibile aprire il file <%s>, il file non esiste o non può essere modificato, richiesta di append fallita\n",pathname);
		errno = ECANCELED;
		return -1;
	}

	if(writen(client_fd, &size, sizeof(size_t)) == -1){				//mando il size dell'append
		errno = ECANCELED;
		return -1;
	}

	if(writen(client_fd, buf, size) == -1){								//mando l'append								
		errno = ECANCELED;
		return -1;
	}


	char *fileRimpiazzato = NULL;
	int stop = 0;
	while(!stop){
		if(readn(client_fd, &answer2, sizeof(size_t)) == -1){		//ricevo la risposta dal server, che se = 0 significa che non ho più file rimpiazzati ricevuti
			errno = ECANCELED;
			return -1;
		}

		if(answer == 0 || answer == -1){
			stop = 1;
			break;
		}

		fileRimpiazzato = malloc(answer);
		if(!fileRimpiazzato){
			errno = ENOMEM;
			return -1;
		}
		if(readn(client_fd, fileRimpiazzato, answer) == -1){		
			errno = ECANCELED;
			return -1;
		}

		save_file(fileRimpiazzato, (char*)dirname, answer);

		free(fileRimpiazzato);
		number ++;
	}

	if(readn(client_fd, &answer, sizeof(int)) == -1){		//ricevo la risposta dell'esito dell'append
			errno = ECANCELED;
			return -1;
	}

	if(answer == 0){
		if(print_flag)
			printf("la richiesta di append per il file <%s> ha avuto successo\n",pathname);
	}
	else{
		if(print_flag)
			printf("la richiesta di append per il file <%s> è fallita\n",pathname);
	}

	return answer;

}

