#!/bin/bash

make;

echo  "proc,mat_size,time";

for proc in {1..4}; do
    for size in {4,8,16,32,64,128,256,384,512,640,768,896,1024}; do
        # echo -e "${proc},${size},\c" && mpirun -np ${proc} main ${size};
        echo -e "${proc},${size},\c" && mpirun -np ${proc} main ${size} > /dev/null;
    done
done
