#!/bin/bash

make hello;

echo  "mat_size,time";

for size in {16,32,64,128,256,512,1024,2048,4096,8192}; do
    echo -e "${size},\c" && ./hello ${size} > /dev/null;
done