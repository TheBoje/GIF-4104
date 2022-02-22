SRC=miller-rabin-gmp.cpp \
	main.cpp


SRCH=miller-rabin-gmp.hpp \
	Chrono.hpp

OBJ=$(SRC:.cpp=.o)
CXX=icc
CXXFLAGS=-g -Wall -pedantic -O2 -fopenmp -lgmp -lgmpxx -Wall

main: $(OBJ)
	$(CXX) $(CXXFLAGS) -o main $(OBJ)

%.o : %.cpp
	${CXX} ${CXXFLAGS} -o $@ $< -c

clean:
	rm *.o main

.PHONY: clean