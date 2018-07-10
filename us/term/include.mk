
$(BUILDDIR)/us/term/term: us/term/term.c $(US_LIBDEPS)
	@mkdir -p $(BUILDDIR)/us/term
	@echo "[CC]  $@"
	@$(TOOLCHAIN_PREFIX)gcc $(US_LDFLAGS) $(US_CFLAGS) -o $@ -nostdlib $(US_PRELINK) $< $(US_POSTLINK)

$(BUILDDIR)/us/term/term.0.meta: $(BUILDDIR)/us/term/term
$(BUILDDIR)/us/term/term.0: $(BUILDDIR)/us/term/term
	@echo "[PE]  $@"
	@$(TWZUTILSDIR)/postelf/postelf $(BUILDDIR)/us/term/term
	@echo "fot:R:0:1:0" | $(TWZUTILSDIR)/objbuild/objbuild -o $(BUILDDIR)/us/term/term.0 -a

TWZOBJS+=term/term.0 term/term.1

