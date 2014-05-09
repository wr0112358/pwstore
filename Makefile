CXX=clang++
CXX_FLAGS=-std=c++11 -g -Wall -Werror 


INCLUDES=-I.deps/
#INCLUDES=-I/opt/usr/include/
LDFLAGS=-lssl -lcrypto

sources_pwstore := pwstore.cc main.cc
objects_pwstore :=  $(sources_pwstore:.cc=.o)

%.o: %.cc
	$(CXX) $(CXX_FLAGS) $(INCLUDES) -c $? -o $@

all: pwstore

pwstore: $(objects_pwstore)
	$(CXX) $(CXX_FLAGS) $(INCLUDES) $(LDFLAGS) $^ -o $@

clean :
	rm -f *.o pwstore
