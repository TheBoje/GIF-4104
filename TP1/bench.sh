#!/bin/bash

echo  "thread,file,time";
for nb_thread in {1..8}; do 
    for fileprefix in {1..7}; do
        echo -e "${nb_thread},${fileprefix},\c" && build/GIF-4104-TP1 ${nb_thread} tests/${fileprefix}_*.txt > /dev/null;
    done 
done
for i in {1}; do
echo -e "1,8,\c" && build/GIF-4104-TP1 1 tests/8_*.txt > /dev/null;
echo -e "4,8,\c" && build/GIF-4104-TP1 4 tests/8_*.txt > /dev/null;
echo -e "8,8,\c" && build/GIF-4104-TP1 8 tests/8_*.txt > /dev/null;
done