$(BUILDDIR)/utils/file2obj: utils/file2obj.c
	mkdir -p $(BUILDDIR)/utils
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $<

$(BUILDDIR)/utils/objstat: utils/objstat.c utils/blake2.h utils/blake2.c
	mkdir -p $(BUILDDIR)/utils
	$(HOSTCC) $(HOSTCFLAGS) -o $@ utils/objstat.c utils/blake2.c

$(BUILDDIR)/utils/elfsplit: utils/elfsplit.c
	mkdir -p $(BUILDDIR)/utils
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $<

$(BUILDDIR)/utils/bsv: utils/bsv.c
	mkdir -p $(BUILDDIR)/utils
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $< -Ius/include

UTILS=$(addprefix $(BUILDDIR)/utils/,objstat file2obj elfsplit bsv)
