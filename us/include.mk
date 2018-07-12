TWZOBJS=bsv
MUSL=musl-1.1.16

USFILES=$(addprefix $(BUILDDIR)/us/, $(TWZOBJS) $(addsuffix .meta,$(TWZOBJS)))

US_LDFLAGS=-static -Wl,-z,max-page-size=0x1000 -Tus/elf.ld -g

$(BUILDDIR)/us:
	@mkdir -p $@


#musl

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

$(BUILDDIR)/us/$(MUSL)/lib/libc.a: $(MUSL_SRCS) $(BUILDDIR)/us/libtwz.a $(BUILDDIR)/us/twix/libtwix.a $(MUSL_READY)
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
MUSL_STATIC_LIBC_POST=$(BUILDDIR)/us/$(MUSL)/lib/crtn.o

#libtwz

LIBTWZ_SRC=$(addprefix us/libtwz/,notify.c bstream.c mutex.c twzio.c viewcall.c twzlog.c name.c corecall.c debug.c object.c blake2.c secctx.c thread.c fault.c kv.c)
LIBTWZ_OBJ=$(addprefix $(BUILDDIR)/,$(LIBTWZ_SRC:.c=.o))

$(BUILDDIR)/us/libtwz.a: $(LIBTWZ_OBJ)
	ar rcs $(BUILDDIR)/us/libtwz.a $(LIBTWZ_OBJ)

$(BUILDDIR)/us/libtwz/%.o: us/libtwz/%.c $(MUSL_READY)
	@mkdir -p $(BUILDDIR)/us/libtwz
	$(TOOLCHAIN_PREFIX)gcc -Og -g -Wall -Wextra -std=gnu11 -I us/include -ffreestanding $(MUSL_INCL) -c -o $@ $< -MD -Werror

-include $(LIBTWZ_OBJ:.o=.d)


LIBGCC=$(shell env PATH=$(PATH) $(TOOLCHAIN_PREFIX)gcc -print-libgcc-file-name)
CRTEND=$(shell env PATH=$(PATH) $(TOOLCHAIN_PREFIX)gcc -print-file-name=crtend.o)
CRTBEGIN=$(shell env PATH=$(PATH) $(TOOLCHAIN_PREFIX)gcc -print-file-name=crtbegin.o)

US_PRELINK=$(MUSL_STATIC_LIBC_PRE_i) $(CRTBEGIN) $(MUSL_STATIC_LIBC_PRE_1)
US_POSTLINK=$(BUILDDIR)/us/libtwz.a $(MUSL_STATIC_LIBC) $(BUILDDIR)/us/twix/libtwix.a -Wl,--as-needed $(BUILDDIR)/us/libtwz.a $(LIBGCC) $(CRTEND) $(MUSL_STATIC_LIBC_POST)
US_CFLAGS=-Ius/include $(MUSL_INCL) -Wall -Wextra -O$(CONFIG_OPTIMIZE) -g
US_LIBDEPS=$(BUILDDIR)/us/libtwz.a $(BUILDDIR)/us/$(MUSL)/lib/libc.a $(BUILDDIR)/us/twix/libtwix.a us/elf.ld


include us/init/include.mk

include us/term/include.mk

include us/shell/include.mk

include us/twix/include.mk


INITNAME=init/init.0

$(BUILDDIR)/us/bsv: $(BUILDDIR)/us/$(INITNAME)
	@id=$$($(TWZUTILSDIR)/objbuild/objstat $(BUILDDIR)/us/$(INITNAME) | grep COID | awk '{print $$3}');\
	$(TWZUTILSDIR)/bootstrap/bsv2 $@ 0,$$id,rx


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
	@id=$$($(TWZUTILSDIR)/objbuild/objstat $(BUILDDIR)/us/bsv | grep COID | awk '{print $$3}');\
		cp $(BUILDDIR)/us/bsv $(BUILDDIR)/us/root/$$id ;\
		cp $(BUILDDIR)/us/bsv.meta $(BUILDDIR)/us/root/$$id.meta ;\
		echo "bsv=$$id" >> $(BUILDDIR)/us/__kc
	@cp $(BUILDDIR)/us/__kc $(BUILDDIR)/us/root/kc
	@echo "[TAR] $@"
	@-rm $(BUILDDIR)/us/root.tar 2>/dev/null
	@tar cf $(BUILDDIR)/us/root.tar -C $(BUILDDIR)/us/root --transform='s/\.\///g' .

userspace: .twizzlerutils $(BUILDDIR)/us/root.tar

