TWZCC?=x86_64-pc-twizzler-musl-gcc
TWZCXX?=x86_64-pc-twizzler-musl-g++
#TWZCC=x86_64-pc-elf-gcc
TWIZZLER_TRIPLET=x86_64-pc-twizzler-musl
# TODO: move the above somewhere non-project-specific

MUSL=musl-1.1.16

SYSROOT_FILES=

$(BUILDDIR)/us/musl-config.mk: $(BUILDDIR)/us/$(MUSL)/configure
	cd $(BUILDDIR)/us/$(MUSL) && ./configure --host=x86_64-pc-twizzler-musl CROSS_COMPILER=x86_64-pc-twizzler-musl- --prefix=/usr --syslibdir=/lib
	mv $(BUILDDIR)/us/$(MUSL)/config.mak $@

$(BUILDDIR)/us/musl-config-bootstrap.mk: $(BUILDDIR)/us/$(MUSL)/configure
	cd $(BUILDDIR)/us/$(MUSL) && ./configure --host=x86_64-pc-elf CROSS_COMPILER=x86_64-pc-elf- --prefix=/usr --syslibdir=/lib
	mv $(BUILDDIR)/us/$(MUSL)/config.mak $@

MUSL_SRCS=$(shell find us/$(MUSL))

MUSL_HDRS=$(BUILDDIR)/us/sysroot/usr/include/string.h

MUSL_H_GEN=obj/include/bits/alltypes.h obj/include/bits/syscall.h

bootstrap-musl: $(BUILDDIR)/us/musl-config-bootstrap.mk
	TWZKROOT=$(shell pwd) TWZKBUILDDIR=$(BUILDDIR) CONFIGFILEPATH=../musl-config-bootstrap.mk $(MAKE) -C $(BUILDDIR)/us/$(MUSL) $(MUSL_H_GEN)
	TWZKROOT=$(shell pwd) TWZKBUILDDIR=$(BUILDDIR) CONFIGFILEPATH=../musl-config-bootstrap.mk $(MAKE) -C $(BUILDDIR)/us/$(MUSL) install-headers DESTDIR=$(shell pwd)/$(BUILDDIR)/us/sysroot
	-@cd $(BUILDDIR)/us/sysroot/usr/include && [ ! -e twz ] && ln -s ../../../../../../../us/include/twz twz

clean-musl:
	-rm -r $(BUILDDIR)/us/$(MUSL)
	-rm -r $(BUILDDIR)/us/sysroot
	-rm us/$(MUSL)/config.mak

$(BUILDDIR)/us/$(MUSL)/configure: $(MUSL_SRCS)
	@mkdir -p $(BUILDDIR)/us
	cp -a us/$(MUSL) $(BUILDDIR)/us
	@touch $@

$(BUILDDIR)/us/$(MUSL)/lib/libc.a: $(BUILDDIR)/us/musl-config.mk
	@mkdir -p $(BUILDDIR)/us
	@mkdir -p $(BUILDDIR)/us/sysroot
	TWZKROOT=$(shell pwd) TWZKBUILDDIR=$(BUILDDIR) CONFIGFILEPATH=../musl-config.mk $(MAKE) -C $(BUILDDIR)/us/$(MUSL) lib/libc.a
	@touch $@

$(MUSL_HDRS): $(BUILDDIR)/us/musl-config.mk $(MUSL_SRCS)
	TWZKROOT=$(shell pwd) TWZKBUILDDIR=$(BUILDDIR) CONFIGFILEPATH=../musl-config.mk $(MAKE) -C $(BUILDDIR)/us/$(MUSL) $(MUSL_H_GEN)
	TWZKROOT=$(shell pwd) TWZKBUILDDIR=$(BUILDDIR) CONFIGFILEPATH=../musl-config.mk $(MAKE) -C $(BUILDDIR)/us/$(MUSL) install-headers DESTDIR=$(shell pwd)/$(BUILDDIR)/us/sysroot
	-@cd $(BUILDDIR)/us/sysroot/usr/include && [ ! -e twz ] && ln -s ../../../../../../../us/include/twz twz
	@touch $@

$(BUILDDIR)/us/sysroot/usr/include/%.h: $(MUSL_HDRS)

$(BUILDDIR)/us/sysroot/usr/lib/libc.a: $(BUILDDIR)/us/$(MUSL)/lib/libc.a
	@mkdir -p $(BUILDDIR)/us/sysroot
	TWZKROOT=$(shell pwd) TWZKBUILDDIR=$(BUILDDIR) CONFIGFILEPATH=../musl-config.mk $(MAKE) -C $(BUILDDIR)/us/$(MUSL) $(shell pwd)/$(BUILDDIR)/us/sysroot/usr/lib/libc.a DESTDIR=$(shell pwd)/$(BUILDDIR)/us/sysroot
	@touch $(BUILDDIR)/us/sysroot/usr/lib/libc.a

$(BUILDDIR)/us/sysroot/usr/lib/crt1.o: $(MUSL_SRCS) $(MUSL_HDRS) $(BUILDDIR)/us/sysroot/usr/lib/libc.a $(BUILDDIR)/us/sysroot/usr/lib/libtwz.a $(BUILDDIR)/us/sysroot/usr/lib/libtwix.a
	@mkdir -p $(BUILDDIR)/us/sysroot
	TWZKROOT=$(shell pwd) TWZKBUILDDIR=$(BUILDDIR) CONFIGFILEPATH=../musl-config.mk $(MAKE) -C $(BUILDDIR)/us/$(MUSL) install DESTDIR=$(shell pwd)/$(BUILDDIR)/us/sysroot
	@touch $(BUILDDIR)/us/sysroot/usr/lib/crt1.o


SYSROOT_READY=$(BUILDDIR)/us/sysroot/usr/lib/crt1.o

SYSROOT_PREP=$(MUSL_HDRS)

#TWZCFLAGS=-Wall -Wextra -O3 -msse2 -msse -mavx -march=native -ffast-math -g
TWZCFLAGS=-Wall -Wextra -Og -g

include us/libtwz/include.mk
include us/twix/include.mk

all_progs: $(addsuffix _all,$(PROGS))

#$(BUILDDIR)/us/%.flags: us/%.flags
#	@-cp $< $@
#	@echo "" > $@

#$(BUILDDIR)/us/twzutils/%.data.obj $(BUILDDIR)/us/twzutils/%.text.obj: $(BUILDDIR)/us/twzutils/% $(UTILS)
#	@echo [split] $<
#	@$(BUILDDIR)/utils/elfsplit $<
#	@echo [obj] $<.data.obj
#	@$(BUILDDIR)/utils/file2obj -i $<.data -o $<.data.obj -p rh
#	@echo [obj] $<.text.obj
#	@dataid=$$($(BUILDDIR)/utils/objstat -i $<.data.obj) ;\
#	$(BUILDDIR)/utils/file2obj -i $<.text -o $<.text.obj -p rxh -f 1:rwd:$$dataid

#$(BUILDDIR)/us/libtwz/libtwz.so.data.obj $(BUILDDIR)/us/libtwz/libtwz.so.text.obj: $(BUILDDIR)/us/libtwz/libtwz.so $(BUILDDIR)/us/libtwz/libtwz.so.flags $(UTILS)
#	@echo [SPLIT] $<
#	@$(BUILDDIR)/utils/elfsplit $<
#	@echo [OBJ] $<.data.obj
#	@$(BUILDDIR)/utils/file2obj -i $<.data -o $<.data.obj -p RH
#	@echo [OBJ] $<.text.obj
#	@DATAID=$$($(BUILDDIR)/utils/objstat -i $<.data.obj) ;\
#	FLAGS=$$(cat $<.flags);\
#	$(BUILDDIR)/utils/file2obj -i $<.text -o $<.text.obj -p RXH -f 1:RWD:$$DATAID $$FLAGS

$(BUILDDIR)/us/sysroot/usr/lib/libtwz.a: $(BUILDDIR)/us/libtwz/libtwz.a
	mkdir -p $(BUILDDIR)/us/sysroot/usr/lib
	cp $< $@

$(BUILDDIR)/us/sysroot/usr/lib/libtwz.so: $(BUILDDIR)/us/libtwz/libtwz.so
	mkdir -p $(BUILDDIR)/us/sysroot/usr/lib
	cp $< $@

$(BUILDDIR)/us/sysroot/usr/lib/libtwix.a: $(BUILDDIR)/us/twix/libtwix.a
	mkdir -p $(BUILDDIR)/us/sysroot/usr/lib
	cp $< $@

$(BUILDDIR)/us/sysroot/usr/lib/libtwix.so: $(BUILDDIR)/us/twix/libtwix.so
	mkdir -p $(BUILDDIR)/us/sysroot/usr/lib
	cp $< $@

SYSLIBS=$(BUILDDIR)/us/sysroot/usr/lib/libtwz.a $(BUILDDIR)/us/sysroot/usr/lib/libtwz.so $(BUILDDIR)/us/sysroot/usr/lib/libtwix.a $(BUILDDIR)/us/sysroot/usr/lib/libc.a

-include $(BUILDDIR)/us/*/*.d

include us/twzutils/include.mk

#TWZOBJS+=$(addprefix $(BUILDDIR)/us/,$(addsuffix .obj,$(foreach x,$(PROGS),twzutils/$(x))))
#TWZOBJS+=$(addprefix $(BUILDDIR)/us/,$(addsuffix .data.obj,$(foreach x,$(PROGS),twzutils/$(x))))

$(BUILDDIR)/us/kc: $(BUILDDIR)/us/root-tmp.tar
	@echo "init=$$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/objroot/usr_bin_init.obj)" >> $(BUILDDIR)/us/kc
	@echo "name=$$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/objroot/__ns)" >> $(BUILDDIR)/us/kc

#TWZOBJS+=$(BUILDDIR)/us/bsv.obj

include us/users.mk

#$(BUILDDIR)/us/sysroot/usr/bin/%.text.obj $(BUILDDIR)/us/sysroot/usr/bin/%.data.obj: $(BUILDDIR)/us/sysroot/usr/bin/% $(UTILS)
#	@echo [split] $<
#	@$(BUILDDIR)/utils/elfsplit $<
#	@echo [obj] $<.data.obj
#	@$(BUILDDIR)/utils/file2obj -i $<.data -o $<.data.obj -p rh
#	@echo [obj] $<.text.obj
#	@dataid=$$($(BUILDDIR)/utils/objstat -i $<.data.obj) ;\
#	$(BUILDDIR)/utils/file2obj -i $<.text -o $<.text.obj -p rxh -f 1:rwd:$$dataid


#TWZOBJS+=$(BUILDDIR)/us/sysroot/usr/bin/bash.text.obj
#TWZOBJS+=$(BUILDDIR)/us/sysroot/usr/bin/bash.data.obj

$(BUILDDIR)/us/objroot/__ns: $(shell find $(BUILDDIR)/us/sysroot) $(SYSROOT_FILES) $(KEYOBJS)
	@mv $(BUILDDIR)/us/sysroot/usr/share/terminfo/l/linux $(BUILDDIR)/us/sysroot/linux
	@rm -rf $(BUILDDIR)/us/sysroot/usr/share/doc
	@rm -rf $(BUILDDIR)/us/sysroot/usr/share/info
	@rm -rf $(BUILDDIR)/us/sysroot/usr/share/man
	@rm -rf $(BUILDDIR)/us/sysroot/usr/share/terminfo
	@mkdir -p $(BUILDDIR)/us/sysroot/usr/share/terminfo/l
	@mv $(BUILDDIR)/us/sysroot/linux $(BUILDDIR)/us/sysroot/usr/share/terminfo/l/linux
	export PROJECT=$(PROJECT) && ./us/gen_root.sh | ./us/gen_root.py projects/x86_64/build/us/objroot/ | ./us/append_ns.sh >/dev/null

$(BUILDDIR)/us/root-tmp.tar: $(BUILDDIR)/us/objroot/__ns $(CTXOBJS) $(UTILS)
	for i in $(CTXOBJS); do \
		echo $$i;\
		ID=$$($(BUILDDIR)/utils/objstat -i $$i) ;\
		ln $$i $(BUILDDIR)/us/objroot/$$ID ;\
		echo $i; \
		echo "r $$ID $$(basename -s .obj $$i)" | $(BUILDDIR)/utils/hier -A | $(BUILDDIR)/utils/appendobj $(BUILDDIR)/us/objroot/__ns;\
	done
	@echo [TAR] $@
	@tar cf $(BUILDDIR)/us/root-tmp.tar -C $(BUILDDIR)/us/objroot --exclude='__ns*' --exclude='*.obj' --xform s:'./':: .

$(BUILDDIR)/us/root.tar: $(BUILDDIR)/us/root-tmp.tar $(BUILDDIR)/us/kc
	@cp $< $@
	@echo [TAR] $@
	@tar rf $@ -C $(BUILDDIR)/us kc
	@tar rf $@ -C $(BUILDDIR)/us/keyroot --exclude='*.obj' --xform s:'./':: .

$(BUILDDIR)/us/root-old.tar: $(TWZOBJS) $(SYSLIBS)
	@-rm -r $(BUILDDIR)/us/root
	@mkdir -p $(BUILDDIR)/us/root
	@NAMES=;\
	LIST=;\
		for i in $(TWZOBJS); do \
		ID=$$($(BUILDDIR)/utils/objstat -i $$i);\
		NAMES="$$NAMES,$$(basename -s .obj $$i)=$$ID";\
		cp $$i $(BUILDDIR)/us/root/$$ID ;\
		done ;\
		echo -n $$NAMES > $(BUILDDIR)/us/names
	@if [[ -f $(BUILDDIR)/us/objroot/__ns ]]; then \
		echo ",__unix__root__=$$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/objroot/__ns)" >> $(BUILDDIR)/us/names ;\
	fi
	@$(BUILDDIR)/utils/file2obj -i $(BUILDDIR)/us/names -o $(BUILDDIR)/us/names.obj -p RWU
	@cp $(BUILDDIR)/us/names.obj $(BUILDDIR)/us/root/$$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/names.obj)
	@echo "init=$$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/twzutils/init.text.obj)" >> $(BUILDDIR)/us/kc
	@echo "name=$$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/names.obj)" >> $(BUILDDIR)/us/kc
	@cp $(BUILDDIR)/us/kc $(BUILDDIR)/us/root/kc
	@echo [TAR] $@
	@tar cf $(BUILDDIR)/us/root.tar -C $(BUILDDIR)/us/root --xform s:'./':: .

userspace: $(BUILDDIR)/us/root.tar


