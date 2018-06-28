#pragma once

#include <arch/syscall.h>


long syscall_thread_spawn(__int128 foo);

long syscall_become(uint64_t sclo, uint64_t schi, struct arch_syscall_become_args *ba);

struct timespec {
	uint64_t tv_sec;
	uint64_t tv_nsec;
};

#define THREAD_SYNC_SLEEP 0
#define THREAD_SYNC_WAKE  1

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

long syscall_ocreate(uint64_t olo, uint64_t ohi, uint64_t tlo, uint64_t thi, uint64_t flags);
long syscall_odelete(uint64_t olo, uint64_t ohi, uint64_t flags);

#define SYS_NULL        0
#define SYS_THRD_SPAWN  1
#define SYS_DEBUG_PRINT 2
#define SYS_INVL_KSO    3
#define SYS_ATTACH      4
#define SYS_DETACH      5
#define SYS_BECOME      6
#define SYS_THRD_SYNC   7
#define SYS_OCREATE     8
#define SYS_ODELETE     9


#define NUM_SYSCALLS 16

