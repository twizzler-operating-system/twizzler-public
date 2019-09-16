#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <twz/debug.h>

/* TODO: arch-dep */

#define LINUX_SYS_read 0
#define LINUX_SYS_write 1
#define LINUX_SYS_open 2
#define LINUX_SYS_close 3
#define LINUX_SYS_stat 4

#define LINUX_SYS_mmap 9

#define LINUX_SYS_ioctl 16
#define LINUX_SYS_pread 17
#define LINUX_SYS_pwrite 18
#define LINUX_SYS_readv 19
#define LINUX_SYS_writev 20
#define LINUX_SYS_access 21

#define LINUX_SYS_clone 56
#define LINUX_SYS_fork 57
#define LINUX_SYS_execve 59
#define LINUX_SYS_exit 60
#define LINUX_SYS_wait4 61

#define LINUX_SYS_arch_prctl 158

#define LINUX_SYS_set_tid_address 218

#define LINUX_SYS_exit_group 231

#define LINUX_SYS_faccessat 269
#define LINUX_SYS_pselect6 270

#define LINUX_SYS_preadv 295
#define LINUX_SYS_pwritev 296

#define LINUX_SYS_preadv2 327
#define LINUX_SYS_pwritev2 328

#include "syscalls.h"

static long (*syscall_table[])() = {
	[LINUX_SYS_arch_prctl] = linux_sys_arch_prctl,
	[LINUX_SYS_set_tid_address] = linux_sys_set_tid_address,
	[LINUX_SYS_pwritev2] = linux_sys_pwritev2,
	[LINUX_SYS_pwritev] = linux_sys_pwritev,
	[LINUX_SYS_writev] = linux_sys_writev,
	[LINUX_SYS_preadv2] = linux_sys_preadv2,
	[LINUX_SYS_preadv] = linux_sys_preadv,
	[LINUX_SYS_readv] = linux_sys_readv,
	[LINUX_SYS_pread] = linux_sys_pread,
	[LINUX_SYS_pwrite] = linux_sys_pwrite,
	[LINUX_SYS_read] = linux_sys_read,
	[LINUX_SYS_write] = linux_sys_write,
	[LINUX_SYS_ioctl] = linux_sys_ioctl,
	[LINUX_SYS_exit] = linux_sys_exit,
	[LINUX_SYS_exit_group] = linux_sys_exit,
	[LINUX_SYS_open] = linux_sys_open,
	[LINUX_SYS_close] = linux_sys_close,
	[LINUX_SYS_mmap] = linux_sys_mmap,
	[LINUX_SYS_execve] = linux_sys_execve,
	[LINUX_SYS_clone] = linux_sys_clone,
	[LINUX_SYS_pselect6] = linux_sys_pselect6,
	[LINUX_SYS_fork] = linux_sys_fork,
	[LINUX_SYS_wait4] = linux_sys_wait4,
	[LINUX_SYS_stat] = linux_sys_stat,
	[LINUX_SYS_access] = linux_sys_access,
	[LINUX_SYS_faccessat] = linux_sys_faccessat,
};

static size_t stlen = sizeof(syscall_table) / sizeof(syscall_table[0]);

long twix_syscall(long num, long a0, long a1, long a2, long a3, long a4, long a5)
{
	__linux_init();
	if((size_t)num >= stlen || num < 0 || syscall_table[num] == NULL) {
#if 1
		debug_printf("Unimplemented Linux system call: %ld\n", num);
#endif
		return -ENOSYS;
	}
	if(num == LINUX_SYS_clone) {
		/* needs frame */
		return -ENOSYS;
	}
	if(num == LINUX_SYS_fork) {
		/* needs frame */
		return -ENOSYS;
	}
	long r = syscall_table[num](a0, a1, a2, a3, a4, a5);
	debug_printf("sc %ld ret %ld\n", num, r);
	return r;
}

static long twix_syscall_frame(struct twix_register_frame *frame,
  long num,
  long a0,
  long a1,
  long a2,
  long a3,
  long a4,
  long a5)
{
	__linux_init();
	if((size_t)num >= stlen || num < 0 || syscall_table[num] == NULL) {
#if 1
		debug_printf("Unimplemented Linux system call: %ld\n", num);
#endif
		return -ENOSYS;
	}
	if(num == LINUX_SYS_clone) {
		/* needs frame */
		return syscall_table[num](frame, a0, a1, a2, a3, a4, a5);
	} else if(num == LINUX_SYS_fork) {
		/* needs frame */
		return syscall_table[num](frame, a0, a1, a2, a3, a4, a5);
	}
	long r = syscall_table[num](a0, a1, a2, a3, a4, a5);
	// debug_printf("sc %ld ret %ld\n", num, r);
	return r;
}

long __twix_syscall_target_c(long num, struct twix_register_frame *frame)
{
	long ret = twix_syscall_frame(
	  frame, num, frame->rdi, frame->rsi, frame->rdx, frame->r10, frame->r8, frame->r9);
	return ret;
}

asm(".global __twix_syscall_target;"
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

    "movabs $__twix_syscall_target_c, %rcx;"
    "call *%rcx;"

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
    "ret;");
