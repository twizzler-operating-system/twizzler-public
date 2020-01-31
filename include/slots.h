#pragma once

#include <krc.h>
#include <lib/rb.h>
#include <memory.h>

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
void slots_init(void);
void slot_init_bootstrap(size_t, size_t);

/* TODO: arch-dep; larger address spaces */
#define KVSLOT_KERNEL_IMAGE ({ vm_max_slot() - 1; })

#define KVSLOT_TMP_MAP ({ vm_max_slot() - 2; })
#define KVSLOT_ALLOC_START ({ vm_max_slot() - 15; })
#define KVSLOT_ALLOC_STOP ({ vm_max_slot() - 5; })
#define KVSLOT_BOOTSTRAP ({ vm_max_slot(); })
#define KVSLOT_PMAP ({ vm_max_slot() - 4; })

#define KVSLOT_MAX ({ vm_max_slot() - 16; })

#define KVSLOT_START ({ (vm_max_slot() + 1) / 2; })

#define SLOT_TO_OADDR(x) ({ (x) * OBJ_MAXSIZE; })

#define SLOT_IS_KERNEL(x) ({ (x) >= KVSLOT_START; })

#define SLOT_TO_VADDR(x)                                                                           \
	({                                                                                             \
		uintptr_t y = (x)*OBJ_MAXSIZE;                                                             \
		if(y & (1ul << 47))                                                                        \
			y |= 0xffff000000000000ul;                                                             \
		(void *)y;                                                                                 \
	})

#define KOSLOT_BOOTSTRAP 0
#define KOSLOT_INIT_ALLOC 1
