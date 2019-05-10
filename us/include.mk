PROGS=test init
TWZCC?=x86_64-pc-elf-gcc


MUSL=musl-1.1.16

$(BUILDDIR)/us/musl-config.mk: $(BUILDDIR)/us/$(MUSL)/configure $(BUILDDIR)/us
	cd $(BUILDDIR)/us/$(MUSL) && ./configure --host=$(CONFIG_TRIPLET) CROSS_COMPILER=$(TOOLCHAIN_PREFIX)
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
	cp -a us/$(MUSL) $(BUILDDIR)/us
	TWZKROOT=$(shell pwd) TWZKBUILDDIR=$(BUILDDIR) CONFIGFILEPATH=../musl-config.mk $(MAKE) -C $(BUILDDIR)/us/$(MUSL)
	@touch $@

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
US_POSTLINK=$(BUILDDIR)/us/libtwz/libtwz.a $(MUSL_STATIC_LIBC) $(BUILDDIR)/us/twix/libtwix.a -Wl,--as-needed $(BUILDDIR)/us/libtwz/libtwz.a $(LIBGCC) $(CRTEND) $(MUSL_STATIC_LIBC_POST)
TWZCFLAGS=-Ius/include $(MUSL_INCL) -Wall -Wextra -O$(CONFIG_OPTIMIZE) -g -Ius/libtwz/include
US_LIBDEPS=$(BUILDDIR)/us/libtwz/libtwz.a $(BUILDDIR)/us/$(MUSL)/lib/libc.a $(BUILDDIR)/us/twix/libtwix.a us/elf.ld
US_LDFLAGS=-static -Wl,-z,max-page-size=0x1000 -Tus/elf.ld -g

include $(addprefix us/,$(addsuffix /include.mk,$(PROGS)))

include us/libtwz/include.mk
include us/twix/include.mk

all_progs: $(addsuffix _all,$(PROGS))

$(BUILDDIR)/us/%.flags: us/%.flags
	@-cp $< $@
	@echo "" > $@

$(BUILDDIR)/us/%.data.obj $(BUILDDIR)/us/%.text.obj: $(BUILDDIR)/us/% $(BUILDDIR)/us/%.flags $(UTILS)
	@echo [SPLIT] $<
	@$(BUILDDIR)/utils/elfsplit $<
	@echo [OBJ] $<.data.obj
	@$(BUILDDIR)/utils/file2obj -i $<.data -o $<.data.obj -p RD
	@echo [OBJ] $<.text.obj
	@DATAID=$$($(BUILDDIR)/utils/objstat -i $<.data.obj) ;\
	FLAGS=$$(cat $<.flags);\
	$(BUILDDIR)/utils/file2obj -i $< -o $<.text.obj -p RXD -f 1:RWD:$$DATAID $$FLAGS

$(BUILDDIR)/us/%.o: us/%.c $(MUSL_READY)
	mkdir -p $(dir $@)
	$(TWZCC) $(TWZCFLAGS) -c -o $@ $<

TWZOBJS=$(addprefix $(BUILDDIR)/us/,$(addsuffix .text.obj,$(foreach x,$(PROGS),$(x)/$(x))))
TWZOBJS+=$(addprefix $(BUILDDIR)/us/,$(addsuffix .data.obj,$(foreach x,$(PROGS),$(x)/$(x))))

$(BUILDDIR)/us/root.tar: $(TWZOBJS)
	tar cf $@ '--transform=s/projects.*\/us\/.*\//\/bin\//g' $(TWZOBJS)

userspace: $(BUILDDIR)/us/root.tar
