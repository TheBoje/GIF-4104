#!/bin/bash

echo  "thread,file,time";

for file in {1..8}; do
    for threads in {1..8}; do
        echo -e "${threads},${file},\c" && ./main ${threads} tests/${file}_*.txt > /dev/null;
    done 
done
