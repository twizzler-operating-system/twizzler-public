CC?=clang++
SOURCES=$(wildcard *.cc)
OBJECTS=$(SOURCES:.cc=.o)

all: $(SOURCES) $(OBJECTS)

.cc.o:
	$(CC) -O3 -c $(CFLAGS) $< -o $@

clean:
	$(RM) $(OBJECTS)
