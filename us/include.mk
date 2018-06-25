USFILES=$(addprefix $(BUILDDIR)/us/, test.0 test.0.meta)

$(BUILDDIR)/us/test: us/test.s
	$(TOOLCHAIN_PREFIX)gcc $< -o $@ -nostdlib

$(BUILDDIR)/us/test.0.meta: $(BUILDDIR)/us/test
$(BUILDDIR)/us/test.0: $(BUILDDIR)/us/test
	$(TWZUTILSDIR)/postelf/postelf $(BUILDDIR)/us/test

$(BUILDDIR)/us:
	@mkdir -p $@

userspace: $(BUILDDIR)/us .twizzlerutils $(USFILES)

