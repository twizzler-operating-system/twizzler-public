#pragma once

struct thread;
__attribute__((const)) static inline struct thread *arch_thread_get_current(void)
{
	register struct thread *thread asm("tp");
	return thread;
}

