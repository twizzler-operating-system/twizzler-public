#pragma once

#define THRD_CTL_SET_FS   1
#define THRD_CTL_SET_GS   2
#define THRD_CTL_SET_IOPL 3

struct arch_syscall_become_args {
	objid_t target_view;
	uint64_t target_rip;
	uint64_t rax;
	uint64_t rbx;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rdi;
	uint64_t rsi;
	uint64_t rsp;
	uint64_t rbp;
	uint64_t r8;
	uint64_t r9;
	uint64_t r10;
	uint64_t r11;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;
};

