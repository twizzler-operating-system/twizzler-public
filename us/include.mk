PROGS=
#SUBDIRS=test init term shell login nls pcie input serial
TWZCC?=x86_64-pc-twizzler-musl-gcc
#TWZCC=x86_64-pc-elf-gcc

PROGS+=term

MUSL=musl-1.1.16

$(BUILDDIR)/us/musl-config.mk: $(BUILDDIR)/us/$(MUSL)/configure $(BUILDDIR)/us
	cd $(BUILDDIR)/us/$(MUSL) && ./configure --host=$(CONFIG_TRIPLET) CROSS_COMPILER=$(TOOLCHAIN_PREFIX) --prefix=/usr --syslibdir=/lib
	mv $(BUILDDIR)/us/$(MUSL)/config.mak $@

MUSL_H_GEN=obj/include/bits/alltypes.h obj/include/bits/syscall.h
musl-prep:
	@mkdir -p $(BUILDDIR)/us
	cp -a us/$(MUSL) $(BUILDDIR)/us
	$(MAKE) $(BUILDDIR)/us/musl-config.mk
	TWZKROOT=$(shell pwd) TWZKBUILDDIR=$(BUILDDIR) CONFIGFILEPATH=../musl-config.mk $(MAKE) -C $(BUILDDIR)/us/$(MUSL) $(MUSL_H_GEN)

MUSL_SRCS=$(shell find us/$(MUSL))

$(BUILDDIR)/us/$(MUSL)/lib/libc.a: $(MUSL_SRCS) $(BUILDDIR)/us/libtwz/libtwz.a $(BUILDDIR)/us/twix/libtwix.a $(MUSL_READY)
	@mkdir -p $(BUILDDIR)/us
	@mkdir -p $(BUILDDIR)/us/sysroot
	cp -a us/$(MUSL) $(BUILDDIR)/us
	TWZKROOT=$(shell pwd) TWZKBUILDDIR=$(BUILDDIR) CONFIGFILEPATH=../musl-config.mk $(MAKE) -C $(BUILDDIR)/us/$(MUSL)
	@touch $@

$(BUILDDIR)/us/sysroot/usr/include/%.h: $(BUILDDIR)/us/sysroot/usr/lib/libc.a

$(BUILDDIR)/us/sysroot/usr/lib/libc.a: $(BUILDDIR)/us/$(MUSL)/lib/libc.a
	@mkdir -p $(BUILDDIR)/us/sysroot
	TWZKROOT=$(shell pwd) TWZKBUILDDIR=$(BUILDDIR) CONFIGFILEPATH=../musl-config.mk $(MAKE) -C $(BUILDDIR)/us/$(MUSL) install DESTDIR=$(shell pwd)/$(BUILDDIR)/us/sysroot
	@touch $(BUILDDIR)/us/sysroot/usr/lib/libc.a
	@cd $(BUILDDIR)/us/sysroot/usr/include && [ ! -f twz ] && ln -s ../../../../../../../us/include//twz twz

.PHONY: sysroot-prep

sysroot-prep: $(BUILDDIR)/us/sysroot/usr/lib/libc.a

MUSL_INCL=$(addprefix -I$(BUILDDIR)/us/$(MUSL)/,include obj/include src/internal obj/src/internal arch/generic arch/$(ARCH))

$(BUILDDIR)/us/$(MUSL)/include/string.h:
	$(MAKE) musl-prep

MUSL_READY=$(BUILDDIR)/us/$(MUSL)/include/string.h

MUSL_STATIC_LIBC_PRE_i=$(BUILDDIR)/us/$(MUSL)/lib/crti.o
MUSL_STATIC_LIBC_PRE_1=$(BUILDDIR)/us/$(MUSL)/lib/crt1.o
MUSL_STATIC_LIBC=$(BUILDDIR)/us/$(MUSL)/lib/libc.a
MUSL_SHARED_LIBC=$(BUILDDIR)/us/$(MUSL)/lib/libc.so
MUSL_STATIC_LIBC_POST=$(BUILDDIR)/us/$(MUSL)/lib/crtn.o


LIBGCC=$(shell env PATH=$(PATH) $(TOOLCHAIN_PREFIX)gcc -print-libgcc-file-name)
CRTEND=$(shell env PATH=$(PATH) $(TOOLCHAIN_PREFIX)gcc -print-file-name=crtend.o)
CRTBEGIN=$(shell env PATH=$(PATH) $(TOOLCHAIN_PREFIX)gcc -print-file-name=crtbegin.o)

US_PRELINK=$(MUSL_STATIC_LIBC_PRE_i) $(CRTBEGIN) $(MUSL_STATIC_LIBC_PRE_1)
US_POSTLINK=-Wl,--start-group $(BUILDDIR)/us/libtwz/libtwz.a $(MUSL_STATIC_LIBC) $(BUILDDIR)/us/twix/libtwix.a -Wl,--as-needed $(BUILDDIR)/us/libtwz/libtwz.a -Wl,--end-group $(LIBGCC) $(CRTEND) $(MUSL_STATIC_LIBC_POST)
TWZCFLAGS=-Ius/include $(MUSL_INCL) -Wall -Wextra -O3 -msse2 -msse -mavx -march=native -ffast-math -g -Ius/libtwz/include
US_LIBDEPS=$(BUILDDIR)/us/libtwz/libtwz.a $(BUILDDIR)/us/$(MUSL)/lib/libc.a $(BUILDDIR)/us/twix/libtwix.a us/elf.ld
US_LDFLAGS=-static -Wl,-z,max-page-size=0x1000 -Tus/elf.ld -g

#include $(addprefix us/,$(addsuffix /include.mk,$(SUBDIRS)))

include us/libtwz/include.mk
include us/twix/include.mk

all_progs: $(addsuffix _all,$(PROGS))

$(BUILDDIR)/us/%.flags: us/%.flags
	@-cp $< $@
	@echo "" > $@

$(BUILDDIR)/us/twzutils/%.data.obj $(BUILDDIR)/us/twzutils/%.text.obj: $(BUILDDIR)/us/twzutils/% $(UTILS)
	@echo [SPLIT] $<
	@$(BUILDDIR)/utils/elfsplit $<
	@echo [OBJ] $<.data.obj
	@$(BUILDDIR)/utils/file2obj -i $<.data -o $<.data.obj -p RH
	@echo [OBJ] $<.text.obj
	@DATAID=$$($(BUILDDIR)/utils/objstat -i $<.data.obj) ;\
	$(BUILDDIR)/utils/file2obj -i $<.text -o $<.text.obj -p RXH -f 1:RWD:$$DATAID

$(BUILDDIR)/us/libtwz/libtwz.so.data.obj $(BUILDDIR)/us/libtwz/libtwz.so.text.obj: $(BUILDDIR)/us/libtwz/libtwz.so $(BUILDDIR)/us/libtwz/libtwz.so.flags $(UTILS)
	@echo [SPLIT] $<
	@$(BUILDDIR)/utils/elfsplit $<
	@echo [OBJ] $<.data.obj
	@$(BUILDDIR)/utils/file2obj -i $<.data -o $<.data.obj -p RH
	@echo [OBJ] $<.text.obj
	@DATAID=$$($(BUILDDIR)/utils/objstat -i $<.data.obj) ;\
	FLAGS=$$(cat $<.flags);\
	$(BUILDDIR)/utils/file2obj -i $<.text -o $<.text.obj -p RXH -f 1:RWD:$$DATAID $$FLAGS

$(BUILDDIR)/us/sysroot/usr/lib/libtwz.a: $(BUILDDIR)/us/libtwz/libtwz.a
	mkdir -p $(BUILDDIR)/us/sysroot/usr/lib
	cp $< $@

$(BUILDDIR)/us/sysroot/usr/lib/libtwz.so: $(BUILDDIR)/us/libtwz/libtwz.so
	mkdir -p $(BUILDDIR)/us/sysroot/usr/lib
	cp $< $@

$(BUILDDIR)/us/sysroot/usr/lib/libtwix.a: $(BUILDDIR)/us/twix/libtwix.a
	mkdir -p $(BUILDDIR)/us/sysroot/usr/lib
	cp $< $@

SYSLIBS=$(BUILDDIR)/us/sysroot/usr/lib/libtwz.a $(BUILDDIR)/us/sysroot/usr/lib/libtwz.so $(BUILDDIR)/us/sysroot/usr/lib/libtwix.a $(BUILDDIR)/us/sysroot/usr/lib/libc.a

#$(BUILDDIR)/us/%.o: us/%.c $(BUILDDIR)/us/sysroot/usr/lib/libc.a
#	@mkdir -p $(dir $@)
#	@echo "[CCC] $@"
#	$(TWZCC) -c -MD -MF $(BUILDDIR)/us/$*.d -o $@ $<

-include $(BUILDDIR)/us/*/*.d

include us/twzutils/include.mk

TWZOBJS=$(addprefix $(BUILDDIR)/us/,$(addsuffix .text.obj,$(foreach x,$(PROGS),twzutils/$(x))))

TWZOBJS+=$(addprefix $(BUILDDIR)/us/,$(addsuffix .data.obj,$(foreach x,$(PROGS),twzutils/$(x))))

#TWZOBJS+=$(BUILDDIR)/us/foo.text.obj $(BUILDDIR)/us/foo.data.obj
#TWZOBJS+=$(BUILDDIR)/us/bash.text.obj $(BUILDDIR)/us/bash.data.obj

$(BUILDDIR)/us/bsv.data: $(BUILDDIR)/us/twzutils/init.text.obj
	@echo "[BSV] $@"
	@$(BUILDDIR)/utils/bsv $@ 0,$$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/twzutils/init.text.obj),RX

$(BUILDDIR)/us/root/kc $(BUILDDIR)/us/bsv.obj: $(BUILDDIR)/us/bsv.data
	@echo "[OBJ] $(BUILDDIR)/us/bsv.obj"
	@$(BUILDDIR)/utils/file2obj -i $< -o $(BUILDDIR)/us/bsv.obj -p RWU
	@echo "bsv=$$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/bsv.obj)" > $(BUILDDIR)/us/kc

TWZOBJS+=$(BUILDDIR)/us/bsv.obj $(BUILDDIR)/us/libtwz/libtwz.so.text.obj $(BUILDDIR)/us/libtwz/libtwz.so.data.obj

TWZOBJS+=$(BUILDDIR)/us/inconsolata.sfn.obj

$(BUILDDIR)/us/inconsolata.sfn.obj: us/inconsolata.sfn $(BUILDDIR)/utils/file2obj
	@echo [OBJ] $@
	@$(BUILDDIR)/utils/file2obj -p rh -i $< -o $@

include us/users.mk

$(BUILDDIR)/us/root.tar: $(TWZOBJS) $(SYSLIBS)
	@-rm -r $(BUILDDIR)/us/root
	@mkdir -p $(BUILDDIR)/us/root
	@NAMES=;\
	LIST=;\
		for i in $(TWZOBJS); do \
		ID=$$($(BUILDDIR)/utils/objstat -i $$i);\
		NAMES="$$NAMES,$$(basename -s .obj $$i)=$$ID";\
		cp $$i $(BUILDDIR)/us/root/$$ID ;\
		done ;\
		echo $$NAMES > $(BUILDDIR)/us/names
	@$(BUILDDIR)/utils/file2obj -i $(BUILDDIR)/us/names -o $(BUILDDIR)/us/names.obj -p RWU
	@cp $(BUILDDIR)/us/names.obj $(BUILDDIR)/us/root/$$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/names.obj)
	@echo "init=$$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/twzutils/init.text.obj)" >> $(BUILDDIR)/us/kc
	@echo "name=$$($(BUILDDIR)/utils/objstat -i $(BUILDDIR)/us/names.obj)" >> $(BUILDDIR)/us/kc
	@cp $(BUILDDIR)/us/kc $(BUILDDIR)/us/root/kc
	@echo [TAR] $@
	@tar cf $(BUILDDIR)/us/root.tar -C $(BUILDDIR)/us/root --xform s:'./':: .

userspace: $(BUILDDIR)/us/root.tar
