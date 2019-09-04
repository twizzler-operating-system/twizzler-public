serial_srcs=$(addprefix us/serial/,serial.c)

serial_objs=$(addprefix $(BUILDDIR)/,$(serial_srcs:.c=.o))
serial_deps=$(addprefix $(BUILDDIR)/,$(serial_srcs:.c=.d))

serial_all: $(BUILDDIR)/us/serial/serial

$(BUILDDIR)/us/serial/serial: $(serial_objs) $(US_LIBDEPS)
	@echo "[CLD] $@"
	$(TWZCC) $(US_LDFLAGS) $(US_CFLAGS) -o $@ -nostdlib $(US_PRELINK) $(serial_objs) $(US_POSTLINK) -MD
-include $(serial_deps)

.PHONY: serial_all
