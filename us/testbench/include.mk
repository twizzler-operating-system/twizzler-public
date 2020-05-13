TESTBENCH_SRCS=$(addprefix us/testbench/,main.c)
TESTBENCH_OBJS=$(addprefix $(BUILDDIR)/,$(TESTBENCH_SRCS:.c=.o))

TESTBENCH_LIBS=-Wl,--whole-archive -lbacktrace -Wl,--no-whole-archive
#TESTBENCH_CFLAGS=-fsanitize=undefined
$(BUILDDIR)/us/sysroot/usr/bin/testbench: $(TESTBENCH_OBJS) $(SYSROOT_READY) $(SYSLIBS) $(UTILS)
	@echo "[LD]      $@"
	@$(TWZCC) $(TWZLDFLAGS) -g -o $@ -MD $< $(TESTBENCH_LIBS) 
	#@echo "[SPLIT]   $@"
	#@$(BUILDDIR)/utils/elfsplit $@.elf
	#@cp $@.elf $@
	#@mv $@.elf.data $@.data
	#@rm $@.elf.text
	#@mv $@.elf $(BUILDDIR)/us/twzutils/$(notdir $@)



$(BUILDDIR)/us/testbench/%.o: us/testbench/%.c $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo "[CC]      $@"
	@$(TWZCC) $(TWZCFLAGS) $(TESTBENCH_CFLAGS) -o $@ -c -MD $< -fno-omit-frame-pointer  -funwind-tables -fexceptions

SYSROOT_FILES+=$(BUILDDIR)/us/sysroot/usr/bin/testbench

