$(BUILDDIR)/utils/file2obj: utils/file2obj.c
	mkdir -p $(BUILDDIR)/utils
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $<
