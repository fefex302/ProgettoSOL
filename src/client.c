#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>

#include "utils.h"

#define BUF_LEN 100
#define BUF_LEN_2 25

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

int main(int argc,char* argv[]){

	int opt;
	int n;
	char buffer[BUF_LEN];
	char *string_buffer[BUF_LEN_2];
	string_buffer[0] = "client_requests";

	while(1){

		memset(buffer,'\0',sizeof(char));
		if(fgets(buffer,BUF_LEN,stdin) == NULL)
			continue;

		n = parse_arguments(buffer,string_buffer);
		printf("n = %d\n",n);
		for(int i = 0; i<BUF_LEN_2; i++){
			if(string_buffer[i] == NULL)
				printf("%d = NULL\n",i);
			else 
				printf("%d = %s\n",i,string_buffer[i]);
		}
		optind = 1;
		while ((opt = getopt(n,string_buffer, "hf:w:W:r:R::d:t::l:u:c:p")) != -1) {
		    switch(opt) {
				case 'h': //arg_h(optarg); 
					help();
					break;
				case 'f': //arg_m(optarg);
					printf("arg = %s\n",optarg);
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
					break;
				case ':': {
			   		printf("l'opzione '-%c' richiede un argomento\n", optopt);
				    } break;
				case '?': {  // restituito se getopt trova una opzione non riconosciuta
				      printf("l'opzione '-%c' non e' gestita\n", optopt);
				    } break;
				default:;
    		}
    		printf("qui\n");
  		}
  		for(int i=1; i<n; i++)
  			free(string_buffer[i]);
	}
  return 0;




}


int parse_arguments(char *buf,char *buf2[]){

	char *tmpstr;													
	char *token = strtok_r(buf, " ", &tmpstr);
	int i = 1;

	while (token) {
		buf2[i] = malloc(strlen(token)*sizeof(char)+1);
		memset(buf2[i],'\0',sizeof(char));
    	//buf2[i] = token;
    	strncpy(buf2[i],token,strlen(token)+1);
    	token = strtok_r(NULL, " ", &tmpstr);
    	i++;
    }
    buf2[i-1][strcspn(buf2[i-1], "\n")] = '\0';
    int n = i;
    while(i<BUF_LEN_2-1){
    	buf2[i] = NULL;
    	i++;
    }

    buf2[BUF_LEN_2 - 1] = NULL;
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
