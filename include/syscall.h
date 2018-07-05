#pragma once

#include <arch/syscall.h>
#include <object.h>
#include <thread.h>

struct sys_thrd_spawn_args {
	objid_t target_view;
    void (*start_func)(void *);  /* thread entry function. */
    void *arg;                   /* argument for entry function. */
    char *stack_base;            /* stack base address. */
	size_t stack_size;
    char *tls_base;              /* tls base address. */
};

long syscall_thread_spawn(uint64_t tidlo, uint64_t tidhi, struct sys_thrd_spawn_args *tsa, int flags);

long syscall_become(uint64_t sclo, uint64_t schi, struct arch_syscall_become_args *ba);

struct timespec {
	uint64_t tv_sec;
	uint64_t tv_nsec;
};

#define THREAD_SYNC_SLEEP 0
#define THREAD_SYNC_WAKE  1

long syscall_thread_sync(int operation, int *addr, int arg, struct timespec *spec);

#define KSO_INVL_RES_OK 0
#define KSO_INVL_RES_ERR -1
#define KSOI_VALID 1

long syscall_invalidate_kso(struct kso_invl_args *invl, size_t count);

long syscall_attach(uint64_t palo, uint64_t pahi, uint64_t chlo, uint64_t chhi, uint64_t flags);
long syscall_detach(uint64_t palo, uint64_t pahi, uint64_t chlo, uint64_t chhi, uint64_t flags);

long syscall_ocreate(uint64_t olo, uint64_t ohi, uint64_t tlo, uint64_t thi, uint64_t flags);
long syscall_odelete(uint64_t olo, uint64_t ohi, uint64_t flags);

#define THRD_CTL_ARCH_MAX 0xff
#define THRD_CTL_EXIT     0x100

int arch_syscall_thrd_ctl(int op, long arg);
long syscall_thrd_ctl(int op, long arg);

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
#define SYS_THRD_CTL    10
#define NUM_SYSCALLS 11

