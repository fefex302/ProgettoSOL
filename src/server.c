#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "utils.h"
#include "hash.h"

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

int config_file_parser(char *stringa, config_parameters* cfg);
int config_values_correctly_initialized(config_parameters *cfg);

static config_parameters configs;		//parametri di configurazione passati dal file config.txt
static hashtable *file_server;			//hashtable dove andrò a memorizzare tutti i file

int main(int argc,char *argv[]){

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

	//fprintf(stdout,"%ld %ld %ld\n",configs.NUM_THREADS,configs.MAX_FILE_NUMBER,configs.SERVER_CAPACITY_MBYTES);
	fclose(config_file);


	fileT *file1;
	char *name1 = malloc(5 * sizeof(char));
	strncpy(name1,"aaaa",5);
	char *content1 = malloc(5 * sizeof(char));
	strncpy(content1,"bbbb",5);

	file_server = icl_hash_create(5, &hash_pjw, &string_compare);

	file1 = icl_hash_insert(file_server,(void *) name1,(void *) content1);
	file1->size = 5;
	printf("key: %s content: %s size: %zu\n",(char *)file1->key,(char *)file1->data,file1->size);

	return 0;
}



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
