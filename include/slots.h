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
void slots_init(void);
void slot_init_bootstrap(size_t, size_t);

/* TODO: arch-dep; larger address spaces */
#define KVSLOT_KERNEL_IMAGE 0x3fffe
#define KVSLOT_TMP_MAP 0x3fffd
#define KVSLOT_ALLOC_START 0x3fff0
#define KVSLOT_ALLOC_STOP 0x3fffa
#define KVSLOT_BOOTSTRAP 0x3ffff
#define KVSLOT_PMAP 0x3fffb

#define KVSLOT_MAX 0x3fff0

#define SLOT_TO_OADDR(x) ({ (x) * OBJ_MAXSIZE; })

#define SLOT_TO_VADDR(x)                                                                           \
	({                                                                                             \
		uintptr_t y = (x)*OBJ_MAXSIZE;                                                             \
		if(y & (1ul << 47))                                                                        \
			y |= 0xffff000000000000ul;                                                             \
		(void *)y;                                                                                 \
	})

#define KOSLOT_BOOTSTRAP 0
#define KOSLOT_INIT_ALLOC 1
