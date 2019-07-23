















$(BUILDDIR)/us/data/dsaparam.pem:
	@echo "[DSAp] $@"
	@mkdir -p $(BUILDDIR)/us/data
	@openssl dsaparam -out $@ 2048

$(BUILDDIR)/us/data/login-rk.pem: $(BUILDDIR)/us/data/dsaparam.pem
	@echo "[DSA] $@"
	@mkdir -p $(BUILDDIR)/us/data
	@openssl gendsa -out $@ $<

$(BUILDDIR)/us/data/bob.sctx: $(BUILDDIR)/utils/file2obj
	@echo "[OBJ] $@"
	@mkdir -p $(BUILDDIR)/us/data
	@$(BUILDDIR)/utils/file2obj -i /dev/null -o $@ -p R

$(BUILDDIR)/us/data/bob.sctx.obj: $(BUILDDIR)/us/data/bob.sctx $(BUILDDIR)/utils/file2obj
	@echo "[OBJ] $@"
	@mkdir -p $(BUILDDIR)/us/data
	@$(BUILDDIR)/utils/file2obj -i $< -o $@ -p R

$(BUILDDIR)/us/data/login.sctx.obj: $(BUILDDIR)/us/data/login.sctx $(BUILDDIR)/utils/file2obj
	@echo "[OBJ] $@"
	@mkdir -p $(BUILDDIR)/us/data
	@$(BUILDDIR)/utils/file2obj -i $< -o $@ -p R

$(BUILDDIR)/us/data/_login.sctx.tmp: $(BUILDDIR)/utils/file2obj
	@echo "[OBJ] $@"
	@mkdir -p $(BUILDDIR)/us/data
	@$(BUILDDIR)/utils/file2obj -i /dev/null -o $@ -p R

$(BUILDDIR)/us/data/login.sctx: $(BUILDDIR)/us/data/_login.sctx.tmp $(BUILDDIR)/us/data/bob.sctx $(BUILDDIR)/utils/sctx $(BUILDDIR)/utils/mkcap $(BUILDDIR)/utils/appendobj $(BUILDDIR)/us/data/login-rk.pem
	@echo "[SCTX] $@"
	@mkdir -p $(BUILDDIR)/us/data
	LID=$$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/data/_login.sctx.tmp);\
	$(BUILDDIR)/utils/mkcap -t $$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/data/bob.sctx) -a $$LID -p RU -h sha1 -s dsa $(BUILDDIR)/us/data/login-rk.pem | $(BUILDDIR)/utils/sctx $@

