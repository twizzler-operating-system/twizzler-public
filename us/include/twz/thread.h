#pragma once
#include <twz/_thrd.h>
#include <twz/obj.h>
void twz_thread_exit(void);

struct thread {
	objid_t tid;
	struct object obj;
};

#define TWZ_THREAD_STACK_SIZE 0x200000
