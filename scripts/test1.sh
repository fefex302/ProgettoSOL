#!/bin/bash
#prefix="valgrind --leak-check=full ./bin/client -f ./sock -p -t 200"
prefix="./bin/client -f ./sock -p -t 200"

#stampa help
$prefix -h

#scrittura di n file all'interno una directory con altre directory all'interno
$prefix -w ./files/,n=4

#scrittura di tutti i file all'interno una directory con altre directory all'interno
$prefix -w ./files/
$prefix -w ./files1/ -D $(pwd)/replacedFiles1/

#scrittura di file all'interno di una cartella inesistente
$prefix -w pippo

#scrittura di file separati da virgola
$prefix -W $(pwd)/files2/provaeseguibile,$(pwd)/files2/prova2,$(pwd)/files2/prova3 -D $(pwd)/replacedFiles2/

#scrittura di file inesistente su disco
$prefix -W fefex

#scrittura di un file già esistente per verificare che non sia sovrascritto (la richiesta deve fallire)
$prefix -W $(pwd)/files/prova1

#lettura di un file e scrittura in una cartella su disco
$prefix -d ./read_dir1/ -r $(pwd)/files/prova1

#lettura di tutti i file del server
$prefix -d ./read_dir2/ -R 

#lettura di N file del server
$prefix -d ./read_dir3/ -R n=3 

#lettura di un file inesistente (la richiesta deve fallire)
$prefix -r /pippo

#applicazione lock a un file
$prefix -l $(pwd)/files/prova1

#applicazione lock a un file inesistente (la richiesta deve fallire)
$prefix -l /pippo

#applicazione remove a un file
$prefix -l $(pwd)/files/prova1 -c $(pwd)/files/prova1

#applicazione remove a un file di cui non si ha la lock (la richiesta deve fallire)
$prefix -c $(pwd)/files/prova1



