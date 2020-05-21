TWZDEV_SRCS=$(addprefix us/bin/twzdev/,twzdev.c)
TWZDEV_OBJS=$(addprefix $(BUILDDIR)/,$(TWZDEV_SRCS:.c=.o))

#TWZDEV_LIBS=-Wl,--whole-archive -lbacktrace -Wl,--no-whole-archive
#TWZDEV_CFLAGS=-fsanitize=undefined

$(BUILDDIR)/us/sysroot/usr/bin/twzdev: $(TWZDEV_OBJS) $(SYSROOT_READY) $(SYSLIBS) $(UTILS)
	@mkdir -p $(dir $@)
	@echo "[LD]      $@"
	@$(TWZCC) $(TWZLDFLAGS) -g -o $@ -MD $< $(TWZDEV_LIBS) 

$(BUILDDIR)/us/bin/twzdev/%.o: us/bin/twzdev/%.c $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo "[CC]      $@"
	@$(TWZCC) $(TWZCFLAGS) $(TWZDEV_CFLAGS) -o $@ -c -MD $<

SYSROOT_FILES+=$(BUILDDIR)/us/sysroot/usr/bin/twzdev

