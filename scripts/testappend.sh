#!/bin/bash

prefix="valgrind --leak-check=full ./bin/client -f ./sock -p -t 200"

$prefix -w ./files/ -a $(pwd)/files/prova1,$(pwd)/appDir/fileToAppend -d ./appDir/ -r $(pwd)/files/prova1
