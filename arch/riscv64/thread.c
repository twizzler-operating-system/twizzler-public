#include <thread.h>
void riscv_switch_thread(void *new_stack, void **old_sp);
void riscv_new_context(void *top, void **sp, void *jump, void *arg);
void arch_thread_initialize(struct thread *idle)
{
	asm volatile("mv tp, %0"::"r"(idle));
}

#include <thread.h>
void arch_thread_start(struct thread *thread, void *jump, void *arg)
{
	riscv_new_context((void *)((uintptr_t)thread->kernel_stack + KERNEL_STACK_SIZE),
			&thread->stack_pointer, jump, arg);
}

void arch_thread_switchto(struct thread *old, struct thread *new)
{
	asm volatile("mv tp, %0"::"r"(new));
	riscv_switch_thread(new->stack_pointer, &old->stack_pointer);
}


