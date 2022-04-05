#!/bin/bash

make;

echo  "mat_size,time";

for size in {16,32,64,128,256,512,1024,2048,4096}; do
    echo -e "${size},\c" && ./main ${size} > /dev/null;
done
