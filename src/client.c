#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>

#include <assert.h>
#include "api.h"
#include "utils.h"

#define BUF_LEN 100
#define BUF_LEN_2 25
#define MSEC 10

static int flag_f = 0;	//variabile che identifica se la connessione col socket è stata effettuata o meno

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
int parse_arguments(char *buf,char *buf2[]);
int help();
int f_req(char* args);

int main(int argc,char* argv[]){

	int opt;
	int n;
	char buffer[BUF_LEN];
	char *string_buffer[BUF_LEN_2];
	string_buffer[0] = "client_requests";
	for(int i =0; i<BUF_LEN_2; i++)
		string_buffer[i] = NULL;
	int end = 0;	//usato per terminare il client
	int restart = 0;	//usato per ricominciare il parse degli argomenti quando leggo qualcosa di sbagliato
	//int h,f,w,W,r,R,d,t,l,o;

	int flag_p = 0;

	while(!end){
		restart = 0;
		memset(buffer,'\0',sizeof(char));
		if(fgets(buffer,BUF_LEN,stdin) == NULL)
			continue;

		n = parse_arguments(buffer,string_buffer);
		printf("n = %d\n",n);

		/*for(int i = 0; i<BUF_LEN_2; i++){
			if(string_buffer[i] == NULL)
				printf("%d = NULL\n",i);
			else 
				printf("%d = %s\n",i,string_buffer[i]);
		}*/
		int d=0,r=0,R=0;

		for(int i = 1; i<BUF_LEN_2; i++){
			if(string_buffer[i] == NULL)
				break;
			else if(strncmp(string_buffer[i],"-d",3) == 0)
				d = i;
			else if(strcmp(string_buffer[i],"-r") == 0)
				r = i;
			else if(strcmp(string_buffer[i],"-R") == 0)
				R = i;
			printf("d = %d,r = %d, R = %d\n",d,r,R);
		}
		if(d>0){
			if(r>0)
				printf("ok\n");
			if(R>0)
				printf("ok\n");
			else{
				printf("-d needs at least one -r or -R\n");
				restart = 1;
			}
		}
		optind = 0;
		opterr = 0;

		while ((opt = getopt(n,string_buffer, ":hf:w:W:r:R:d:t:l:u:c:p")) != -1 && !end && !restart) {
		    switch(opt) {
				case 'h':  
					help();
					end = 1;
					break;
				case 'f':
					if(flag_p)
						printf("tentativo di connessione con il socket %s\n",optarg);
					if(!flag_f){
						if(f_req(optarg)== 0)
							flag_f = 1;
						else{
							printf("connessione con il server fallita\n");
							restart = 1;
						}
					}
					else{
						printf("connessione con il server già effettuata\n");
					}
					break;
				case 'w': //arg_o(optarg);  
					break;
				case 'W': //arg_h(argv[0]); 
					break;
				case 'r': //arg_h(argv[0]); 
					break;
				case 'R': //arg_h(argv[0]);
					printf("R con argomenti\n");
					break;
				case 'd': //arg_h(argv[0]); 
					break;
				case 't': //arg_h(argv[0]); 
					break;
				case 'l': //arg_h(argv[0]); 
					break;
				case 'u': //arg_h(argv[0]); 
					break;
				case 'c': //arg_h(argv[0]); 
					break;
				case 'p': //arg_h(argv[0]); 
					flag_p = 1;
					break;
				case ':': 
					switch(optopt){
						case 'R':
							printf("ciaoooooo\n");
							break;
						case 't':
							printf("t senza argomenti\n");
							break;
						default:
			   				printf("l'opzione '-%c' richiede un argomento\n", optopt);
				    } break;
				case '?': {  // restituito se getopt trova una opzione non riconosciuta
				      printf("l'opzione '-%c' non e' gestita\n", optopt);
				    } break;
				default:;
    		}
  		}
  		for(int i=1; i<n; i++){
  				if(string_buffer[i]){
                free(string_buffer[i]);
                string_buffer[i] = NULL;
         	}
        }
	}
  return 0;




}


int parse_arguments(char *buf,char *buf2[]){

	char *tmpstr;													
	char *token = strtok_r(buf, " ", &tmpstr);
	int i = 1;

	while (token) {
		buf2[i] = calloc(strlen(token)+1,sizeof(char));

		if(buf2[i] == NULL)	//controllo manuale dato che mi dava warning con la funzione NULL_EXIT
			exit(1);

		memset(buf2[i],'\0',sizeof(char));
    	//buf2[i] = token;
    	strncpy(buf2[i],token,strlen(token)+1);
    	token = strtok_r(NULL, " ", &tmpstr);
    	i++;
    }
    buf2[i-1][strcspn(buf2[i-1], "\n")] = '\0';			//setto l'ultimo carattere della stringa a \0
    int n = i;
    //buf2[BUF_LEN_2-1] = NULL;
    while(i<BUF_LEN_2){		//setto tutte le stringhe non inizializzate a NULL dato che potrebbero contenere comandi 
    	buf2[i] = NULL;			//usati in precedenza
    	i++;
    }

    return n;
}


int help(){
	printf("%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
			"Opzioni accettate dal client: ",
			"-h",
			"-f filename",
			"-wdirname[,n=0]",
			"-W file1[,file2]",
			"-r file1[,file2]",
			"-R[n=0]",
			"-d dirname",
			"-t time",
			"-l file1[,file2]",
			"-u file1[,file2]",
			"-c file1[,file2]",
			"-p");
	return 1;

}

int f_req(char* args){
	struct timespec abstime;
	MINUS_ONE_EXIT(clock_gettime(CLOCK_REALTIME,&abstime),"clock_gettime");
	abstime.tv_sec += 10;
	
	char *s = malloc(strlen(args)+1);
	strncpy(s,args,strlen(args)+1);
	printf("%s\n",s);
	if(openConnection(s,MSEC,abstime) == -1){
		free(s);
		return -1;
	}

	else return 0;
}


int disconnect(char *args){
	return closeConnection(args);
}

int read_file(int fd_file, char *file_path, char *file_return){		//legge un file e restituisce nel campo file_return 
	fd_file = open(file_path,O_RDONLY);								//una stringa contenente tutti i bit del file
	struct stat buf;
	MINUS_ONE_EXIT(fstat(fd_file, &buf),"fstat");

	size_t size = buf.st_size;

	NULL_EXIT(file_return = malloc(size),"malloc");
	
	return readn(fd_file,file_return,size);
}


int open_file(char *file_to_read,char *path_to_dir){		//legge il file contenuto nella stringa e se una cartella
	if(path_to_dir == NULL)									//di salvataggio è settata allora crea un file in quella cartella
		return 0;		

	size_t size = strlen(file_to_read);

	int fd_file;

	fd_file = open(path_to_dir, O_CREAT|O_WRONLY, 0666);

	return writen(esempio2,file_return,size);

}
