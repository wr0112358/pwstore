CXX=clang++
CXX_FLAGS=-std=c++11 -g -Wall -Werror 
DEFINES=
APP=pwstore

INSTALL_BIN_DIR=/opt/usr/bin
INSTALL_SHARE_DIR=/opt/usr/share

#INCLUDES=-I.deps/
INCLUDES=-I..
LDFLAGS=-lssl -lcrypto -lX11

sources_pwstore := pwstore.cc main.cc pwstore_api_cxx.cc pwstore_api_c.cc
objects_pwstore :=  $(sources_pwstore:.cc=.o)

%.o: %.cc
	$(CXX) $(CXX_FLAGS) $(INCLUDES) $(DEFINES) -c $? -o $@

all: pwstore

pwstore: $(objects_pwstore)
#	$(CXX) $(CXX_FLAGS) $(INCLUDES) $(LDFLAGS) $^ -o $(APP)
	$(CXX) $(CXX_FLAGS) $(INCLUDES) $^ -o $(APP) $(LDFLAGS)

clean:
	rm -f *.o pwstore pwstore.exe *.a test_c

install: pwstore
	cp pwstore $(INSTALL_BIN_DIR)
	cp .pwstore-completion.sh $(INSTALL_SHARE_DIR)
	@printf "\nTo enable bash completion for $(APP).\nAdd to .bashrc:\n\tsource $(INSTALL_SHARE_DIR)/.pwstore-completion.sh\n"


win64: CXX=x86_64-w64-mingw32-g++
win64: DEFINES=-DNO_GOOD $(MINGW_DEFINES)
# LDFLAGS must appear in this order an at the end of the linker command.
win64: LDFLAGS=-lssl -lcrypto -lgdi32 -lws2_32
win64: APP=pwstore.exe
win64: clean pwstore
	./win_install.sh




sources_pwstore_api := pwstore.cc pwstore_api_cxx.cc pwstore_api_c.cc
objects_pwstore_api :=  $(sources_pwstore:.cc=.o)
test_c_lib: $(objects_pwstore_api)
	ar rcs test_lib.a $^

%.o: %.c
	gcc -c $? -o $@

test_c_prog: test_c_api.o
#	 g++ $^ -o test_c $(LDFLAGS) test_lib.a
	gcc -lstdc++ test_c_api.c -o test_c $(LDFLAGS) test_lib.a

test_c: test_c_lib test_c_prog
