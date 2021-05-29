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
#include <fcntl.h>

#include <assert.h>
#include "api.h"
#include "utils.h"

#define BUF_LEN 100
#define BUF_LEN_2 25
#define MSEC 10

static int flag_f = 0;	//variabile che identifica se la connessione col socket è stata effettuata o meno
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


//*******************************************MAIN*****************************************************************************
int main(int argc,char* argv[]){

	if(argc == 1){
		printf("usare almeno un argomento, lanciare con opzione -h per vedere le richieste possibili\n");
		return 0;
	}

	int opt;
	int end = 0;	//usato per terminare il client

	char *sockname = NULL;
	char *dirname = NULL;	//cartella dove memorizzare i file letti dal server	

	int d=0,r=0,R=0,h=0,f=0,p=0;

	for(int i = 1; i<argc; i++){
		if(strncmp(argv[i],"-d",3) == 0)
			d = i;
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
			r++;
			dirname = argv[r];
		}
		else if(R>0){
			R++;
			dirname = argv[R];
		}
		else{
			printf("-d needs at least one -r or -R\n");
			end = 1;
		}
	}

	//abilito le stampe delle operazioni
	if(p>0){
		print_flag = 1;
	}
		
	opterr = 0;

	while ((opt = getopt(argc,argv, ":hf:w:W:r:R:d:t:l:u:c:p")) != -1 && !end) {
		switch(opt) {
			case 'h':  
				break;
			case 'f':
				break;
			case 'w':   
				break;
			case 'W': 
				W_req(optarg,dirname);
				break;
			case 'r':  
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
  	}
  	sleep(2);
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


int help(){
	printf("%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
			"Opzioni accettate dal client: ",
			"-h",
			"-f filename",
			"-wdirname[,n=0]",
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
	strncpy(s,args,strlen(args)+1);
	if(openConnection(s,MSEC,abstime) == -1){
		free(s);
		return -1;
	}

	else return 0;
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
	char *token = strtok_r(args, ",", &tmpstr);
	int i = 0;
	
	while (token) {

		if(writeFile(token,dirname) != 0){		
			if(print_flag)
				printf("richiesta di scrittura del file <%s> è fallita\n",token);
		}

    	token = strtok_r(NULL, ",", &tmpstr);
    	
    }
   
    return 0;

}

/*
int r_req(char *args,char *dirname){				//args lista di file da leggere separati da virgola
									
	char *tmpstr;													
	char *token = strtok_r(args, ",", &tmpstr);
	int i = 0;
	char* buf = NULL;
	size_t filesize = -1;

	while (token) {

		if(readFile(token, buf, &filesize) != 0){		
			if(print_flag)
				printf("richiesta di lettura del file <%s> è fallita\n",token);
		}

    	token = strtok_r(NULL, ",", &tmpstr);
    	
    }
   
    return 0;

}*/
