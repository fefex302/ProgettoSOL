#!/bin/bash
prefix="valgrind --leak-check=full ./bin/client -f ./sock -p -t 200"

#scrittura di file all'interno una directory con altre directory all'interno
$prefix -w ./files/,n=3

#scrittura di file separati da virgola
$prefix -W $(pwd)/files2/prova1,$(pwd)/files2/prova2,$(pwd)/files2/prova3

#scrittura di un file già esistente per verificare che non sia sovrascritto (la richiesta deve fallire)
$prefix -W $(pwd)/files/prova1

#lettura di un file e scrittura in una cartella su disco
$prefix -d ./IDEE/ -r  $(pwd)/files/prova1

#applicazione lock a un file
$prefix -l $(pwd)/files/prova1

#applicazione remove a un file
$prefix -l $(pwd)/files/prova1 -c $(pwd)/files/prova1



