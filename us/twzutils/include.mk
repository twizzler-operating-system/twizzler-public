TWZUTILS=init login nls shell input pcie serial term

PROGS+=$(TWZUTILS)

.PRECIOUS: $(addprefix $(BUILDDIR)/us/twzutils/,$(TWZUTILS))

$(BUILDDIR)/us/twzutils/%: $(BUILDDIR)/us/twzutils/%.o $(SYSROOT_READY)
	@echo [LD] $@
	@$(TWZCC) -static $(TWZLDFLAGS) -o $@ -MD $<

$(BUILDDIR)/us/twzutils/%.o: us/twzutils/%.c $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo [CC] $@
	@$(TWZCC) $(TWZCFLAGS) -o $@ -c -MD $<

$(BUILDDIR)/us/twzutils/term: $(BUILDDIR)/us/twzutils/term.o $(BUILDDIR)/us/twzutils/kbd.o $(SYSROOT_READY)
	@echo [LD] $@
	@$(TWZCC) -static $(TWZLDFLAGS) -o $@ -MD $(BUILDDIR)/us/twzutils/term.o $(BUILDDIR)/us/twzutils/kbd.o



$(BUILDDIR)/us/data/pcieids.obj: /usr/share/hwdata/pci.ids $(BUILDDIR)/utils/file2obj
	@echo [OBJ] $@
	@$(BUILDDIR)/utils/file2obj -i $< -o $@ -p rh

TWZOBJS+=$(BUILDDIR)/us/data/pcieids.obj

$(BUILDDIR)/us/inconsolata.sfn.obj: us/inconsolata.sfn $(BUILDDIR)/utils/file2obj
	@echo [OBJ] $@
	@$(BUILDDIR)/utils/file2obj -p rh -i $< -o $@

TWZOBJS+=$(BUILDDIR)/us/inconsolata.sfn.obj

