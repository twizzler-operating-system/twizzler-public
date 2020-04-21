TWZUTILS=init login nls shell input pcie serial term bstream pty tst nvme kls lscpu bench kvdr lsmem
# TWZUTILS+=sqb

LIBS_tst=-lncurses
LIBS_sqb=-lsqlite3
LIBS_login=#-lubsan -Wl,--whole-archive -lbacktrace -Wl,--no-whole-archive
CFLAGS_term=-O3 -march=native -msse2 -msse4 -msse -mavx -ffast-math
CFLAGS_init=-O3 -march=native
CFLAGS_login=-O3 -march=native -fsanitize=undefined -fno-omit-frame-pointer
CFLAGS_init_test=-O3 -march=native
EXTRAS_term=$(BUILDDIR)/us/twzutils/kbd.o
EXTRAS_init=$(BUILDDIR)/us/twzutils/init_test.o
EXTRAS_kvdr=$(BUILDDIR)/us/twzutils/kv.o
ALL_EXTRAS=$(EXTRAS_term) $(EXTRAS_init) $(EXTRAS_kvdr)

TWZUTILSLIBS=-lubsan -Wl,--whole-archive -lbacktrace -Wl,--no-whole-archive
#TWZLDFLAGS=-lubsan

TWZUTILSCFLAGS=-fsanitize=undefined -g

$(BUILDDIR)/us/sysroot/usr/bin/%: $(BUILDDIR)/us/twzutils/%.o $(SYSROOT_READY) $(SYSLIBS) $(UTILS) $(ALL_EXTRAS)
	@echo "[LD]      $@"
	@$(TWZCC) -static $(TWZLDFLAGS) -g -o $@.elf -MD $< $(EXTRAS_$(notdir $@)) $(LIBS_$(notdir $@)) $(TWZUTILSLIBS)
	@echo "[SPLIT]   $@"
	@$(BUILDDIR)/utils/elfsplit $@.elf
	@cp $@.elf $@
	@mv $@.elf.data $@.data
	@rm $@.elf.text
	@mv $@.elf $(BUILDDIR)/us/twzutils/$(notdir $@)

$(BUILDDIR)/us/sysroot/usr/bin/%: $(BUILDDIR)/us/twzutils/%.opp $(SYSROOT_READY) $(SYSLIBS) $(UTILS) $(ALL_EXTRAS)
	@echo "[LD]      $@"
	@$(TWZCXX) -static $(TWZLDFLAGS) -g -o $@.elf -MD $< $(EXTRAS_$(notdir $@)) $(LIBS_$(notdir $@)) $(TWZUTILSLIBS)
	@echo "[SPLIT]   $@"
	@$(BUILDDIR)/utils/elfsplit $@.elf
	@cp $@.elf $@
	@mv $@.elf.data $@.data
	@rm $@.elf.text
	@mv $@.elf $(BUILDDIR)/us/twzutils/$(notdir $@)


$(BUILDDIR)/us/twzutils/%.o: us/twzutils/%.c $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo "[CC]      $@"
	@$(TWZCC) $(TWZCFLAGS) $(TWZUTILSCFLAGS) -o $@ $(CFLAGS_$(basename $(notdir $@))) -c -MD $<

$(BUILDDIR)/us/twzutils/%.opp: us/twzutils/%.cpp $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo "[CC]      $@"
	@$(TWZCXX) $(TWZCFLAGS) $(TWZUTILSCFLAGS) -o $@ -c -MD $<

$(BUILDDIR)/us/sysroot/usr/share/pcieids: /usr/share/hwdata/pci.ids
	@mkdir -p $(dir $@)
	@cp $< $@

$(BUILDDIR)/us/sysroot/usr/share/inconsolata.sfn: us/inconsolata.sfn
	@mkdir -p $(dir $@)
	@cp $< $@

$(BUILDDIR)/us/sysroot/usr/share/mountains.jpeg: us/mountains.jpeg
	@mkdir -p $(dir $@)
	@cp $< $@


SYSROOT_FILES+=$(BUILDDIR)/us/sysroot/usr/share/inconsolata.sfn $(BUILDDIR)/us/sysroot/usr/share/pcieids $(BUILDDIR)/us/sysroot/usr/share/mountains.jpeg
SYSROOT_FILES+=$(addprefix $(BUILDDIR)/us/sysroot/usr/bin/,$(TWZUTILS))

