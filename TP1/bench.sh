#!/bin/bash

echo  "thread,file,time";
for nb_thread in {7..8}; do 
    for fileprefix in {1..7}; do
        echo -e "${nb_thread},${fileprefix},\c" && build/GIF-4104-TP1 ${nb_thread} tests/${fileprefix}_*.txt > /dev/null;
    done 
done