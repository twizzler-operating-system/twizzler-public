TWZUTILS=init login nls shell input pcie serial term bstream pty tst nvme

LIBS_tst=-lncurses
CFLAGS_term=-O3 -march=native -msse2 -msse4 -msse -mavx -ffast-math
EXTRAS_term=$(BUILDDIR)/us/twzutils/kbd.o
ALL_EXTRAS=$(EXTRAS_term)

$(BUILDDIR)/us/sysroot/usr/bin/%: $(BUILDDIR)/us/twzutils/%.o $(SYSROOT_READY) $(SYSLIBS) $(UTILS) $(ALL_EXTRAS)
	@echo "[LD]      $@"
	@$(TWZCC) -static $(TWZLDFLAGS) -o $@.elf -MD $< $(EXTRAS_$(notdir $@)) $(LIBS_$(notdir $@))
	@echo "[SPLIT]   $@"
	@$(BUILDDIR)/utils/elfsplit $@.elf
	@mv $@.elf.text $@
	@mv $@.elf.data $@.data
	@mv $@.elf $(BUILDDIR)/us/twzutils/$(notdir $@)

$(BUILDDIR)/us/sysroot/usr/bin/%: $(BUILDDIR)/us/twzutils/%.opp $(SYSROOT_READY) $(SYSLIBS) $(UTILS) $(ALL_EXTRAS)
	@echo "[LD]      $@"
	@$(TWZCXX) -static $(TWZLDFLAGS) -o $@.elf -MD $< $(EXTRAS_$(notdir $@)) $(LIBS_$(notdir $@))
	@echo "[SPLIT]   $@"
	@$(BUILDDIR)/utils/elfsplit $@.elf
	@mv $@.elf.text $@
	@mv $@.elf.data $@.data
	@mv $@.elf $(BUILDDIR)/us/twzutils/$(notdir $@)


$(BUILDDIR)/us/twzutils/%.o: us/twzutils/%.c $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo "[CC]      $@"
	@$(TWZCC) $(TWZCFLAGS) -o $@ $(CFLAGS_$(basename $(notdir $@))) -c -MD $<

$(BUILDDIR)/us/twzutils/%.opp: us/twzutils/%.cpp $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo "[CC]      $@"
	@$(TWZCXX) $(TWZCFLAGS) -o $@ -c -MD $<

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

