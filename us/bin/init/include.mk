
INIT_SRCS=$(addprefix us/bin/init/,main.c)
INIT_OBJS=$(addprefix $(BUILDDIR)/,$(INIT_SRCS:.c=.o))

#INIT_LIBS=-Wl,--whole-archive -lbacktrace -Wl,--no-whole-archive
#INIT_CFLAGS=-fsanitize=undefined

$(BUILDDIR)/us/sysroot/usr/bin/init: $(INIT_OBJS) $(SYSROOT_READY) $(SYSLIBS) $(UTILS)
	@mkdir -p $(dir $@)
	@echo "[LD]      $@"
	@$(TWZCC) $(TWZLDFLAGS) -g -o $@ -MD $< $(INIT_LIBS) 

$(BUILDDIR)/us/bin/init/%.o: us/bin/init/%.c $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo "[CC]      $@"
	@$(TWZCC) $(TWZCFLAGS) $(INIT_CFLAGS) -o $@ -c -MD $<

SYSROOT_FILES+=$(BUILDDIR)/us/sysroot/usr/bin/init

