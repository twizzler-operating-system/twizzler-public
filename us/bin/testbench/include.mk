
TESTBENCH_SRCS=$(addprefix us/bin/testbench/,main.c)
TESTBENCH_OBJS=$(addprefix $(BUILDDIR)/,$(TESTBENCH_SRCS:.c=.o))

#TESTBENCH_LIBS=-Wl,--whole-archive -lbacktrace -Wl,--no-whole-archive
#TESTBENCH_CFLAGS=-fsanitize=undefined

$(BUILDDIR)/us/sysroot/usr/bin/testbench: $(TESTBENCH_OBJS) $(SYSROOT_READY) $(SYSLIBS) $(UTILS)
	@mkdir -p $(dir $@)
	@echo "[LD]      $@"
	@$(TWZCC) $(TWZLDFLAGS) -g -o $@ -MD $< $(TESTBENCH_LIBS) 

$(BUILDDIR)/us/bin/testbench/%.o: us/bin/testbench/%.c $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo "[CC]      $@"
	@$(TWZCC) $(TWZCFLAGS) $(TESTBENCH_CFLAGS) -o $@ -c -MD $<

SYSROOT_FILES+=$(BUILDDIR)/us/sysroot/usr/bin/testbench

