PROGS=nls

$(BUILDDIR)/us/utils/%: us/utils/%.c $(US_LIBDEPS)
	@mkdir -p $(BUILDDIR)/us/utils
	@echo "[CC]  $@"
	@$(TOOLCHAIN_PREFIX)gcc $(US_LDFLAGS) $(US_CFLAGS) -o $@ -nostdlib $(US_PRELINK) $< $(US_POSTLINK) -MD

-include $(BUILDDIR)/us/utils/*.d

$(BUILDDIR)/us/utils/%.0: $(BUILDDIR)/us/utils/%
	@echo "[PE]  $@"
	@$(TWZUTILSDIR)/postelf/postelf $<

TWZOBJS+=$(addsuffix .0,$(addprefix utils/,$(PROGS)))

TWZOBJS+=$(addsuffix .1,$(addprefix utils/,$(PROGS)))

