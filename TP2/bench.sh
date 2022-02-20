#!/bin/bash

echo  "thread,file,time";

for file in {8}; do
    for threads in {5..8}; do
        echo -e "${threads},${file},\c" && ./main ${threads} tests/${file}_*.txt > /dev/null;
    done 
done
