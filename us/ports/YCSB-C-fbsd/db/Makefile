CC?=clang++
CFLAGS?=-std=c++11 -c -g -Wall -O3
INCLUDES=-I../ -I/usr/local/include
SOURCES=$(wildcard *.cc)
OBJECTS=$(SOURCES:.cc=.o)

all: $(SOURCES) $(OBJECTS)

db_factory.o: db_sl.h

.cc.o:
	$(CC) -c $(CFLAGS) $(INCLUDES) $< -o $@

clean:
	$(RM) $(OBJECTS)

