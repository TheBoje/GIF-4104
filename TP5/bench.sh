#!/bin/bash

/opt/apache-spark/sbin/stop-worker.sh  > /dev/null
/opt/apache-spark/sbin/start-master.sh  > /dev/null

echo "file,cores,time";
# Because Spark logging system, some stuff is printed to console which make this quite useless
for file in {"python","ulaval","apache"}; do
    for cores in {1,2,3,4,5,6,7,8}; do
        ram=$((16 / ${cores}));
        /opt/apache-spark/sbin/start-worker.sh "spark://boj-laptop:7077" -c ${cores} -m 16G > /dev/null
        echo -e "${file},${cores},\c" && make run-${file} EXEC_NUM=${cores} EXEC_MEM=${ram}G > /dev/null;
        /opt/apache-spark/sbin/stop-worker.sh  > /dev/null
    done
done
