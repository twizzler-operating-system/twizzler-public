
KEYBOARD_SRCS=$(addprefix us/drivers/keyboard/,keyboard.c)
KEYBOARD_OBJS=$(addprefix $(BUILDDIR)/,$(KEYBOARD_SRCS:.c=.o))

#KEYBOARD_LIBS=-Wl,--whole-archive -lbacktrace -Wl,--no-whole-archive
#KEYBOARD_CFLAGS=-fsanitize=undefined

$(BUILDDIR)/us/sysroot/usr/bin/keyboard: $(KEYBOARD_OBJS) $(SYSROOT_READY) $(SYSLIBS) $(UTILS)
	@mkdir -p $(dir $@)
	@echo "[LD]      $@"
	@$(TWZCC) $(TWZLDFLAGS) -g -o $@ -MD $< $(KEYBOARD_LIBS) 

$(BUILDDIR)/us/drivers/keyboard/%.o: us/drivers/keyboard/%.c $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo "[CC]      $@"
	@$(TWZCC) $(TWZCFLAGS) $(KEYBOARD_CFLAGS) -o $@ -c -MD $<

SYSROOT_FILES+=$(BUILDDIR)/us/sysroot/usr/bin/keyboard

