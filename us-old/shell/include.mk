
$(BUILDDIR)/us/shell/shell: us/shell/shell.c $(US_LIBDEPS)
	@mkdir -p $(BUILDDIR)/us/shell
	@echo "[CC]  $@"
	@$(TOOLCHAIN_PREFIX)gcc $(US_LDFLAGS) $(US_CFLAGS) -o $@ -nostdlib $(US_PRELINK) $< $(US_POSTLINK) -MD

-include $(BUILDDIR)/us/shell/shell.d

$(BUILDDIR)/us/shell/shell.0.meta: $(BUILDDIR)/us/shell/shell
$(BUILDDIR)/us/shell/shell.0: $(BUILDDIR)/us/shell/shell
	@echo "[PE]  $@"
	@$(TWZUTILSDIR)/postelf/postelf $(BUILDDIR)/us/shell/shell
	@echo "fot:R:0:1:0" | $(TWZUTILSDIR)/objbuild/objbuild -o $(BUILDDIR)/us/shell/shell.0 -a

TWZOBJS+=shell/shell.0 shell/shell.1

