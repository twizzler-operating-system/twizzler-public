PROGS=
TWZCC=cc
TWZCFLAGS=
TWZLDFLAGS=

all: all_progs

include test/include.mk
include init/include.mk

all_progs: $(addsuffix _all,$(PROGS))
	echo $(PROGS)

clean: $(addsuffix _clean,$(PROGS))

%.o: %.c
	$(TWZCC) $(TWZCFLAGS) -c -o $@ $<

.PHONY: all clean
