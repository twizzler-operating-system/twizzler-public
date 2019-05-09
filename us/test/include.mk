PROGS+=test

test_srcs=$(addprefix test/,test.c)

test_objs=$(test_srcs:.c=.o)
test_deps=$(test_srcs:.c=.d)

test_all: test/test

test_clean:
	-rm $(test_objs) $(test_deps) test/test

test/test: $(test_objs)
	$(TWZCC) $(TWZLDFLAGS) -o test/test $(test_objs)

-include $(test_deps)

.PHONY: test_all test_clean
