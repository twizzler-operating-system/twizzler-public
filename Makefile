ifndef PROJECT
$(error PROJECT is not set. Please choose one of ($(shell ls --format=commas projects)), or create a new one in projects/)
endif

HOSTCC=$(CC)
HOSTCFLAGS=-Wall -Wextra -O3 -Wshadow -g

.DEFAULT_GOAL=all

.newproj:
	mkdir projects/${PROJECT}
	cp projects/.config.mk.template projects/${PROJECT}/config.mk

ifneq ("$(shell ls -d projects/$(PROJECT))","projects/$(PROJECT)")
$(error Project $(PROJECT) does not exist. Perhaps you wish to create it?)
endif

CONFIGFILE=projects/$(PROJECT)/config.mk
BUILDDIR=projects/$(PROJECT)/build

include $(CONFIGFILE)
TOOLCHAIN_PREFIX=$(CONFIG_TRIPLET)-

export PATH := ${TOOLCHAIN_PATH}/bin:$(PATH)
DEFINES=$(addprefix -D,$(shell grep "CONFIG" $(CONFIGFILE) | sed -e 's/=y/=1/g' -e 's/=n/=0/g' -e 's/\#.*$$//' -e '/^$$/d' -e 's/+=/=/g'))

ARCH=$(CONFIG_ARCH)
MACHINE=$(CONFIG_MACHINE)

CORE_CFLAGS=-ffreestanding -fno-omit-frame-pointer -std=gnu11 -g
WARN_CFLAGS=-Wall -Wextra -Wno-error=unused-variable -Wno-error=unused-function -Wno-error=unused-parameter -Wshadow
DFL_INCLUDES=-include stdbool.h -include stddef.h -include stdint.h -include printk.h -include system.h
INCLUDE_DIRS=include machine/$(MACHINE)/include arch/$(ARCH)/include us/include
INCLUDES=$(addprefix -I,$(INCLUDE_DIRS))
CFLAGS=$(CORE_CFLAGS) $(WARN_CFLAGS) $(INCLUDES) $(DFL_INCLUDES) $(DEFINES)
ASFLAGS=$(INCLUDES) $(DEFINES)

ifeq ($(CONFIG_WERROR),y)
CFLAGS+=-Werror
endif

ifeq ($(CONFIG_UBSAN),y)
CFLAGS+=-fsanitize=undefined -fstack-check -fstack-protector-all
endif

CFLAGS+=-O$(CONFIG_OPTIMIZE)

C_SOURCES=
ASM_SOURCES=

ifeq ($(CONFIG_INSTRUMENT),y)
CFLAGS+=-finstrument-functions '-finstrument-functions-exclude-file-list=lib/vsprintk.c,core/panic.c,core/instrument.c,core/ksymbol.c'
C_SOURCES+=core/instrument.c
endif

OBJECTS=$(addprefix $(BUILDDIR)/,$(ASM_SOURCES:.S=.o) $(C_SOURCES:.c=.o))

TWZUTILSDIR=twizzler-utils

.twizzlerutils:
	@$(MAKE) -s -C $(TWZUTILSDIR)

all: $(BUILDDIR)/kernel

include arch/$(ARCH)/include.mk
include machine/$(MACHINE)/include.mk
include lib/include.mk
include core/include.mk
FEATURE_FLAGS=$(addprefix -D,$(addsuffix =1,$(ARCH_KERNEL_SUPPORT)))

CFLAGS+=$(FEATURE_FLAGS)
ASFLAGS+=$(FEATURE_FLAGS)

CRTBEGIN=$(shell $(TOOLCHAIN_PATH)/bin/$(TOOLCHAIN_PREFIX)gcc -print-file-name=crtbegin.o)
CRTEND=$(shell $(TOOLCHAIN_PATH)/bin/$(TOOLCHAIN_PREFIX)gcc -print-file-name=crtend.o)
ifeq ($(CRTI),)
CRTI=$(shell $(TOOLCHAIN_PATH)/bin/$(TOOLCHAIN_PREFIX)gcc -print-file-name=crti.o)
endif
ifeq ($(CRTN),)
CRTN=$(shell $(TOOLCHAIN_PATH)/bin/$(TOOLCHAIN_PREFIX)gcc -print-file-name=crtn.o)
endif

-include $(addprefix $(BUILDDIR)/,$(C_SOURCES:.c=.d) $(ASM_SOURCES:.S=.d))

KLIBS=

include third-party/include.mk

test: $(BUILDDIR)/kernel $(BUILDDIR)/us/root.tar
	$(QEMU) $(QEMU_FLAGS) -initrd $(BUILDDIR)/us/root.tar -serial stdio | tee serial.txt

export TOOLCHAIN_PREFIX
export BUILDDIR

$(BUILDDIR)/kernel: $(BUILDDIR)/link.ld $(OBJECTS) $(BUILDDIR)/symbols.o $(KLIBS)
	@mkdir -p $(BUILDDIR)
	@echo "[LD]  $@"
	@$(TOOLCHAIN_PREFIX)gcc -ffreestanding -nostdlib $(CRTI) $(CRTBEGIN) $(OBJECTS) $(KLIBS) $(BUILDDIR)/symbols.o $(CRTEND) $(CRTN) -o $(BUILDDIR)/kernel -T $(BUILDDIR)/link.ld -lgcc -Wl,--export-dynamic $(LDFLAGS)

$(BUILDDIR)/symbols.o: $(BUILDDIR)/symbols.c
	@echo "[CC]  $@"
	@$(TOOLCHAIN_PREFIX)gcc $(CFLAGS) -c -o $@ $<

$(BUILDDIR)/symbols.c: $(BUILDDIR)/kernel.sym.c
	@echo "[GEN] $@"
	@echo '#include <ksymbol.h>' > $(BUILDDIR)/symbols.c
	@echo '__attribute__((section(".ksyms"))) const struct ksymbol kernel_symbol_table[] = {' >> $(BUILDDIR)/symbols.c
	@cat $(BUILDDIR)/kernel.sym.c >> $(BUILDDIR)/symbols.c
	@echo "};" >> $(BUILDDIR)/symbols.c
	@echo >> $(BUILDDIR)/symbols.c
	@echo '__attribute__((section(".ksyms"))) const size_t kernel_symbol_table_length = ' $$(wc -l < $(BUILDDIR)/kernel.sym.c) ';' >> $(BUILDDIR)/symbols.c

$(BUILDDIR)/kernel.sym.c: $(BUILDDIR)/kernel.stage1
	@echo "[GEN] $@"
	@$(TOOLCHAIN_PREFIX)objdump -t $(BUILDDIR)/kernel.stage1 | grep '^.* [lg]' | awk '{print $$1 " " $$(NF-1) " " $$NF}' | grep -v '.hidden' | sed -rn 's|([0-9a-f]+) ([0-9a-f]+) ([a-zA-Z0-9_/\.]+)|{.value=0x\1, .size=0x\2, .name="\3"},|p' > $(BUILDDIR)/kernel.sym.c

$(BUILDDIR)/kernel.stage1: $(BUILDDIR)/link.ld $(OBJECTS) $(KLIBS)
	@echo "[LD]  $@"
	@mkdir -p $(BUILDDIR)
	@$(TOOLCHAIN_PREFIX)gcc -ffreestanding -nostdlib $(CRTI) $(CRTBEGIN) $(OBJECTS) $(KLIBS) $(CRTEND) $(CRTN) -o $(BUILDDIR)/kernel.stage1 -T $(BUILDDIR)/link.ld -lgcc -Wl,--export-dynamic $(LDFLAGS)

$(BUILDDIR)/%.o : %.S $(CONFIGFILE)
	@echo "[AS]  $@"
	@mkdir -p $(@D)
	@$(TOOLCHAIN_PREFIX)gcc $(ASFLAGS) -c $< -o $@ -MD -MF $(BUILDDIR)/$*.d

$(BUILDDIR)/%.o : %.c $(CONFIGFILE)
	@echo "[CC]  $@"
	@mkdir -p $(@D)
	@$(TOOLCHAIN_PREFIX)gcc $(CFLAGS) $($(addprefix CFLAGS_,$(subst /,_,$<))) -c $< -o $@ -MD -MF $(BUILDDIR)/$*.d

clean:
	-rm -rf $(BUILDDIR)

od: $(BUILDDIR)/kernel
	$(TOOLCHAIN_PREFIX)objdump -dS $(BUILDDIR)/kernel

re: $(BUILDDIR)/kernel
	$(TOOLCHAIN_PREFIX)readelf -a $(BUILDDIR)/kernel

rehw: $(BUILDDIR)/kernel userspace
	cp projects/x86_64/build/kernel /srv/tftp/kernel
	cp projects/x86_64/build/us/root.tar /srv/tftp/root.tar

$(BUILDDIR)/hd.img: $(USRPROGS)
	@-sudo umount $(BUILDDIR)/mnt
	truncate -s 1GB $(BUILDDIR)/hd.img
	mke2fs -F $(BUILDDIR)/hd.img
	mkdir -p $(BUILDDIR)/mnt
	sudo mount $(BUILDDIR)/hd.img $(BUILDDIR)/mnt
	sudo cp $(BUILDDIR)/kernel $(BUILDDIR)/mnt/
	sudo umount $(BUILDDIR)/mnt

CHEADERS=$(foreach file,$(C_SOURCES),$(shell cpp -MM -I include -I arch/$(ARCH)/include -I machine/$(MACHINE)/include $(file) -H 2>&1 >/dev/null | grep '^\.' | awk '{print $$2}'))
AHEADERS=$(foreach file,$(ASM_SOURCES),$(shell cpp -MM -I include -I arch/$(ARCH)/include -I machine/$(MACHINE)/include $(file) -H 2>&1 >/dev/null | grep '^\.' | awk '{print $$2}'))

tags: $(C_SOURCES) $(ASM_SOURCES) $(CHEADERS) $(AHEADERS)
	@ctags $(C_SOURCES) $(ASM_SOURCES) $(CHEADERS) $(AHEADERS)

include utils/include.mk
include us/include.mk

.PHONY: od re clean all test newproj userspace .twizzlerutils

depclean:
	-rm $(addprefix $(BUILDDIR)/,$(C_SOURCES:.c=.d) $(ASM_SOURCES:.S=.d))
