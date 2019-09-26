$(BUILDDIR)/utils/file2obj: utils/file2obj.c
	@echo "[HOSTCC]  $@"
	@mkdir -p $(BUILDDIR)/utils
	@$(HOSTCC) $(HOSTCFLAGS) -Ius/include -o $@ $< -MD

$(BUILDDIR)/utils/appendobj: utils/appendobj.c
	@echo "[HOSTCC]  $@"
	@mkdir -p $(BUILDDIR)/utils
	@$(HOSTCC) $(HOSTCFLAGS) -Ius/include -o $@ $< -MD

$(BUILDDIR)/utils/objstat: utils/objstat.c utils/blake2.h utils/blake2.c
	@echo "[HOSTCC]  $@"
	@mkdir -p $(BUILDDIR)/utils
	@$(HOSTCC) $(HOSTCFLAGS) -Ius/include -o $@ utils/objstat.c utils/blake2.c -MD

$(BUILDDIR)/utils/elfsplit: utils/elfsplit.c
	@echo "[HOSTCC]  $@"
	@mkdir -p $(BUILDDIR)/utils
	@$(HOSTCC) $(HOSTCFLAGS) -Ius/include -o $@ $< -MD

$(BUILDDIR)/utils/bsv: utils/bsv.c
	@echo "[HOSTCC]  $@"
	@mkdir -p $(BUILDDIR)/utils
	@$(HOSTCC) $(HOSTCFLAGS) -Ius/include -o $@ $< -Ius/include -MD

$(BUILDDIR)/utils/sctx: utils/sctx.c
	@echo "[HOSTCC]  $@"
	@mkdir -p $(BUILDDIR)/utils
	@$(HOSTCC) $(HOSTCFLAGS) -Ius/include -o $@ $< -Ius/include -MD

$(BUILDDIR)/utils/mkcap: utils/mkcap.c
	@echo "[HOSTCC]  $@"
	@mkdir -p $(BUILDDIR)/utils
	@$(HOSTCC) $(HOSTCFLAGS) -Ius/include -o $@ utils/mkcap.c -Ius/include -ltomcrypt -MD

$(BUILDDIR)/utils/mkdlg: utils/mkdlg.c
	@echo "[HOSTCC]  $@"
	@mkdir -p $(BUILDDIR)/utils
	@$(HOSTCC) $(HOSTCFLAGS) -Ius/include -o $@ utils/mkdlg.c -Ius/include -ltomcrypt -MD

$(BUILDDIR)/utils/makekey: utils/makekey.c
	@echo "[HOSTCC]  $@"
	@mkdir -p $(BUILDDIR)/utils
	@$(HOSTCC) $(HOSTCFLAGS) -Ius/include -o $@ utils/makekey.c -Ius/include -MD

$(BUILDDIR)/utils/mkuser: utils/user.c
	@echo "[HOSTCC]  $@"
	@mkdir -p $(BUILDDIR)/utils
	@$(HOSTCC) $(HOSTCFLAGS) -Ius/include -o $@ utils/user.c -Ius/include -MD


$(BUILDDIR)/utils/hier: utils/hier.c
	@echo "[HOSTCC]  $@"
	@mkdir -p $(BUILDDIR)/utils
	@$(HOSTCC) $(HOSTCFLAGS) -Ius/include -o $@ utils/hier.c -Ius/include -MD


UTILS=$(addprefix $(BUILDDIR)/utils/,objstat file2obj elfsplit bsv appendobj sctx mkcap mkdlg makekey mkuser hier)

-include $(addsuffix .d,$(UTILS))
