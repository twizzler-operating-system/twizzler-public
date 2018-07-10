
$(BUILDDIR)/us/init/init: us/init/init.c $(US_LIBDEPS)
	@mkdir -p $(BUILDDIR)/us/init
	@echo "[CC]  $@"
	@$(TOOLCHAIN_PREFIX)gcc $(US_LDFLAGS) $(US_CFLAGS) -o $@ -nostdlib $(US_PRELINK) $< $(US_POSTLINK)

$(BUILDDIR)/us/init/init.0.meta: $(BUILDDIR)/us/init/init
$(BUILDDIR)/us/init/init.0: $(BUILDDIR)/us/init/init
	@echo "[PE]  $@"
	@$(TWZUTILSDIR)/postelf/postelf $(BUILDDIR)/us/init/init
	@echo "fot:R:0:1:0" | $(TWZUTILSDIR)/objbuild/objbuild -o $(BUILDDIR)/us/init/init.0 -a

TWZOBJS+=init/init.0 init/init.1

