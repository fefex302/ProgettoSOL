#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <assert.h>
#include <libgen.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/stat.h>

#include "api.h"
#include "utils.h"

#define BUF_LEN 100
#define BUF_LEN_2 25
#define MSEC 10
#define MAX_FILE_NAME 2048

extern int print_flag;	//variabile che identifica se la stampa delle operazioni è abilitata

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
int disconnect(char *args);
int W_req(char *args,char *dirname);
int r_req(char *args,char *dirname);
int w_parse(char *args, char *wdirname, long *nFileToSend);
int w_req( const char* nomedir, long* n );

typedef struct _arg_list{
    char* arg;
    struct _arg_list* next;
} arg_list;

int isdot(const char dir[]) {
  int l = strlen(dir);

  if ( (l>0 && dir[l-1] == '.') ) return 1;
  return 0;
}

//*******************************************MAIN*****************************************************************************
int main(int argc,char* argv[]){

	if(argc == 1){
		printf("usare almeno un argomento, lanciare con opzione -h per vedere le richieste possibili\n");
		return 0;
	}

	int opt;
	int end = 0;	//usato per terminare il client

	char *sockname = NULL;
	char *read_dirname = NULL;	//cartella dove memorizzare i file letti dal server	
	char *dirname = NULL;
	int d=0,r=0,R=0,h=0,f=0,p=0;

	for(int i = 1; i<argc; i++){
		if(strncmp(argv[i],"-d",3) == 0){
			d = i;
			printf("indice %d\n",d);
		}
		else if(strcmp(argv[i],"-r") == 0)
			r = i;
		else if(strcmp(argv[i],"-R") == 0)
			R = i;
		else if(strcmp(argv[i],"-h") == 0)
			h = i;
		else if(strcmp(argv[i],"-f") == 0)
			f = i;
		else if(strcmp(argv[i],"-p") == 0)
			p = i;
	}
	if(h > 0){
		help();
		return 0;
	}

	if(f > 0){
		f++;
		printf("tentativo di connessione con il socket << %s >>\n",argv[f]);
		if(f_req(argv[f]) != 0){
			printf("connessione con il socket << %s >> fallita\n",argv[f]);
			return 0;
		}
		else{
			printf("connessione con il socket << %s >> stabilita\n",argv[f]);
			sockname = argv[f];
		}
	}
	else{
		printf("prima di svolgere operazioni è necessario stabilire prima una connessione con il server\nusare -f nomesocket\n");
		return 0;
	}


	if(d>0){
		if(r>0){
			d++;
			read_dirname = argv[d];
		}
		else if(R>0){
			d++;
			read_dirname = argv[d];
		}
		else{
			printf("-d ha bisogno di almeno uno tra -r o -R\n");
			end = 1;
		}
	}
	printf("read dirname %s\n",read_dirname);
	//abilito le stampe delle operazioni
	if(p>0){
		print_flag = 1;
	}
		
	opterr = 0;
	long nFileToSend = -1;
	char wdirname[MAX_FILE_NAME];
	memset(wdirname,'\0',MAX_FILE_NAME);

	while ((opt = getopt(argc,argv, ":hf:w:W:r:R:d:t:l:u:c:p")) != -1 && !end) {
		switch(opt) {
			case 'h':  
				break;
			case 'f':
				break;
			case 'w':
				if(w_parse(optarg,wdirname,&nFileToSend) == 0){
					w_req(wdirname,&nFileToSend);
				}
				else {
					printf("-w: errore nel parsing degli argomenti\n");
				}
				break;
			case 'W': 
				W_req(optarg,dirname);
				break;
			case 'r':  
				r_req(optarg,read_dirname);
				break;
			case 'R': 
				printf("R con argomenti\n");
				break;
			case 'd': 
				break;
			case 't':  
				break;
			case 'l':  
				break;
			case 'u':  
				break;
			case 'c':  
				break;
			case 'p': 
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
    	sleep(1);
  	}
  	sleep(1);
	if(sockname != NULL)
		disconnect(sockname);
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

int w_parse(char *args, char *wdirname, long *nFileToSend){
	int i = 0;
	while(args[i] != '\0' && args[i] != ',')
		i++;
	if(args[i] != '\0'){
		strncpy(wdirname, args, i);
		printf("DIRNAME %s\n",wdirname);
		char aux[11];
		memset(aux,'\0',11);
		strncpy(aux, args+i, 11);
		printf("AUX %s\n",aux);
		if(aux[0] != 'n' && aux[1]!= '=')
			return -1;
		else {
			if(isNumber(aux+2, nFileToSend) != 0)
				return -1;
		}
	}
	else{
		strncpy(wdirname, args, i+1);
		printf("DIRNAME %s\n",wdirname);
	}

	return 0;
	
}

int w_req( const char* nomedir, long* n ){

    // controllo se il parametro sia una directory
    struct stat statbuf;
    memset(&statbuf, '0', sizeof(statbuf));
    MINUS_ONE_EXIT(stat(nomedir, &statbuf), "stat");

    DIR *dir;
    if((dir = opendir(nomedir)) == NULL){
        perror("opendir");
        return -1;
    }else{
        struct dirent* file;

        while(*n != 0 && (errno = 0, file = readdir(dir)) != NULL){
            struct stat statbuf;
            char filename[MAX_FILE_NAME];
            int len1 = strlen(nomedir);
            int len2 = strlen(file->d_name);
            if((len1 + len2 + 2) > MAX_FILE_NAME){
                fprintf(stderr, "ERROR: MAX_FILE_NAME too small : %d\n", MAX_FILE_NAME);
                return -1;
            }
            strncpy(filename, nomedir, MAX_FILE_NAME-1);
            strncat(filename, "/", MAX_FILE_NAME-1);
            strncat(filename, file->d_name, MAX_FILE_NAME-1);
fprintf(stdout, "Nome del file che sto esaminando: '%s'\n", filename);
            if(stat(filename, &statbuf)==-1) {
              perror("eseguendo la stat");
              fprintf(stderr, "ERROR: Error in file %s\n", filename);
              return -1;
            }

            if(S_ISDIR(statbuf.st_mode)){
                      if ( !isdot(filename) ){
fprintf(stdout, "L'elemento che sto per visitare è una directory di nome '%s'\n", filename);
                          int res = w_req(filename,n);
                          if(res == -1) return -1;
                      }
            }else{
            	printf("scrivo il file\n");
                *n = *n - 1;
               W_req(filename,NULL);
           }
        }
        if (errno != 0) perror("readdir");
        closedir(dir);
        return 0;
    }
}

int help(){
	printf("%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
			"Opzioni accettate dal client: ",
			"-h",
			"-f filename",
			"-w dirname[,n=0]",
			"-W file1[,file2]",
			"-r file1[,file2]",
			"-R [n=0]",
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
	if(!s)
		s = malloc(strlen(args)+1);
	if(!s)
		exit(EXIT_FAILURE);
	memset(s,'\0',strlen(args)+1);

	strncpy(s,args,strlen(args)+1);
	if(openConnection(s,MSEC,abstime) == -1){
		free(s);
		return -1;
	}
	else{
		free(s);
		return 0;
	}
}


int disconnect(char *args){
	if(print_flag)
		printf("richiesta di chiusura della connessione\n");
	if(closeConnection(args) != -1){
		if(print_flag)
			printf("connessione chiusa\n");
		return 0;
	}
	else {
		if(print_flag)
			printf("richiesta di chiusura della connessione fallita\n");
		return -1;
	}

}

int W_req(char *args,char *dirname){				//args lista di file da scrivere separati da virgola
									
	char *tmpstr;		
	printf("args %s\n",args);											
	char *token = strtok_r(args, ",", &tmpstr);
	char *resolvedpath;
	printf("token %s\n",token);
	while (token) {
		printf("sto scrivendo il file\n");
		resolvedpath = realpath(token, NULL);
		if(!resolvedpath)
			resolvedpath = realpath(token, NULL);
		if(!resolvedpath)
			exit(EXIT_FAILURE);

		if(writeFile(resolvedpath,dirname) != 0){		
			if(print_flag)
				printf("richiesta di scrittura del file <%s> è fallita\n",token);
		}
		free(resolvedpath);
    	token = strtok_r(NULL, ",", &tmpstr);
    	
    }
   
    return 0;

}


int r_req(char *args,char *dirname){				//args lista di file da leggere separati da virgola
	if(!args)
		return -1;				
	char *tmpstr;													
	char *token = strtok_r(args, ",", &tmpstr);
	void *buf = NULL;
	size_t filesize = -1;
	int len = strlen(dirname) + 1;
	int n = 0;
	while (token) {

		if(readFile(token, &buf, &filesize) != 0){		
			if(print_flag)
				printf("richiesta di lettura del file <%s> è fallita\n",token);
		}

  
    	if(dirname != NULL  && buf!= NULL && filesize != -1){
    		//******************DO ERROR MANAGEMENT *****************************
			int fd_file;
		    char str[10];
		    printf("cartella %s\n",dirname);
			
			char namefile[40] = {'f','i','l','e'};
			if(sprintf(str, "%d", n) <0){
				token = strtok_r(NULL, ",", &tmpstr);
				continue;
			}

			strncat(namefile,str,strlen(str));

			char* dirfile = malloc(len+strlen(namefile));
			if(!dirfile){
				token = strtok_r(NULL, ",", &tmpstr);
				continue;
			}
			memset(dirfile,'\0',len);

			strncpy(dirfile,dirname,len);

			strncat(dirfile, namefile, strlen(namefile));
			n++;
			printf("cartella %s\n",dirfile);
			//******************************************************************
			if((fd_file = open(dirfile, O_CREAT|O_WRONLY, 0666)) == -1){
				perror("open");
				token = strtok_r(NULL, ",", &tmpstr);
				continue;
			}

			if(writen(fd_file, buf, filesize) == -1){
				perror("writen");
				token = strtok_r(NULL, ",", &tmpstr);
				continue;
			}
			close(fd_file);
			free(dirfile);
    	}		
    	token = strtok_r(NULL, ",", &tmpstr);
						
    	free(buf);
    }
   
    return 0;

}
