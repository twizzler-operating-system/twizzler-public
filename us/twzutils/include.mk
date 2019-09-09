TWZUTILS=init login nls shell input pcie serial term

PROGS+=$(TWZUTILS)

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


