#include <interrupt.h>
#include <thread.h>
#include <syscall.h>

long syscall_null(void)
{
	printk("-- called null syscall\n");
	return 0;
}

long syscall_debug_print(const char *data, size_t len)
{
	if(len > 1024) len = 1024;
	char buf[len+1];
	strncpy(buf, data, len);
	printk("[us]: %s\n", data);

	return len;
}

long (*syscall_table[NUM_SYSCALLS])() = {
	[SYS_NULL]        = syscall_null,
	[SYS_THRD_SPAWN]  = syscall_thread_spawn,
	[SYS_DEBUG_PRINT] = syscall_debug_print,
	[SYS_INVL_KSO]    = syscall_invalidate_kso,
	[SYS_ATTACH]      = syscall_attach,
	[SYS_DETACH]      = syscall_detach,
	[SYS_BECOME]      = syscall_become,
	[SYS_THRD_SYNC]   = syscall_thread_sync,
};

