TWZUTILS=init login nls shell input pcie serial term

PROGS+=$(TWZUTILS)

$(BUILDDIR)/us/twzutils/%: $(BUILDDIR)/us/twzutils/%.o
	@echo [LD] $@
	@$(TWZCC) -static $(TWZLDFLAGS) -o $@ -MD $<

$(BUILDDIR)/us/twzutils/%.o: us/twzutils/%.c
	@mkdir -p $(dir $@)
	@echo [CC] $@
	@$(TWZCC) $(TWZCFLAGS) -o $@ -c -MD $<

$(BUILDDIR)/us/twzutils/term: $(BUILDDIR)/us/twzutils/term.o $(BUILDDIR)/us/twzutils/kbd.o
	@echo [LD] $@
	@$(TWZCC) -static $(TWZLDFLAGS) -o $@ -MD $(BUILDDIR)/us/twzutils/term.o $(BUILDDIR)/us/twzutils/kbd.o


