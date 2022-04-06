#!/bin/bash

make;

echo  "mat_size,time";

for size in {16,32,64,128}; do
    echo -e "${size},\c" && ./main ${size} > /dev/null;
done
