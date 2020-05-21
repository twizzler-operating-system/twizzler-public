TWZUTILS=login nls pcie tst kls lscpu bench kvdr lsmem init_bootstrap
# TWZUTILS+=sqb

LIBS_tst=-lncurses
LIBS_sqb=-lsqlite3
LIBS_login=#-lubsan -Wl,--whole-archive -lbacktrace -Wl,--no-whole-archive
CFLAGS_login=-O3 -march=native -fsanitize=undefined -fno-omit-frame-pointer
EXTRAS_kvdr=$(BUILDDIR)/us/bin/twzutils/kv.o
ALL_EXTRAS=$(EXTRAS_term) $(EXTRAS_init) $(EXTRAS_kvdr)

TWZUTILSLIBS=-lubsan -Wl,--whole-archive -lbacktrace -Wl,--no-whole-archive
TWZUTILSCFLAGS= -g

# init_bootstrap must be static
$(BUILDDIR)/us/sysroot/usr/bin/init_bootstrap: $(BUILDDIR)/us/bin/twzutils/init_bootstrap.o $(SYSROOT_READY) $(SYSLIBS) $(UTILS) $(ALL_EXTRAS)
	@echo "[LD]      $@"
	@$(TWZCC) $(TWZLDFLAGS) -static -g -o $@.elf -MD $< $(EXTRAS_$(notdir $@)) $(LIBS_$(notdir $@)) $(TWZUTILSLIBS)
	@echo "[SPLIT]   $@"
	@$(BUILDDIR)/utils/elfsplit $@.elf
	@cp $@.elf $@
	@mv $@.elf.data $@.data
	@rm $@.elf.text
	@mv $@.elf $(BUILDDIR)/us/bin/twzutils/$(notdir $@)

$(BUILDDIR)/us/sysroot/usr/bin/%: $(BUILDDIR)/us/bin/twzutils/%.o $(SYSROOT_READY) $(SYSLIBS) $(UTILS) $(ALL_EXTRAS)
	@echo "[LD]      $@"
	@$(TWZCC) $(TWZLDFLAGS) -g -o $@ -MD $< $(EXTRAS_$(notdir $@)) $(LIBS_$(notdir $@)) $(TWZUTILSLIBS)

$(BUILDDIR)/us/sysroot/usr/bin/%: $(BUILDDIR)/us/bin/twzutils/%.opp $(SYSROOT_READY) $(SYSLIBS) $(UTILS) $(ALL_EXTRAS)
	@echo "[LD]      $@"
	@$(TWZCXX) $(TWZLDFLAGS) -g -o $@ -MD $< $(EXTRAS_$(notdir $@)) $(LIBS_$(notdir $@)) $(TWZUTILSLIBS)

$(BUILDDIR)/us/bin/twzutils/%.o: us/bin/twzutils/%.c $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo "[CC]      $@"
	@$(TWZCC) $(TWZCFLAGS) $(TWZUTILSCFLAGS) -o $@ $(CFLAGS_$(basename $(notdir $@))) -c -MD $<

$(BUILDDIR)/us/bin/twzutils/%.opp: us/bin/twzutils/%.cpp $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo "[CC]      $@"
	@$(TWZCXX) $(TWZCFLAGS) $(TWZUTILSCFLAGS) -o $@ -c -MD $<

$(BUILDDIR)/us/sysroot/usr/share/pcieids: /usr/share/hwdata/pci.ids
	@mkdir -p $(dir $@)
	@cp $< $@

$(BUILDDIR)/us/sysroot/usr/share/mountains.jpeg: us/mountains.jpeg
	@mkdir -p $(dir $@)
	@cp $< $@


SYSROOT_FILES+=$(BUILDDIR)/us/sysroot/usr/share/pcieids
SYSROOT_FILES+=$(addprefix $(BUILDDIR)/us/sysroot/usr/bin/,$(TWZUTILS))

