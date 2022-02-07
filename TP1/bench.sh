#!/bin/bash

echo  "thread,file,time";


for threads in {1..8}; do
    echo -e "${threads},8,\c" && build/GIF-4104-TP1 ${threads} tests/8_*.txt > /dev/null;
done 
