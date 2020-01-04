CC?=clang++
CXX?=clang++
CFLAGS=-std=c++11 -g -Wall -I./ -O3
CXXFLAGS=-std=c++11 -g -Wall -I./ -O3
LDFLAGS= -lpthread
SUBDIRS=core db
SUBSRCS=$(wildcard core/*.cc) db/db_factory.cc db/hashtable_db.cc
OBJECTS=$(SUBSRCS:.cc=.o)
EXEC=ycsbc

all: $(SUBDIRS) $(EXEC)

export CFLAGS
export CC

$(SUBDIRS):
	CFLAGS="$(CFLAGS)" $(MAKE) -C $@

#SQ=/usr/local/lib/libsqlite3.a
#SQ=../sq-bench/sqlightning/.libs/libsqlite3.a
SQ=-lsqlite3
$(EXEC): $(wildcard *.cc) $(OBJECTS)
	$(CC) -static $(CFLAGS) $^ $(LDFLAGS) -o $@ $(SQ) #/usr/local/lib/libsqlite3.a

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir $@; \
	done
	$(RM) $(EXEC)

.PHONY: $(SUBDIRS) $(EXEC)

