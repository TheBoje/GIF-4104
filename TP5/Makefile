SPARK_DIR=/opt/apache-spark
CLASSPATH_FLAG=-cp ".:${SPARK_DIR}/jars/*"
JAVA_FLAGS=-Xss5m ${CLASSPATH_FLAG}
JAVAC_FLAGS=${CLASSPATH_FLAG}
IN_FILE="data/spark.apache.org.json"
MASTER="spark://boj-laptop:7077"
CORES=1
EXEC_MEM=2G

Main.class: Main.java
	javac ${JAVAC_FLAGS} Main.java

Main.jar: Main.class
	jar -cvfe Main.jar Main *.class

run: Main.jar
	echo "NOTE: please use \"make start-spark\" once"
	spark-submit --verbose --conf "spark.driver.defaultJavaOptions=-Xss3m" --total-executor-cores ${CORES} --executor-cores ${CORES} --num-executors 1 --driver-cores ${CORES} --master spark://boj-laptop:7077 --executor-memory ${EXEC_MEM} Main.jar ${IN_FILE} ${MASTER}

run-python:
	make run IN_FILE="data/python.org.json"

run-apache:
	make run IN_FILE="data/spark.apache.org.json"

run-ulaval:
	make run IN_FILE="data/www.fsg.ulaval.ca.json"

clean:
	-rm *.class *.jar

start-spark:
	${SPARK_DIR}/sbin/start-master.sh
	${SPARK_DIR}/sbin/start-worker.sh ${MASTER}
	echo "checkout http://localhost:8080/ for WebUI"

stop-spark:
	-${SPARK_DIR}/sbin/stop-master.sh
	${SPARK_DIR}/sbin/stop-worker.sh
