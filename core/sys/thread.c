#include <thread.h>
#include <syscall.h>

long syscall_thread_spawn(__int128 foo)
{
	printk(" sys %lx %lx\n", (uint64_t)(foo >> 64), (uint64_t)(foo & 0xFFFFFFFFFFFFFFFF));
	
	return (uint64_t)(foo);
}

long syscall_become(uint64_t sclo, uint64_t schi, struct arch_syscall_become_args *ba)
{
	return 0;
}

