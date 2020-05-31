#pragma once

#ifdef __cplusplus
#include <atomic>
using std::atomic_uint_least64_t;
#else /* not __cplusplus */
#include <stdatomic.h>
#endif /* __cplusplus */

#include <twz/_thrd.h>
#include <twz/_types.h>
#include <twz/obj.h>

#define TWZ_THREAD_STACK_SIZE 0x200000
struct thread {
	objid_t tid;
	twzobj obj;
};

struct thrd_spawn_args {
	objid_t target_view;
	void (*start_func)(void *); /* thread entry function. */
	void *arg;                  /* argument for entry function. */
	char *stack_base;           /* stack base address. */
	size_t stack_size;
	char *tls_base; /* tls base address. */
};

__attribute__((non_null, const)) struct twzthread_repr *twz_thread_repr_base(void);
__attribute__((non_null, const)) twzobj *__twz_get_stdstack_obj(void);
#define twz_stdstack ({ __twz_get_stdstack_obj(); })

__must_check int twz_thread_create(struct thread *thrd);

int twz_thread_release(struct thread *thrd);

__must_check int twz_thread_spawn(struct thread *thrd, struct thrd_spawn_args *args);

__attribute__((noreturn)) void twz_thread_exit(uint64_t);

__must_check ssize_t twz_thread_wait(size_t count,
  struct thread **threads,
  int *syncpoints,
  long *event,
  uint64_t *info);

int twz_thread_ready(struct thread *thread, int sp, uint64_t info);

struct timespec;
__must_check int twz_thread_sync(int op,
  atomic_uint_least64_t *addr,
  uint64_t val,
  struct timespec *timeout);

struct sys_thread_sync_args;
void twz_thread_sync_init(struct sys_thread_sync_args *args,
  int op,
  atomic_uint_least64_t *addr,
  uint64_t val,
  struct timespec *timeout);
int twz_thread_sync32(int op, atomic_uint_least32_t *addr, uint32_t val, struct timespec *timeout);

__must_check int twz_thread_sync_multiple(size_t count, struct sys_thread_sync_args *);

/* TODO: get rid of these from the standard library */
int twz_exec(objid_t id, char const *const *argv, char *const *env);
int twz_exec_create_view(twzobj *view, objid_t id, objid_t *vid);
int twz_exec_view(twzobj *view,
  objid_t vid,
  size_t entry,
  char const *const *argv,
  char *const *env);

#ifndef __KERNEL__
void twz_thread_cword_wake(atomic_uint_least64_t *w, uint64_t val);
uint64_t twz_thread_cword_consume(atomic_uint_least64_t *w, uint64_t reset);
#endif
