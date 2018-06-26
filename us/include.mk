TWZOBJS=test.0

USFILES=$(addprefix $(BUILDDIR)/us/, $(TWZOBJS) $(addsuffix .meta,$(TWZOBJS)))

USCFLAGS=-static -Wl,-z,max-page-size=0x1000 -Tus/elf.ld -Wall -Os -g

$(BUILDDIR)/us/test: us/test.s us/elf.ld
	@echo "[AS]  $@"
	@$(TOOLCHAIN_PREFIX)gcc $(USCFLAGS) $< -o $@ -nostdlib

$(BUILDDIR)/us/test.0.meta: $(BUILDDIR)/us/test
$(BUILDDIR)/us/test.0: $(BUILDDIR)/us/test
	@echo "[PE]  $@"
	@$(TWZUTILSDIR)/postelf/postelf $(BUILDDIR)/us/test

$(BUILDDIR)/us:
	@mkdir -p $@

INITNAME=test.0

$(BUILDDIR)/us/root.tar: $(BUILDDIR)/us $(USFILES)
	@echo "[AGG] $(BUILDDIR)/us/root"
	@-rm -r $(BUILDDIR)/us/root 2>/dev/null
	@mkdir -p $(BUILDDIR)/us/root
	@-rm $(BUILDDIR)/us/__nameobj 2>/dev/null
	@-rm $(BUILDDIR)/us/__kc 2>/dev/null
	@for obj in $(TWZOBJS); do \
		id=$$($(TWZUTILSDIR)/objbuild/objstat $(BUILDDIR)/us/$$obj | grep COID | awk '{print $$3}');\
		cp $(BUILDDIR)/us/$$obj $(BUILDDIR)/us/root/$$id ;\
		cp $(BUILDDIR)/us/$$obj.meta $(BUILDDIR)/us/root/$$id.meta ;\
		echo "$$obj=$$id" >> $(BUILDDIR)/us/__nameobj ;\
		if [[ "$$obj" == "$(INITNAME)" ]]; then \
			echo "init=$$id" >> $(BUILDDIR)/us/__kc ;\
		fi;\
	done
	@echo "[OBJ] $(BUILDDIR)/us/nameobj"
	@echo "dat $(BUILDDIR)/us/__nameobj" | $(TWZUTILSDIR)/objbuild/objbuild -s -d r -o $(BUILDDIR)/us/nameobj -m none
	@id=$$($(TWZUTILSDIR)/objbuild/objstat $(BUILDDIR)/us/nameobj | grep COID | awk '{print $$3}');\
		cp $(BUILDDIR)/us/nameobj $(BUILDDIR)/us/root/$$id ;\
		cp $(BUILDDIR)/us/nameobj.meta $(BUILDDIR)/us/root/$$id.meta ;\
		echo "name=$$id" >> $(BUILDDIR)/us/__kc
	@cp $(BUILDDIR)/us/__kc $(BUILDDIR)/us/root/kc
	@echo "[TAR] $@"
	@-rm $(BUILDDIR)/us/root.tar 2>/dev/null
	@tar cf $(BUILDDIR)/us/root.tar -C $(BUILDDIR)/us/root --transform='s/\.\///g' .

userspace: .twizzlerutils $(BUILDDIR)/us/root.tar

