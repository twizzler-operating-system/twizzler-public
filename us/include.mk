TWZOBJS=test.0 bsv test2.0
MUSL=musl-1.1.16

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

$(BUILDDIR)/us/$(MUSL)/lib/libc.a: $(MUSL_SRCS) $(BUILDDIR)/us/libtwz.a $(MUSL_READY)
	@mkdir -p $(BUILDDIR)/us
	cp -a us/$(MUSL) $(BUILDDIR)/us
	TWZKROOT=$(shell pwd) TWZKBUILDDIR=$(BUILDDIR) CONFIGFILEPATH=../musl-config.mk $(MAKE) -C $(BUILDDIR)/us/$(MUSL)
	@touch $@

MUSL_INCL=$(addprefix -I$(BUILDDIR)/us/$(MUSL)/,include obj/include src/internal obj/src/internal arch/generic arch/$(ARCH))

$(BUILDDIR)/us/$(MUSL)/include/string.h:
	$(MAKE) musl-prep

MUSL_READY=$(BUILDDIR)/us/$(MUSL)/include/string.h

MUSL_STATIC_LIBC_PRE=$(addprefix $(BUILDDIR)/us/$(MUSL)/lib/,crti.o crt1.o)
MUSL_STATIC_LIBC=$(BUILDDIR)/us/$(MUSL)/lib/libc.a
MUSL_STATIC_LIBC_POST=$(BUILDDIR)/us/$(MUSL)/lib/crtn.o

#libtwz

LIBTWZ_SRC=$(addprefix us/libtwz/,notify.c bstream.c mutex.c twzio.c viewcall.c twzlog.c name.c corecall.c debug.c object.c blake2.c secctx.c thread.c fault.c)
LIBTWZ_OBJ=$(addprefix $(BUILDDIR)/,$(LIBTWZ_SRC:.c=.o))

$(BUILDDIR)/us/libtwz.a: $(LIBTWZ_OBJ)
	ar rcs $(BUILDDIR)/us/libtwz.a $(LIBTWZ_OBJ)

$(BUILDDIR)/us/libtwz/%.o: us/libtwz/%.c $(MUSL_READY)
	@mkdir -p $(BUILDDIR)/us/libtwz
	$(TOOLCHAIN_PREFIX)gcc -Og -g -Wall -Wextra -std=gnu11 -I us/include -ffreestanding $(MUSL_INCL) -c -o $@ $< -MD -Werror

-include $(LIBTWZ_OBJ:.o=.d)



$(BUILDDIR)/us/test2: us/test2.c us/elf.ld $(BUILDDIR)/us/libtwz.a $(BUILDDIR)/us/$(MUSL)/lib/libc.a
	@echo "[CC]  $@"
	@$(TOOLCHAIN_PREFIX)gcc $(USCFLAGS) -o $@ -nostdlib $(MUSL_STATIC_LIBC_PRE) $< $(BUILDDIR)/us/libtwz.a $(MUSL_STATIC_LIBC) $(MUSL_STATIC_LIBC_POST) -I us/include $(MUSL_INCL)

$(BUILDDIR)/us/test2.0.meta: $(BUILDDIR)/us/test2
$(BUILDDIR)/us/test2.0: $(BUILDDIR)/us/test2
	@echo "[PE]  $@"
	@$(TWZUTILSDIR)/postelf/postelf $(BUILDDIR)/us/test2
	@echo "fot:R:0:1:0" | $(TWZUTILSDIR)/objbuild/objbuild -o $(BUILDDIR)/us/test2.0 -a











INITNAME=test2.0

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

