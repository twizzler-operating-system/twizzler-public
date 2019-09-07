

USEROBJS=bob.sctx.obj bob-key.pub.obj bob-key.pri.obj bob.user.obj bob.kring.obj
USEROBJS+=login.sctx.obj login-key.pub.obj login-key.pri.obj
USEROBJS+=init.sctx.obj init-key.pub.obj init-key.pri.obj

.PRECIOUS: $(BUILDDIR)/us/data/%.pem $(BUILDDIR)/us/data/%.obj $(BUILDDIR)/us/data/%.user $(BUILDDIR)/us/data/%.pub $(BUILDDIR)/us/data/%.pri

.SECONDARY: $(BUILDDIR)/us/data/%.pem $(BUILDDIR)/us/data/%.obj $(BUILDDIR)/us/data/%.user $(BUILDDIR)/us/data/%.pub $(BUILDDIR)/us/data/%.pri

$(BUILDDIR)/us/data/dsaparam.pem:
	@echo "[DSAp] $@"
	@mkdir -p $(BUILDDIR)/us/data
	@openssl dsaparam -out $@ 2048

$(BUILDDIR)/us/data/%-rk.pem: $(BUILDDIR)/us/data/dsaparam.pem
	@echo "[DSA] $@"
	@mkdir -p $(BUILDDIR)/us/data
	@openssl gendsa -out $@ $<

$(BUILDDIR)/us/data/%.kring $(BUILDDIR)/us/data/%.user: $(BUILDDIR)/utils/mkuser $(BUILDDIR)/us/data/%.sctx.obj
	@echo "[MKUSER] $*"
	@mkdir -p $(BUILDDIR)/us/data
	@$(BUILDDIR)/utils/mkuser -u $(BUILDDIR)/us/data/$*.user -r $(BUILDDIR)/us/data/$*.kring -s $$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/data/$*.sctx.obj) $*

$(BUILDDIR)/us/data/%.kring.obj: $(BUILDDIR)/utils/file2obj $(BUILDDIR)/us/data/%.kring $(BUILDDIR)/us/data/%-key.pub.obj $(BUILDDIR)/us/data/%-key.pri.obj
	@echo "[OBJ] $@"
	@mkdir -p $(BUILDDIR)/us/data
	$(BUILDDIR)/utils/file2obj -i $(BUILDDIR)/us/data/$*.kring -o $@ -p R -f 1:r:$$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/data/$*-key.pri.obj) -f 2:r:$$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/data/$*-key.pub.obj)

$(BUILDDIR)/us/data/%.user.obj: $(BUILDDIR)/utils/file2obj $(BUILDDIR)/us/data/%.user $(BUILDDIR)/us/data/%.kring.obj
	@echo "[OBJ] $@"
	@mkdir -p $(BUILDDIR)/us/data
	$(BUILDDIR)/utils/file2obj -i $(BUILDDIR)/us/data/$*.user -o $@ -p R -f 1:rw:$$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/data/$*.kring.obj)

$(BUILDDIR)/us/data/%.sctx: $(BUILDDIR)/utils/file2obj $(BUILDDIR)/us/data/%-key.pub.obj
	@echo "[OBJ] $@"
	@mkdir -p $(BUILDDIR)/us/data
	$(BUILDDIR)/utils/file2obj -i /dev/null -o $@ -p R -k $$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/data/$*-key.pub.obj)

$(BUILDDIR)/us/data/%.sctx.obj: $(BUILDDIR)/us/data/%.sctx $(BUILDDIR)/utils/appendobj $(BUILDDIR)/us/data/%.sctx.tmp
	@echo "[OBJ] $@"
	@mkdir -p $(BUILDDIR)/us/data
	@cp $(BUILDDIR)/us/data/$*.sctx.tmp $@
	@$(BUILDDIR)/utils/appendobj $@ < $(BUILDDIR)/us/data/$*.sctx


$(BUILDDIR)/us/data/%.sctx.tmp: $(BUILDDIR)/utils/file2obj $(BUILDDIR)/us/data/%-key.pub.obj
	@echo "[OBJ] $@"
	@mkdir -p $(BUILDDIR)/us/data
	@$(BUILDDIR)/utils/file2obj -i /dev/null -o $@ -p R -k $$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/data/$*-key.pub.obj)

$(BUILDDIR)/us/data/bob.sctx: $(BUILDDIR)/us/data/bob.sctx.tmp $(BUILDDIR)/utils/sctx $(BUILDDIR)/utils/mkcap $(BUILDDIR)/utils/appendobj $(BUILDDIR)/us/data/bob-rk.pem
	@echo "[SCTX] $@"
	@mkdir -p $(BUILDDIR)/us/data
	LID=$$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/data/bob.sctx.tmp);\
	$(BUILDDIR)/utils/sctx -n "bob" $@ < /dev/null



$(BUILDDIR)/us/data/login.sctx: $(BUILDDIR)/us/data/login.sctx.tmp $(BUILDDIR)/us/data/bob.sctx.obj $(BUILDDIR)/utils/sctx $(BUILDDIR)/utils/mkcap $(BUILDDIR)/utils/appendobj $(BUILDDIR)/us/data/login-rk.pem $(BUILDDIR)/us/twzutils/login.text.obj $(BUILDDIR)/us/data/bob-rk.pem
	@echo "[SCTX] $@"
	@mkdir -p $(BUILDDIR)/us/data
	LID=$$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/data/login.sctx.tmp);\
	( \
		$(BUILDDIR)/utils/mkcap -t $$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/data/bob.sctx.obj) -a $$LID -p RU -h sha1 -s dsa $(BUILDDIR)/us/data/bob-rk.pem &&\
		$(BUILDDIR)/utils/mkcap -t $$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/twzutils/login.text.obj) -a $$LID -p RX -h sha1 -s dsa $(BUILDDIR)/us/data/login-rk.pem \
		) | $(BUILDDIR)/utils/sctx -n "login" $@

$(BUILDDIR)/us/data/init.sctx: $(BUILDDIR)/us/data/init.sctx.tmp $(BUILDDIR)/us/data/login.sctx.obj $(BUILDDIR)/utils/sctx $(BUILDDIR)/utils/mkcap $(BUILDDIR)/utils/appendobj $(BUILDDIR)/us/data/login-rk.pem $(BUILDDIR)/us/data/init-rk.pem $(BUILDDIR)/us/twzutils/init.text.obj
	@echo "[SCTX] $@"
	@mkdir -p $(BUILDDIR)/us/data
	LID=$$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/data/init.sctx.tmp);\
	( \
		$(BUILDDIR)/utils/mkcap -t $$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/data/login.sctx.obj) -a $$LID -p RU -h sha1 -s dsa $(BUILDDIR)/us/data/login-rk.pem &&\
		$(BUILDDIR)/utils/mkcap -t $$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/twzutils/init.text.obj) -a $$LID -p RX -h sha1 -s dsa $(BUILDDIR)/us/data/init-rk.pem \
		) | $(BUILDDIR)/utils/sctx -n "init" $@


$(BUILDDIR)/us/data/%-key.pub $(BUILDDIR)/us/data/%-key.pri: $(BUILDDIR)/us/data/%-rk.pem
	@echo "[KEY] $@"
	@mkdir -p $(BUILDDIR)/us/data
	@$(BUILDDIR)/utils/makekey -i $< -r $(BUILDDIR)/us/data/$*-key.pri -u $(BUILDDIR)/us/data/$*-key.pub -t dsa

$(BUILDDIR)/us/data/%-key.pub.obj: $(BUILDDIR)/us/data/%-key.pub
	@echo "[OBJ] $@"
	@mkdir -p $(BUILDDIR)/us/data
	$(BUILDDIR)/utils/file2obj -i $< -o $@ -p R

$(BUILDDIR)/us/data/%-key.pri.obj: $(BUILDDIR)/us/data/%-key.pri
	@echo "[OBJ] $@"
	@mkdir -p $(BUILDDIR)/us/data
	@$(BUILDDIR)/utils/file2obj -i $< -o $@

$(BUILDDIR)/us/twzutils/login.data.obj $(BUILDDIR)/us/twzutils/login.text.obj: $(BUILDDIR)/us/twzutils/login $(UTILS) $(BUILDDIR)/us/data/login-key.pub.obj
	@echo [SPLIT] $<
	@$(BUILDDIR)/utils/elfsplit $<
	@echo [OBJ] $<.data.obj
	@$(BUILDDIR)/utils/file2obj -i $<.data -o $<.data.obj -p RH
	@echo [OBJ] $<.text.obj
	@DATAID=$$($(BUILDDIR)/utils/objstat -i $<.data.obj) ;\
	$(BUILDDIR)/utils/file2obj -i $<.text -o $<.text.obj -p HR -k $$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/data/login-key.pub.obj) -f 1:RWD:$$DATAID

$(BUILDDIR)/us/twzutils/init.data.obj $(BUILDDIR)/us/twzutils/init.text.obj: $(BUILDDIR)/us/twzutils/init $(UTILS) $(BUILDDIR)/us/data/init-key.pub.obj
	@echo [SPLIT] $<
	@$(BUILDDIR)/utils/elfsplit $<
	@echo [OBJ] $<.data.obj
	@$(BUILDDIR)/utils/file2obj -i $<.data -o $<.data.obj -p RH
	@echo [OBJ] $<.text.obj
	@DATAID=$$($(BUILDDIR)/utils/objstat -i $<.data.obj) ;\
	$(BUILDDIR)/utils/file2obj -i $<.text -o $<.text.obj -p HR -k $$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/data/init-key.pub.obj) -f 1:RWD:$$DATAID






TWZOBJS+=$(addprefix $(BUILDDIR)/us/data/,$(USEROBJS))
