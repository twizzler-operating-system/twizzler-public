C_SOURCES+=$(addprefix core/,main.c panic.c spinlock.c pm_buddy.c memory.c slab2.c debug.c interrupt.c processor.c syscall.c timer.c unwind.c ksymbol.c ksymbol_weak.c schedule.c object.c vmap.c clksrc.c secctx.c kc.c rand.c csprng.c kso.c)

include core/sys/include.mk
