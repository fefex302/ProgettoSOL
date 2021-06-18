#!/bin/bash

prefix="valgrind --leak-check=full ./bin/client -f ./sock -p -t 200"

$prefix -w ./files/ 

$prefix -w ./files1/ -D $(pwd)/replacedFiles1/

$prefix -w ./files2/ -D $(pwd)/replacedFiles2/
