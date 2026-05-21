CXXFLAGS := -g -Wall -std=c++11 -lm
CXX=g++
SRC=procsim.cpp procsim_driver.cpp

all: build

build:
	$(CXX) $(CXXFLAGS) $(SRC) -o procsim

submit: build
	tar -cvf please_rename_me.tar procsim.cpp procsim.hpp

clean:
	rm -f procsim *.o *.tar
