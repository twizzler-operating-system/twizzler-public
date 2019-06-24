#pragma once
#include <twz/_thrd.h>
#include <twz/_types.h>
#include <twz/obj.h>
void twz_thread_exit(void);

struct thread {
	objid_t tid;
	struct object obj;
};

struct thrd_spawn_args {
	objid_t target_view;
	void (*start_func)(void *); /* thread entry function. */
	void *arg;                  /* argument for entry function. */
	char *stack_base;           /* stack base address. */
	size_t stack_size;
	char *tls_base; /* tls base address. */
};

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
		(void *)(a + OBJ_NULLPAGE_SIZE);                                                           \
	})

#define TWZ_THREAD_STACK_SIZE 0x200000

int twz_exec(objid_t id, char const *const *argv, char *const *env);

int twz_exec_create_view(struct object *view, objid_t id, objid_t *vid);
int twz_exec_view(struct object *view,
  objid_t vid,
  size_t entry,
  char const *const *argv,
  char *const *env);
