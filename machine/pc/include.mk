ifneq ($(CONFIG_ARCH),x86_64)
$(error "Machine pc supports architectures: x86_64")
endif

C_SOURCES+=machine/pc/serial.c
