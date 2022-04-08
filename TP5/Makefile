APACHE_JARS_DIR=/opt/apache-spark/jars
CLASSPATH_FLAG=-cp ".:${APACHE_JARS_DIR}/*"
JAVA_FLAGS=-Xss5m ${CLASSPATH_FLAG}
JAVAC_FLAGS=${CLASSPATH_FLAG}
IN_FILE="data/spark.apache.org.json"

build:
	javac ${JAVAC_FLAGS} Main.java

run: build
	java ${JAVA_FLAGS} Main ${IN_FILE}

run-python:
	make run IN_FILE="data/python.org.json"

run-apache:
	make run IN_FILE="data/spark.apache.org.json"

run-ulaval:
	make run IN_FILE="data/www.fsg.ulaval.ca.json"

clean:
	rm *.class