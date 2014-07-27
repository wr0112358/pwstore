CXX=clang++
CXX_FLAGS=-std=c++11 -g -Wall -Werror 
DEFINES=
APP=pwstore

INCLUDES=-I.deps/
#INCLUDES=-I..
LDFLAGS=-lssl -lcrypto -lX11

sources_pwstore := pwstore.cc main.cc
objects_pwstore :=  $(sources_pwstore:.cc=.o)

%.o: %.cc
	$(CXX) $(CXX_FLAGS) $(INCLUDES) $(DEFINES) -c $? -o $@

all: pwstore

pwstore: $(objects_pwstore)
#	$(CXX) $(CXX_FLAGS) $(INCLUDES) $(LDFLAGS) $^ -o $(APP)
	$(CXX) $(CXX_FLAGS) $(INCLUDES) $^ -o $(APP) $(LDFLAGS)

clean :
	rm -f *.o pwstore pwstore.exe

win64: CXX=x86_64-w64-mingw32-g++
win64: DEFINES=-DNO_GOOD $(MINGW_DEFINES)
# LDFLAGS must appear in this order an at the end of the linker command.
win64: LDFLAGS=-lssl -lcrypto -lgdi32 -lws2_32
win64: APP=pwstore.exe
win64: clean pwstore
	./win_install.sh
