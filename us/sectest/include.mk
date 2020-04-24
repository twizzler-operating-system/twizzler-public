SECTEST_SRCS=$(addprefix us/sectest/,main.c)
SECTEST_OBJS=$(addprefix $(BUILDDIR)/,$(SECTEST_SRCS:.c=.o))

SECTEST_LIBS=-lubsan -Wl,--whole-archive -lbacktrace -Wl,--no-whole-archive -ltomcrypt -ltommath
SECTEST_CFLAGS=-fsanitize=undefined

$(BUILDDIR)/us/sysroot/usr/bin/st: $(SECTEST_OBJS) $(SYSROOT_READY) $(SYSLIBS) $(UTILS)
	@echo "[LD]      $@"
	@$(TWZCC) -static $(TWZLDFLAGS) -g -o $@.elf -MD $< $(SECTEST_LIBS)
	@echo "[SPLIT]   $@"
	@$(BUILDDIR)/utils/elfsplit $@.elf
	@cp $@.elf $@
	@mv $@.elf.data $@.data
	@rm $@.elf.text
	@mv $@.elf $(BUILDDIR)/us/sectest/$(notdir $@)

$(BUILDDIR)/us/sectest/%.o: us/sectest/%.c $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo "[CC]      $@"
	@$(TWZCC) $(TWZCFLAGS) $(SECTEST_CFLAGS) -o $@ -c -MD $<

SYSROOT_FILES+=$(BUILDDIR)/us/sysroot/usr/bin/st

