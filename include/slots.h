#pragma once

#include <krc.h>
#include <lib/rb.h>

struct object;
struct slot {
	struct krc rc;
	struct object *obj;
	size_t num;
	struct rbnode node;
	struct slot *next;
};
struct slot *slot_alloc(void);
struct slot *slot_lookup(size_t);
void slot_release(struct slot *);
