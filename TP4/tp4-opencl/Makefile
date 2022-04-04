SRC=main.cpp

CXX=g++
PLATFORM_ID=2
CXXFLAGS=-Wall -Wpedantic -lOpenCL -DGIF_PLATFORM_ID=$(PLATFORM_ID)
CXXDEBUGFLAGS=-g -DGIF_DEBUG

DEFAULT: main

main:
	$(CXX) $(CXXFLAGS) -o main main.cpp

debug:
	$(CXX) $(CXXFLAGS) $(CXXDEBUGFLAGS) -o main main.cpp

clean:
	rm main

.PHONY: clean
