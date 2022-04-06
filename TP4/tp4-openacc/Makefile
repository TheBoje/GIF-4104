SRC=Matrix.cpp \
	main.cpp

OBJ=$(SRC:.cpp=.o)
CXX=nvc++
CXXFLAGS=-fast -acc=multicore

DEFAULT: main

main: $(OBJ)
	$(CXX) $(CXXFLAGS) -o main $(OBJ)

%.o : %.cpp
	@echo "Compiling $<"
	${CXX} ${CXXFLAGS} -c $<

clean:
	rm *.o main

.PHONY: clean
