$(BUILDDIR)/utils/file2obj: utils/file2obj.c
	mkdir -p $(BUILDDIR)/utils
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $<

$(BUILDDIR)/utils/objstat: utils/objstat.c
	mkdir -p $(BUILDDIR)/utils
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $<

UTILS=$(addprefix $(BUILDDIR)/utils/,objstat file2obj)
