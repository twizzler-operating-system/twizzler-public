#include <interrupt.h>
#include <processor.h>
#include <syscall.h>
#include <thread.h>

long syscall_null(long a)
{
	printk("-- called null syscall\n");
	if(a == 0x1234) {
		thread_print_all_threads();
	}
	return 0;
}

long syscall_debug_print(const char *data, size_t len)
{
	if(len > 1024)
		len = 1024;
	char buf[len + 1];
	strncpy(buf, data, len);
	// printk("[us:%ld]: %s\n", current_thread ? current_thread->id : 0, data);
	printk("%s", data);

	return len;
}

long (*syscall_table[NUM_SYSCALLS])() = {
	[SYS_NULL] = syscall_null,
	[SYS_THRD_SPAWN] = syscall_thread_spawn,
	[SYS_DEBUG_PRINT] = syscall_debug_print,
	[SYS_INVL_KSO] = syscall_invalidate_kso,
	[SYS_ATTACH] = syscall_attach,
	[SYS_DETACH] = syscall_detach,
	[SYS_BECOME] = syscall_become,
	[SYS_THRD_SYNC] = syscall_thread_sync,
	[SYS_OCREATE] = syscall_ocreate,
	[SYS_ODELETE] = syscall_odelete,
	[SYS_THRD_CTL] = syscall_thrd_ctl,
	[SYS_KACTION] = syscall_kaction,
	[SYS_OPIN] = syscall_opin,
	[SYS_OCTL] = syscall_octl,
};

long syscall_prelude(int num)
{
	kso_detach_event(current_thread, true, num);
	return 0;
}

long syscall_epilogue(int num)
{
	kso_detach_event(current_thread, false, num);
	return 0;
}
