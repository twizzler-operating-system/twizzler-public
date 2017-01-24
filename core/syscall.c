#include <interrupt.h>
#include <thread.h>
#include <syscall.h>

long syscall_null(void)
{
	printk("-- called null syscall\n");
	return 0;
}

__int128 syscall_test128(void)
{
	__int128 g = 0x1122334455667788ul;
	g <<= 64;
	g |= 0x99aabbccddeefful;
	return g;
}

long (*syscall_table[NUM_SYSCALLS])() = {
	[0] = syscall_null,
	[1] = syscall_thread_spawn,
	[2] = syscall_null,
	[3] = syscall_null,
	[4] = syscall_null,
	[5] = syscall_null,
	[6] = syscall_null,
	[7] = syscall_null,
};

__int128 (*syscall_table128[NUM_SYSCALLS128])() = {
	[0] = syscall_test128,
};

