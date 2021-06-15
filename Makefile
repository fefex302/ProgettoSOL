SHELL           = /bin/bash
CC				=  gcc
AR              =  ar
CFLAGS	        = -std=gnu99 -pedantic -Wall -g
ARFLAGS         =  rvs
INCLUDES		= -I. -I ./includes
LDFLAGS 		= -L.
OPTFLAGS		= -O3 
LIBS            = -lpthread

OBJ				= bin/
INC 			= includes/
SRC 			= src/
STRCT 			= structures/

TARGETS		= $(OBJ)server \
				$(OBJ)client


.PHONY: all clean cleanall test1
.SUFFIXES: .c .h


all: $(TARGETS)


$(OBJ)client: $(OBJ)api.o $(OBJ)client.o
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) $^ -o $@ $(LDFLAGS) $(LIBS)

$(OBJ)server: $(OBJ)server.o $(OBJ)hash.o
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) $^ -o $@ $(LDFLAGS) $(LIBS)

$(OBJ)server.o: $(SRC)server.c $(INC)utils.h $(INC)hash.h $(INC)api.h $(INC)list.h
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c -o $@ $<

$(OBJ)client.o: $(SRC)client.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c -o $@ $<

$(OBJ)hash.o: $(STRCT)hash.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c -o $@ $<

$(OBJ)api.o: $(SRC)api.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c -o $@ $<

clean		: 
	rm -f $(TARGETS)
cleanall	: clean
	\ rm -f ./bin/*.o *~ *.a ./sock

test1	:
	valgrind --leak-check=full ./bin/server config.txt & bash ./scripts/test1.sh; kill -1 $$!
