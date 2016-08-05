#pragma once
#include <thread-bits.h>
#include <ref.h>
#include <lib/hash.h>

struct thread {
	void *kernel_stack;
	struct ref ref;
	unsigned long id;
	_Atomic int flags;

	struct hashelem elem;
};

