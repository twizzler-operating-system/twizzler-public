
SERIAL_SRCS=$(addprefix us/drivers/serial/,serial.c)
SERIAL_OBJS=$(addprefix $(BUILDDIR)/,$(SERIAL_SRCS:.c=.o))

#SERIAL_LIBS=-Wl,--whole-archive -lbacktrace -Wl,--no-whole-archive
#SERIAL_CFLAGS=-fsanitize=undefined

$(BUILDDIR)/us/sysroot/usr/bin/serial: $(SERIAL_OBJS) $(SYSROOT_READY) $(SYSLIBS) $(UTILS)
	@mkdir -p $(dir $@)
	@echo "[LD]      $@"
	@$(TWZCC) $(TWZLDFLAGS) -g -o $@ -MD $< $(SERIAL_LIBS) 

$(BUILDDIR)/us/drivers/serial/%.o: us/drivers/serial/%.c $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo "[CC]      $@"
	@$(TWZCC) $(TWZCFLAGS) $(SERIAL_CFLAGS) -o $@ -c -MD $<

SYSROOT_FILES+=$(BUILDDIR)/us/sysroot/usr/bin/serial

