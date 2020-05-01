SECTEST_SRCS=$(addprefix us/sectest/,main.c)
SECTEST_OBJS=$(addprefix $(BUILDDIR)/,$(SECTEST_SRCS:.c=.o))

SECTEST2_SRCS=$(addprefix us/sectest/,stlib.c)
SECTEST2_OBJS=$(addprefix $(BUILDDIR)/,$(SECTEST2_SRCS:.c=.o))

SECTEST_LIBS=-lubsan -Wl,--whole-archive -lbacktrace -Wl,--no-whole-archive -ltomcrypt -ltommath
SECTEST_CFLAGS=-g -fPIC -fpie

$(BUILDDIR)/us/sysroot/usr/bin/st: $(SECTEST_OBJS) $(SYSROOT_READY) $(SYSLIBS) $(UTILS)
	@echo "[LD]      $@"
	@$(TWZCC) -static $(TWZLDFLAGS) -g -o $@.elf -MD $< $(SECTEST_LIBS)
	@echo "[SPLIT]   $@"
	@$(BUILDDIR)/utils/elfsplit $@.elf
	@cp $@.elf $@
	@mv $@.elf.data $@.data
	@rm $@.elf.text
	@mv $@.elf $(BUILDDIR)/us/sectest/$(notdir $@)

$(BUILDDIR)/us/sysroot/usr/bin/st-lib: $(SECTEST2_OBJS) $(SYSROOT_READY) $(SYSLIBS) $(UTILS)
	@echo "[LD]      $@"
	@$(TWZCC) -static $(TWZLDFLAGS) -static-pie -fPIC -fpie -fPIE -g -o $@.elf -MD $< $(SECTEST_LIBS)
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

SYSROOT_FILES+=$(BUILDDIR)/us/sysroot/usr/bin/st $(BUILDDIR)/us/sysroot/usr/bin/st-lib

