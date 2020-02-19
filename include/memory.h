#pragma once
#include <arch/memory.h>
#include <lib/list.h>
#include <machine/memory.h>
#include <spinlock.h>
#include <string.h>
#include <workqueue.h>

#define MM_BUDDY_MIN_SIZE 0x1000
#define MAX_ORDER 20
#define MIN_SIZE MM_BUDDY_MIN_SIZE
#define MAX_SIZE ((uintptr_t)MIN_SIZE << MAX_ORDER)

#define PM_TYPE_NV 1
#define PM_TYPE_DRAM 2
#define PM_TYPE_ANY (~0)

#define VADDR_IS_KERNEL(x) ({ (x) >= 0xFFFF000000000000ul; })

enum memory_type {
	MEMORY_UNKNOWN,
	MEMORY_AVAILABLE,
	MEMORY_RESERVED,
	MEMORY_RECLAIMABLE,
	MEMORY_BAD,
	MEMORY_CODE,
	MEMORY_KERNEL_IMAGE,
};

enum memory_subtype {
	MEMORY_SUBTYPE_NONE,
	MEMORY_AVAILABLE_VOLATILE,
	MEMORY_AVAILABLE_PERSISTENT,
};

struct mem_allocator {
	struct spinlock pm_buddy_lock;
	uint8_t *bitmaps[MAX_ORDER + 1];
	struct list freelists[MAX_ORDER + 1];
	size_t num_allocated[MAX_ORDER + 1];
	_Atomic size_t free_memory, available_memory;
	uintptr_t marker;
	bool ready;
	// char static_bitmaps[((MEMORY_SIZE / MIN_SIZE) / 8) * 2];
	char *static_bitmaps;
	size_t off;
	uintptr_t start;
	size_t length;
	void *vstart;
	struct list entry;
};

struct memregion {
	uintptr_t start;
	size_t length;
	int flags;
	bool ready; /* TODO: move this to flags */
	enum memory_type type;
	enum memory_subtype subtype;
	struct list entry;
	size_t off;
};

void mm_init(void);
void mm_init_phase_2(void);
void arch_mm_init(void);
uintptr_t pmm_buddy_allocate(struct mem_allocator *, size_t length);
void pmm_buddy_deallocate(struct mem_allocator *, uintptr_t address);
void pmm_buddy_init(struct mem_allocator *);
void mm_register_region(struct memregion *reg);
void mm_init_region(struct memregion *reg,
  uintptr_t start,
  size_t len,
  enum memory_type type,
  enum memory_subtype);

void *mm_memory_alloc(size_t length, int type, bool clear);
void mm_memory_dealloc(void *addr);
uintptr_t mm_physical_early_alloc(void);
void *mm_ptov(uintptr_t addr);
uintptr_t mm_vtop(void *addr);
uintptr_t mm_vtoo(void *addr);
uintptr_t mm_otop(uintptr_t oaddr);

void mm_print_stats(void);

static inline void *mm_virtual_early_alloc(void)
{
	void *p = mm_ptov(mm_physical_early_alloc());
	memset(p, 0, mm_page_size(0));
	return p;
}

#include <krc.h>
#include <lib/rb.h>
#include <twz/_view.h>

struct vm_context {
	struct arch_vm_context arch;
	struct kso_view *view;
	struct rbroot root;
	struct spinlock lock;
	struct krc refs;
	struct task free_task;
};

void arch_mm_switch_context(struct vm_context *vm);
void arch_mm_context_init(struct vm_context *ctx);

#define VMAP_WIRE 1

struct vmap {
	struct object *obj;
	size_t slot;
	uint32_t flags;
	int status;

	struct rbnode node;
	struct list entry;
};

// struct vmap *vm_context_map(struct vm_context *v, uint128_t objid, size_t slot, uint32_t flags);
void vm_context_destroy(struct vm_context *v);
struct vm_context *vm_context_create(void);
void kso_view_write(struct object *obj, size_t slot, struct viewentry *v);
void vm_kernel_map_object(struct object *obj);
void vm_kernel_unmap_object(struct object *obj);
size_t vm_max_slot(void);
void vm_context_fault(uintptr_t ip, uintptr_t addr, int flags);
struct object;
struct slot;
void arch_vm_map_object(struct vm_context *ctx, struct vmap *map, struct slot *);
void arch_vm_unmap_object(struct vm_context *ctx, struct vmap *map);
bool arch_vm_map(struct vm_context *ctx, uintptr_t virt, uintptr_t phys, int level, uint64_t flags);
bool arch_vm_unmap(struct vm_context *ctx, uintptr_t virt);
bool arch_vm_getmap(struct vm_context *ctx,
  uintptr_t virt,
  uintptr_t *phys,
  int *level,
  uint64_t *flags);

struct thread;
bool vm_setview(struct thread *, struct object *viewobj);
bool vm_vaddr_lookup(void *addr, objid_t *id, uint64_t *off);
int vm_context_wire(const void *p);

void vm_context_put(struct vm_context *);
void vm_context_map(struct vm_context *v, struct vmap *m);
void vm_vmap_init(struct vmap *vmap, struct object *obj, size_t vslot, uint32_t flags);
#define FAULT_EXEC 0x1
#define FAULT_WRITE 0x2
#define FAULT_USER 0x4
#define FAULT_ERROR_PERM 0x10
#define FAULT_ERROR_PRES 0x20
void kernel_fault_entry(uintptr_t ip, uintptr_t addr, int flags);

extern bool mm_ready;
