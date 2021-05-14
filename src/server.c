#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "utils.h"

#define BUF_LEN 100

//#include "hash.h"
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

typedef struct _configs{
	long NUM_THREADS;
	long MAX_FILE_NUMBER;
	long SERVER_CAPACITY_MBYTES;
}config_parameters;

int config_file_parser(char *stringa, config_parameters* cfg);

int main(int argc,char *argv[]){

	if(argc != 2){
		printf("you must use a config file\n");
		return EXIT_FAILURE;
	}

	FILE *config_file;

	NULL_EXIT((config_file = fopen(argv[1],"r")),"config file opening");

	char buffer[BUF_LEN];

	config_parameters configs;

	int i = 0;

	while(i<3){
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
		i++;
	}

	fprintf(stdout,"%ld %ld %ld\n",configs.NUM_THREADS,configs.MAX_FILE_NUMBER,configs.SERVER_CAPACITY_MBYTES);
	fclose(config_file);

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
			if(cfg->NUM_THREADS <= 0){
				printf("a value is non positive\n");
				return -1;
			}
		}

		else if(strncmp("MAX_FILE_NUMBER",token,16) == 0){
			token = strtok_r(NULL, "=", &tmpstr);
			token[strcspn(token, "\n")] = '\0';
			if(isNumber(token,&(cfg->MAX_FILE_NUMBER)) != 0)
				return -1;
			if(cfg->MAX_FILE_NUMBER <= 0){
				printf("a value is non positive\n");
				return -1;
			}
		}

		else if(strncmp("SERVER_CAPACITY_MBYTES",token,23) == 0){
			token = strtok_r(NULL, "=", &tmpstr);
			token[strcspn(token, "\n")] = '\0';
			if(isNumber(token,&(cfg->SERVER_CAPACITY_MBYTES)) != 0)
				return -1;
			if(cfg->SERVER_CAPACITY_MBYTES <= 0){
				printf("a value is non positive\n");
				return -1;
			}
		}

	token = strtok_r(NULL, "=", &tmpstr);
	}
}
