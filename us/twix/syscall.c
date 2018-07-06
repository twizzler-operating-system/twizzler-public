
#include <stdint.h>

/* TODO: arch-dep */

struct twix_register_frame {
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t r11;
	uint64_t r10;
	uint64_t r9;
	uint64_t r8;
	uint64_t rbp;
	uint64_t rsi;
	uint64_t rdi;
	uint64_t rdx;
	uint64_t rbx;
	uint64_t rsp;
};

#include <debug.h>
#include <errno.h>
#include <twzsys.h>
#define LINUX_SYS_arch_prctl 158
#define LINUX_SYS_set_tid_address 218



#define ARCH_SET_GS 0x1001
#define ARCH_SET_FS 0x1002
#define ARCH_GET_FS 0x1003
#define ARCH_GET_GS 0x1004

long linux_sys_arch_prctl(int code, unsigned long addr)
{
	switch(code) {
		case ARCH_SET_FS:
			sys_thrd_ctl(THRD_CTL_SET_FS, (long)addr);
			break;
		case ARCH_SET_GS:
			sys_thrd_ctl(THRD_CTL_SET_GS, (long)addr);
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

long linux_sys_set_tid_address()
{
	/* TODO: NI */
	return 0;
}

static long (*syscall_table[])() = {
	[LINUX_SYS_arch_prctl] = linux_sys_arch_prctl,
	[LINUX_SYS_set_tid_address] = linux_sys_set_tid_address,
};

static size_t stlen = sizeof(syscall_table) / sizeof(syscall_table[0]);

long twix_syscall(long num, long a0, long a1, long a2, long a3, long a4, long a5)
{
	if((size_t)num >= stlen || num < 0 || syscall_table[num] == NULL) {
		return -ENOSYS;
	}
	return syscall_table[num](a0, a1, a2, a3, a4, a5);
}

long __twix_syscall_target_c(long num, struct twix_register_frame *frame)
{
	debug_printf("TWIX entry: %ld, %p %lx\n", num, frame, frame->rsp);
	long ret = twix_syscall(num, frame->rdi, frame->rsi, frame->rdx, frame->r10, frame->r8, frame->r9);
	if(ret == -ENOSYS) {
		debug_printf("Unimplemented UNIX system call: %ld", num);
	}
	return ret;
}

asm (
	".global __twix_syscall_target;"
	"__twix_syscall_target:;"
	"movq %rsp, %rcx;"
	"andq $-16, %rsp;"
	"pushq %rcx;"

	"pushq %rbx;"
	"pushq %rdx;"
	"pushq %rdi;"
	"pushq %rsi;"
	"pushq %rbp;"
	"pushq %r8;"
	"pushq %r9;"
	"pushq %r10;"
	"pushq %r11;"
	"pushq %r12;"
	"pushq %r13;"
	"pushq %r14;"
	"pushq %r15;"

	"movq %rsp, %rsi;"
	"movq %rax, %rdi;"
	"call __twix_syscall_target_c;"

	"popq %r15;"
	"popq %r14;"
	"popq %r13;"
	"popq %r12;"
	"popq %r11;"
	"popq %r10;"
	"popq %r9;"
	"popq %r8;"
	"popq %rbp;"
	"popq %rsi;"
	"popq %rdi;"
	"popq %rdx;"
	"popq %rbx;"
	"popq %rsp;"
	"ret;"
);


