HOSTCFLAGS+=-fsanitize=undefined -fsanitize=address

$(BUILDDIR)/utils/file2obj: utils/file2obj.c
	mkdir -p $(BUILDDIR)/utils
	$(HOSTCC) $(HOSTCFLAGS) -Ius/include -o $@ $<

$(BUILDDIR)/utils/appendobj: utils/appendobj.c
	mkdir -p $(BUILDDIR)/utils
	$(HOSTCC) $(HOSTCFLAGS) -Ius/include -o $@ $<

$(BUILDDIR)/utils/objstat: utils/objstat.c utils/blake2.h utils/blake2.c
	mkdir -p $(BUILDDIR)/utils
	$(HOSTCC) $(HOSTCFLAGS) -Ius/include -o $@ utils/objstat.c utils/blake2.c

$(BUILDDIR)/utils/elfsplit: utils/elfsplit.c
	mkdir -p $(BUILDDIR)/utils
	$(HOSTCC) $(HOSTCFLAGS) -Ius/include -o $@ $<

$(BUILDDIR)/utils/bsv: utils/bsv.c
	mkdir -p $(BUILDDIR)/utils
	$(HOSTCC) $(HOSTCFLAGS) -Ius/include -o $@ $< -Ius/include

$(BUILDDIR)/utils/sctx: utils/sctx.c
	mkdir -p $(BUILDDIR)/utils
	$(HOSTCC) $(HOSTCFLAGS) -Ius/include -o $@ $< -Ius/include

$(BUILDDIR)/utils/mkcap: utils/mkcap.c
	mkdir -p $(BUILDDIR)/utils
	$(HOSTCC) $(HOSTCFLAGS) -Ius/include -o $@ utils/mkcap.c -Ius/include -ltomcrypt

$(BUILDDIR)/utils/mkdlg: utils/mkdlg.c
	mkdir -p $(BUILDDIR)/utils
	$(HOSTCC) $(HOSTCFLAGS) -Ius/include -o $@ utils/mkdlg.c -Ius/include -ltomcrypt

$(BUILDDIR)/utils/makekey: utils/makekey.c
	mkdir -p $(BUILDDIR)/utils
	$(HOSTCC) $(HOSTCFLAGS) -Ius/include -o $@ utils/makekey.c -Ius/include

$(BUILDDIR)/utils/mkuser: utils/user.c
	mkdir -p $(BUILDDIR)/utils
	$(HOSTCC) $(HOSTCFLAGS) -Ius/include -o $@ utils/user.c -Ius/include




UTILS=$(addprefix $(BUILDDIR)/utils/,objstat file2obj elfsplit bsv appendobj sctx mkcap mkdlg makekey mkuser)
