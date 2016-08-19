#include <processor.h>
void arch_processor_enumerate(void)
{
	processor_register(true, 0);
}

void arch_processor_boot(struct processor *proc)
{

}

bool arch_debug_unwind_frame(struct frame *frame)
{
	if(frame->fp == 0 || (frame->fp < PHYSICAL_MAP_START)) {
		return false;
	}
	if(current_thread && frame->fp >= current_thread->kernel_stack + KERNEL_STACK_SIZE) {
		return false;
	}
	void *fp = (void *)(frame->fp - 16);
	frame->fp = *(uintptr_t *)(fp);
	frame->pc = *(uintptr_t *)((uintptr_t)fp + 8) - 4;
	return true;
}

