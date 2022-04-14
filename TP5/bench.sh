#!/bin/bash

echo "file,cores,time";
# Because Spark logging system, some stuff is printed to console which make this quite useless
for file in {"python","ulaval","apache"}; do
    for cores in {1,2,3,4,5,6,7,8}; do
        ram=$((16 / ${cores}))
        echo -e "${file},${cores},\c"
        make run-${file} EXEC_NUM=${cores} EXEC_MEM=${ram}G MASTER="local[${cores}]"> /dev/null
    done
done
