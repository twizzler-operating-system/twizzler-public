C_SOURCES+=$(addprefix core/,main.c panic.c spinlock.c debug.c interrupt.c processor.c syscall.c timer.c unwind.c ksymbol.c ksymbol_weak.c schedule.c clksrc.c kc.c rand.c csprng.c)

CFLAGS_core_ubsan.c=-Wno-missing-prototypes
CFLAGS_core_dlmalloc.c=-Wno-sign-compare

include core/obj/include.mk
include core/mm/include.mk
include core/sys/include.mk
