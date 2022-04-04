SRC=Matrix.cpp \
	main.cpp

OBJ=$(SRC:.cpp=.o)
CXX=nvc++
CXXFLAGS=-fast -acc=gpu -Minfo 

DEFAULT: main

run: main
	./main

main: $(OBJ)
	$(CXX) $(CXXFLAGS) -o main $(OBJ)
	@echo "Compilation done! Use \"make run\" to run"

%.o : %.cpp
	@echo "Compiling $<"
	${CXX} ${CXXFLAGS} -c $<

clean:
	rm *.o main

.PHONY: clean
