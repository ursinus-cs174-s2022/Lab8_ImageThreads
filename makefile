CC=g++
CFLAGS=-std=c++11 -g -Wall -pthread

all: imfilter

simplecanvas.o: simplecanvas/simplecanvas.h simplecanvas/simplecanvas.cpp
	$(CC) $(CFLAGS) -c simplecanvas/simplecanvas.cpp

imfilter: imfilter.cpp simplecanvas.o
	$(CC) $(CFLAGS) -o imfilter simplecanvas.o imfilter.cpp


clean:
	rm *.o imfilter