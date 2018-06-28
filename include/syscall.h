#pragma once

#include <arch/syscall.h>

#define NUM_SYSCALLS 8

long syscall_thread_spawn(__int128 foo);

long syscall_become(uint64_t sclo, uint64_t schi, struct arch_syscall_become_args *ba);

struct timespec {
	uint64_t tv_sec;
	uint64_t tv_nsec;
};

long syscall_thread_sync(int operation, int *addr, int arg, struct timespec *spec);

#define KSO_INVL_RES_OK 0

struct syscall_kso_invl {
	objid_t id;
	size_t offset;
	uint32_t length;
	uint16_t flags;
	uint16_t result;
};

long syscall_invalidate_kso(struct syscall_kso_invl *invl, size_t count);

long syscall_attach(uint64_t palo, uint64_t pahi, uint64_t chlo, uint64_t chhi, uint64_t flags);
long syscall_detach(uint64_t palo, uint64_t pahi, uint64_t chlo, uint64_t chhi, uint64_t flags);


