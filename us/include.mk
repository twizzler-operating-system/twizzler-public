TWZCC?=x86_64-pc-twizzler-musl-gcc
TWZCXX?=x86_64-pc-twizzler-musl-g++
#TWZCC=x86_64-pc-elf-gcc
TWIZZLER_TRIPLET=x86_64-pc-twizzler-musl
# TODO: move the above somewhere non-project-specific

MUSL=musl-1.1.16

SYSROOT_FILES=

$(BUILDDIR)/us/musl-config.mk: $(BUILDDIR)/us/$(MUSL)/configure
	cd $(BUILDDIR)/us/$(MUSL) && ./configure --host=x86_64-pc-twizzler-musl CROSS_COMPILER=x86_64-pc-twizzler-musl- --prefix=/usr --syslibdir=/lib --enable-debug --enable-optimize
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
	@echo "[INSTL]   musl-libs-bootstrap"
	@mkdir -p $(BUILDDIR)/us
	@mkdir -p $(BUILDDIR)/us/sysroot
	@TWZKROOT=$(shell pwd) TWZKBUILDDIR=$(BUILDDIR) CONFIGFILEPATH=../musl-config.mk $(MAKE) -C $(BUILDDIR)/us/$(MUSL) lib/libc.a
	@touch $@

$(MUSL_HDRS): $(BUILDDIR)/us/musl-config.mk $(MUSL_SRCS)
	@echo "[INSTL]   musl-headers"
	@TWZKROOT=$(shell pwd) TWZKBUILDDIR=$(BUILDDIR) CONFIGFILEPATH=../musl-config.mk $(MAKE) -C $(BUILDDIR)/us/$(MUSL) $(MUSL_H_GEN)
	@TWZKROOT=$(shell pwd) TWZKBUILDDIR=$(BUILDDIR) CONFIGFILEPATH=../musl-config.mk $(MAKE) -C $(BUILDDIR)/us/$(MUSL) install-headers DESTDIR=$(shell pwd)/$(BUILDDIR)/us/sysroot
	-@cd $(BUILDDIR)/us/sysroot/usr/include && [ ! -e twz ] && ln -s ../../../../../../../us/include/twz twz
	@touch $@

$(BUILDDIR)/us/sysroot/usr/include/%.h: $(MUSL_HDRS)

$(BUILDDIR)/us/sysroot/usr/lib/libc.a: $(BUILDDIR)/us/$(MUSL)/lib/libc.a
	@echo "[INSTL]   musl-libraries"
	@mkdir -p $(BUILDDIR)/us/sysroot
	TWZKROOT=$(shell pwd) TWZKBUILDDIR=$(BUILDDIR) CONFIGFILEPATH=../musl-config.mk $(MAKE) -C $(BUILDDIR)/us/$(MUSL) $(shell pwd)/$(BUILDDIR)/us/sysroot/usr/lib/libc.a DESTDIR=$(shell pwd)/$(BUILDDIR)/us/sysroot
	@touch $(BUILDDIR)/us/sysroot/usr/lib/libc.a

$(BUILDDIR)/us/sysroot/usr/lib/crt1.o: $(MUSL_SRCS) $(MUSL_HDRS) $(BUILDDIR)/us/sysroot/usr/lib/libc.a $(BUILDDIR)/us/sysroot/usr/lib/libtwz.a $(BUILDDIR)/us/sysroot/usr/lib/libtwix.a
	@echo "[INSTL]   crt-libraries"
	@mkdir -p $(BUILDDIR)/us/sysroot
	@TWZKROOT=$(shell pwd) TWZKBUILDDIR=$(BUILDDIR) CONFIGFILEPATH=../musl-config.mk $(MAKE) -C $(BUILDDIR)/us/$(MUSL) install DESTDIR=$(shell pwd)/$(BUILDDIR)/us/sysroot
	@touch $(BUILDDIR)/us/sysroot/usr/lib/crt1.o
	@-rm $(BUILDDIR)/us/sysroot/lib/ld64.so.1
	@ln -s ../usr/lib/libc.so $(BUILDDIR)/us/sysroot/lib/ld64.so.1
	@cp -a $(TOOLCHAIN_PATH)/x86_64-pc-twizzler-musl/lib/libgcc_s.so $(BUILDDIR)/us/sysroot/usr/lib
	@cp -a $(TOOLCHAIN_PATH)/x86_64-pc-twizzler-musl/lib/libgcc_s.so.1 $(BUILDDIR)/us/sysroot/usr/lib
	@rm $(BUILDDIR)/us/sysroot/lib/ld-musl-x86_64.so.1

$(BUILDDIR)/us/sysroot/lib/ld64.so:
	@ln -s ../usr/lib/libc.so $(BUILDDIR)/us/sysroot/lib/ld64.so


$(BUILDDIR)/us/sysroot/lib/ld64.so.1:
	@ln -s ../usr/lib/libc.so $(BUILDDIR)/us/sysroot/lib/ld64.so.1

SYSROOT_READY=$(BUILDDIR)/us/sysroot/usr/lib/crt1.o

SYSROOT_PREP=$(MUSL_HDRS)

#TWZCFLAGS=-Wall -Wextra -O3 -msse2 -msse -mavx -march=native -ffast-math -g
TWZCFLAGS=-Wall -Wextra -O3 -g -march=native -mclflushopt #-mclwb

include us/libtwz/include.mk
include us/twix/include.mk

all_progs: $(addsuffix _all,$(PROGS))

$(BUILDDIR)/us/sysroot/usr/lib/libtwz.a: $(BUILDDIR)/us/libtwz/libtwz.a
	@mkdir -p $(BUILDDIR)/us/sysroot/usr/lib
	@cp $< $@

$(BUILDDIR)/us/sysroot/usr/lib/libtwz.so: $(BUILDDIR)/us/libtwz/libtwz.so
	@mkdir -p $(BUILDDIR)/us/sysroot/usr/lib
	@cp $< $@

$(BUILDDIR)/us/sysroot/usr/lib/libtwix.a: $(BUILDDIR)/us/twix/libtwix.a
	@mkdir -p $(BUILDDIR)/us/sysroot/usr/lib
	@cp $< $@

$(BUILDDIR)/us/sysroot/usr/lib/libtwix.so: $(BUILDDIR)/us/twix/libtwix.so
	@mkdir -p $(BUILDDIR)/us/sysroot/usr/lib
	@cp $< $@

$(BUILDDIR)/us/sysroot/usr/lib/libc.so: $(BUILDDIR)/us/twix/libtwix.a $(BUILDDIR)/us/libtwz/libtwz.a
	@echo "[INSTL]   libc.so"
	@mkdir -p $(BUILDDIR)/us/sysroot
	@TWZKROOT=$(shell pwd) TWZKBUILDDIR=$(BUILDDIR) CONFIGFILEPATH=../musl-config.mk $(MAKE) -C $(BUILDDIR)/us/$(MUSL) $(shell pwd)/$(BUILDDIR)/us/sysroot/usr/lib/libc.so DESTDIR=$(shell pwd)/$(BUILDDIR)/us/sysroot
	@touch $(BUILDDIR)/us/sysroot/usr/lib/libc.so


SYSLIBS=$(BUILDDIR)/us/sysroot/usr/lib/libtwz.a $(BUILDDIR)/us/sysroot/usr/lib/libtwz.so $(BUILDDIR)/us/sysroot/usr/lib/libtwix.a $(BUILDDIR)/us/sysroot/usr/lib/libc.a $(BUILDDIR)/us/sysroot/usr/lib/libc.so

-include $(BUILDDIR)/us/*/*.d

include us/bin/include.mk
include us/drivers/include.mk
include us/playground/include.mk

$(BUILDDIR)/us/kc: $(BUILDDIR)/us/root-tmp.tar
	@echo "init=$$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/objroot/usr_bin_init_bootstrap.obj)" >> $(BUILDDIR)/us/kc
	@echo "name=$$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/objroot/__ns)" >> $(BUILDDIR)/us/kc

$(BUILDDIR)/us/kc-initrd: $(BUILDDIR)/us/initrd-tmp.tar
	@echo "init=$$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/initrdroot/usr_bin_init_bootstrap.obj)" >> $(BUILDDIR)/us/kc-initrd
	@echo "name=$$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/initrdroot/__ns)" >> $(BUILDDIR)/us/kc-initrd

include us/users.mk

$(BUILDDIR)/us/objroot/__ns: $(shell find $(BUILDDIR)/us/sysroot) $(SYSROOT_FILES) $(KEYOBJS) $(BUILDDIR)/us/opt-objroot/__ns
	@echo "[GEN]     objroot"
	@mv $(BUILDDIR)/us/sysroot/usr/share/terminfo/l/linux $(BUILDDIR)/us/sysroot/linux
	@rm -rf $(BUILDDIR)/us/sysroot/usr/share/doc
	@rm -rf $(BUILDDIR)/us/sysroot/usr/share/info
	@rm -rf $(BUILDDIR)/us/sysroot/usr/share/man
	@rm -rf $(BUILDDIR)/us/sysroot/usr/share/terminfo
	@mkdir -p $(BUILDDIR)/us/sysroot/usr/share/terminfo/l
	@mv $(BUILDDIR)/us/sysroot/linux $(BUILDDIR)/us/sysroot/usr/share/terminfo/l/linux
	@-rm -r $(BUILDDIR)/us/objroot
	@mkdir -p $(BUILDDIR)/us/objroot
	@export PROJECT=$(PROJECT) && ./us/gen_root.sh | ./us/gen_root.py projects/x86_64/build/us/objroot/ | ./us/append_ns.sh >/dev/null
	@ID=$$(projects/x86_64/build/utils/objstat -i $(BUILDDIR)/us/opt-objroot/__ns);\
	echo "d $$ID opt" | $(BUILDDIR)/utils/hier -A | $(BUILDDIR)/utils/appendobj $(BUILDDIR)/us/objroot/__ns

$(BUILDDIR)/us/opt-objroot/__ns: $(shell find $(BUILDDIR)/us/opt-sysroot)
	@echo "[GEN]     opt-objroot"
	@-rm -r $(BUILDDIR)/us/opt-objroot
	@mkdir -p $(BUILDDIR)/us/opt-objroot
	@export PROJECT=$(PROJECT) && ./us/gen_root_simple.sh $(BUILDDIR)/us/opt-sysroot/ $(BUILDDIR)/us/opt-objroot | ./us/gen_root.py $(BUILDDIR)/us/opt-objroot | ./us/append_ns_simple.sh $(BUILDDIR)/us/opt-objroot >/dev/null



$(BUILDDIR)/us/nvme.img: $(BUILDDIR)/us/objroot/__ns $(CTXOBJS) $(UTILS) $(BUILDDIR)/us/opt-objroot/__ns
	@echo "[MKIMG]   $@"
	@ID=$$(projects/x86_64/build/utils/objstat -i $(BUILDDIR)/us/objroot/__ns);\
	./projects/x86_64/build/utils/mkimg $(BUILDDIR)/us/objroot $(BUILDDIR)/us/opt-objroot -o $@ -n $$ID


INITRD_FILES=usr/bin/init usr/bin/init_bootstrap usr/bin/init_bootstrap.data usr/lib/libtwz.so usr/lib/libtwix.so usr/lib/libc.so lib/ld64.so lib/ld64.so.1 usr/lib/libgcc_s.so usr/lib/libgcc_s.so.1 usr/lib/libstdc++.so usr/lib/libstdc++.so.6 usr/lib/libstdc++.so.6.0.27 usr/bin/pager usr/bin/nvme usr/bin/input usr/bin/keyboard usr/bin/serial usr/bin/twzdev usr/lib/libbacktrace.so usr/lib/libbacktrace.so.0 usr/lib/libbacktrace.so.0.0.0
$(BUILDDIR)/us/initrdroot/__ns: $(KEYOBJS) $(addprefix $(BUILDDIR)/us/sysroot/,$(INITRD_FILES)) us/include.mk
	@echo "[GEN]     initrdroot"
	@-rm -r $(BUILDDIR)/us/initrdfiles
	@mkdir -p $(BUILDDIR)/us/initrdfiles
	@for i in $(INITRD_FILES); do \
		mkdir -p $$(dirname $(BUILDDIR)/us/initrdfiles/$$i);\
		cp -a $(BUILDDIR)/us/sysroot/$$i $(BUILDDIR)/us/initrdfiles/$$i;\
	done
	@-rm -r $(BUILDDIR)/us/initrdroot
	@mkdir -p $(BUILDDIR)/us/initrdroot
	@export PROJECT=$(PROJECT) && ./us/gen_root_simple.sh $(BUILDDIR)/us/initrdfiles/ $(BUILDDIR)/us/initrdroot | ./us/gen_root.py $(BUILDDIR)/us/initrdroot | ./us/append_ns_simple.sh $(BUILDDIR)/us/initrdroot > /dev/null



$(BUILDDIR)/us/initrd-tmp.tar: $(BUILDDIR)/us/initrdroot/__ns $(CTXOBJS) $(UTILS)
	@mkdir -p $(BUILDDIR)/us/initrd-objroot
	@for i in $(CTXOBJS); do \
		ID=$$($(BUILDDIR)/utils/objstat -i $$i) ;\
		ln $$i $(BUILDDIR)/us/initrdroot/$$ID ;\
		echo "r $$ID $$(basename -s .obj $$i)" | $(BUILDDIR)/utils/hier -A | $(BUILDDIR)/utils/appendobj $(BUILDDIR)/us/initrdroot/__ns;\
	done
	@echo "[TAR]     $@"
	@tar cf $(BUILDDIR)/us/initrd-tmp.tar -C $(BUILDDIR)/us/initrdroot --exclude='__ns*' --exclude='*.obj' --xform s:'./':: .

$(BUILDDIR)/us/initrd.tar: $(BUILDDIR)/us/initrd-tmp.tar $(BUILDDIR)/us/kc-initrd
	@cp $< $@
	@echo "[TAR]     $@"
	@tar rf $@ -C $(BUILDDIR)/us kc-initrd
	@tar rf $@ -C $(BUILDDIR)/us/keyroot --exclude='*.obj' --xform s:'./':: .

$(BUILDDIR)/us/root-tmp.tar: $(BUILDDIR)/us/objroot/__ns $(CTXOBJS) $(UTILS)
	@for i in $(CTXOBJS); do \
		ID=$$($(BUILDDIR)/utils/objstat -i $$i) ;\
		ln $$i $(BUILDDIR)/us/objroot/$$ID ;\
		echo "r $$ID $$(basename -s .obj $$i)" | $(BUILDDIR)/utils/hier -A | $(BUILDDIR)/utils/appendobj $(BUILDDIR)/us/objroot/__ns;\
	done
	@echo "[TAR]     $@"
	@tar cf $(BUILDDIR)/us/root-tmp.tar -C $(BUILDDIR)/us/objroot --exclude='__ns*' --exclude='*.obj' --xform s:'./':: .

$(BUILDDIR)/us/root.tar: $(BUILDDIR)/us/root-tmp.tar $(BUILDDIR)/us/kc
	@cp $< $@
	@echo "[TAR]     $@"
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
	@echo "[TAR]     $@"
	@tar cf $(BUILDDIR)/us/root.tar -C $(BUILDDIR)/us/root --xform s:'./':: .

userspace: $(BUILDDIR)/us/root.tar $(BUILDDIR)/us/initrd.tar


