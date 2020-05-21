
INPUT_SRCS=$(addprefix us/drivers/input/,input.c)
INPUT_OBJS=$(addprefix $(BUILDDIR)/,$(INPUT_SRCS:.c=.o))

#INPUT_LIBS=-Wl,--whole-archive -lbacktrace -Wl,--no-whole-archive
#INPUT_CFLAGS=-fsanitize=undefined

$(BUILDDIR)/us/sysroot/usr/bin/input: $(INPUT_OBJS) $(SYSROOT_READY) $(SYSLIBS) $(UTILS)
	@mkdir -p $(dir $@)
	@echo "[LD]      $@"
	@$(TWZCC) $(TWZLDFLAGS) -g -o $@ -MD $< $(INPUT_LIBS) 

$(BUILDDIR)/us/drivers/input/%.o: us/drivers/input/%.c $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo "[CC]      $@"
	@$(TWZCC) $(TWZCFLAGS) $(INPUT_CFLAGS) -o $@ -c -MD $<

SYSROOT_FILES+=$(BUILDDIR)/us/sysroot/usr/bin/input

