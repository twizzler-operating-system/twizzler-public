#pragma once

#include <arch/syscall.h>
#include <object.h>
#include <thread.h>

#include <twz/_sys.h>

long syscall_thread_spawn(uint64_t tidlo,
  uint64_t tidhi,
  struct sys_thrd_spawn_args *tsa,
  int flags);

long syscall_become(struct arch_syscall_become_args *ba);
long syscall_epilogue(int num);
long syscall_prelude(int num);

struct timespec {
	uint64_t tv_sec;
	uint64_t tv_nsec;
};

long syscall_thread_sync(size_t count, struct sys_thread_sync_args *args);

long syscall_invalidate_kso(struct kso_invl_args *invl, size_t count);

long syscall_attach(uint64_t palo, uint64_t pahi, uint64_t chlo, uint64_t chhi, uint64_t flags);
long syscall_detach(uint64_t palo, uint64_t pahi, uint64_t chlo, uint64_t chhi, uint64_t flags);

long syscall_ocreate(uint64_t kulo,
  uint64_t kuhi,
  uint64_t tlo,
  uint64_t thi,
  uint64_t flags,
  objid_t *);
long syscall_odelete(uint64_t olo, uint64_t ohi, uint64_t flags);

#define THRD_CTL_ARCH_MAX 0xff

int arch_syscall_thrd_ctl(int op, long arg);
long syscall_thrd_ctl(int op, long arg);
long syscall_kaction(size_t count, struct sys_kaction_args *args);

long syscall_opin(uint64_t lo, uint64_t hi, uint64_t *addr, int flags);
long syscall_octl(uint64_t lo, uint64_t hi, int op, long arg, long arg2, long arg3);
long syscall_kconf(int cmd, long arg);
