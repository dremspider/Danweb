CC=gcc

EXECUTABLES=rand

# targets (the first target is the default)

all:
	@echo "make all."
	gcc -std=c99 -g -ldl -lpthread -g danweb.c -o danweb 
	gcc -fPIC -shared hello.c -o hello.o -lc

danweb: danweb.c 
	gcc -std=c99 -Wall -g -ldl -lpthread -g danweb.c -o danweb 

hello: hello.c
	gcc -fPIC -shared hello.c -o hello -lc
hello: rev.c
	gcc -fPIC -shared rev.c -o rev -lc

clean: 
	rm hello.o 
	rm danweb

install:
	cp hello.o /usr/lib/ 
