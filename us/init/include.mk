PROGS+=init

init_srcs=$(addprefix init/,init.c)

init_objs=$(init_srcs:.c=.o)
init_deps=$(init_srcs:.c=.d)

init_all: init/init

init_clean:
	-rm $(init_objs) $(init_deps) init/init

init/init: $(init_objs)
	$(TWZCC) $(TWZLDFLAGS) -o init/init $(init_objs)

-include $(init_deps)

.PHONY: init_all init_clean
