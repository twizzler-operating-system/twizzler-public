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
	[0] = syscall_null,
	[1] = syscall_thread_spawn,
	[2] = syscall_debug_print,
	[3] = syscall_null,
	[4] = syscall_null,
	[5] = syscall_null,
	[6] = syscall_null,
	[7] = syscall_null,
};

