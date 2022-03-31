SRC=main.cpp

OBJ=$(SRC:.cpp=.o)
CXX=mpic++
LXX=mpirun
CXXFLAGS=-g -Wall -pedantic -fopenacc
LXXFLAGS=-np 1

DEFAULT: main

run: main
	$(LXX) $(LXXFLAGS) main 256

main: $(OBJ)
	$(CXX) $(CXXFLAGS) -o main $(OBJ)
	@echo "Compilation done! Use \"make run\" to run"

%.o : %.cpp
	@echo "Compiling $<"
	${CXX} ${CXXFLAGS} -c $<

clean:
	rm *.o main

.PHONY: clean
