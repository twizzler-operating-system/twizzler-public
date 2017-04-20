#include <thread.h>

long syscall_thread_spawn(__int128 foo)
{
	printk(" sys %lx %lx\n", (uint64_t)(foo >> 64), (uint64_t)(foo & 0xFFFFFFFFFFFFFFFF));
	
	return (uint64_t)(foo);
}

