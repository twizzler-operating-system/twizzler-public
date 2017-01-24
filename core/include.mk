C_SOURCES+=$(addprefix core/,main.c panic.c spinlock.c pm_buddy.c memory.c ubsan.c slab.c debug.c interrupt.c processor.c syscall.c timer.c unwind.c ksymbol.c schedule.c) 

include core/sys/include.mk
