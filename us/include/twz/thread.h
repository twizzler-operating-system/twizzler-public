#pragma once
#include <twz/_thrd.h>
#include <twz/_types.h>
#include <twz/obj.h>
void twz_thread_exit(void);

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

int twz_thread_create(struct thread *thrd);
int twz_thread_release(struct thread *thrd);
int twz_thread_spawn(struct thread *thrd, struct thrd_spawn_args *args);
ssize_t twz_thread_wait(size_t count,
  struct thread **threads,
  int *syncpoints,
  long *event,
  uint64_t *info);
int twz_thread_ready(struct thread *thread, int sp, uint64_t info);

#define twz_thread_repr_base()                                                                     \
	({                                                                                             \
		uint64_t a;                                                                                \
		asm volatile("rdgsbase %%rax" : "=a"(a));                                                  \
		(struct twzthread_repr *)(a + OBJ_NULLPAGE_SIZE);                                          \
	})

#define TWZ_THREAD_STACK_SIZE 0x200000

int twz_exec(objid_t id, char const *const *argv, char *const *env);

int twz_exec_create_view(twzobj *view, objid_t id, objid_t *vid);
int twz_exec_view(twzobj *view,
  objid_t vid,
  size_t entry,
  char const *const *argv,
  char *const *env);

twzobj *__twz_get_stdstack_obj(void);
#define twz_stdstack ({ __twz_get_stdstack_obj(); })

struct timespec;
int twz_thread_sync(int op, _Atomic uint64_t *addr, uint64_t val, struct timespec *timeout);
struct sys_thread_sync_args;
void twz_thread_sync_init(struct sys_thread_sync_args *args,
  int op,
  _Atomic uint64_t *addr,
  uint64_t val,
  struct timespec *timeout);

int twz_thread_sync_multiple(size_t count, struct sys_thread_sync_args *);

#ifndef __KERNEL__
#include <stdatomic.h>
#include <twz/_sys.h>
static inline uint64_t twz_thread_cword_consume(_Atomic uint64_t *w, uint64_t reset)
{
	while(true) {
		uint64_t tmp = atomic_exchange(w, reset);
		if(tmp != reset) {
			return tmp;
		}
		twz_thread_sync(THREAD_SYNC_SLEEP, w, reset, NULL);
	}
}

#include <limits.h>
static inline void twz_thread_cword_wake(_Atomic uint64_t *w, uint64_t val)
{
	*w = val;
	twz_thread_sync(THREAD_SYNC_WAKE, w, INT_MAX, NULL);
}

#endif
