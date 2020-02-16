#include <memory.h>
#include <processor.h>
extern struct vm_context kernel_ctx;

#define PML4_IDX(v) (((v) >> 39) & 0x1FF)
#define PDPT_IDX(v) (((v) >> 30) & 0x1FF)
#define PD_IDX(v) (((v) >> 21) & 0x1FF)
#define PT_IDX(v) (((v) >> 12) & 0x1FF)
#define PAGE_PRESENT (1ull << 0)
#define PAGE_LARGE (1ull << 7)

static uint64_t *kernel_virts_pdpt[256];

#define RECUR_ATTR_MASK (VM_MAP_EXEC | VM_MAP_USER | VM_MAP_WRITE | VM_MAP_ACCESSED | VM_MAP_DIRTY)

static bool __do_vm_map(struct vm_context *ctx,
  uintptr_t virt,
  uintptr_t phys,
  int level,
  uint64_t flags)
{
	/* translate flags for NX bit (toggle) */
	flags ^= VM_MAP_EXEC;
	assert(level == 2); /* TODO: remove level */

	int pml4_idx = PML4_IDX(virt);
	int pdpt_idx = PDPT_IDX(virt);
	bool is_kernel = VADDR_IS_KERNEL(virt);

	uint64_t **table = is_kernel ? ctx->arch.kernel_pdpts : ctx->arch.user_pdpts;
	if(!ctx->arch.pml4[pml4_idx]) {
		table[is_kernel ? pml4_idx / 2 : pml4_idx] =
		  (void *)mm_memory_alloc(0x1000, PM_TYPE_DRAM, true);
		/* TODO: right flags? */
		ctx->arch.pml4[pml4_idx] = mm_vtoo(table[is_kernel ? pml4_idx / 2 : pml4_idx])
		                           | PAGE_PRESENT | VM_MAP_WRITE | VM_MAP_USER;
	}
	uintptr_t *pdpt = table[is_kernel ? pml4_idx / 2 : pml4_idx];
	if(pdpt[pdpt_idx]) {
		return false;
	}
	pdpt[pdpt_idx] = phys | flags | PAGE_PRESENT | PAGE_LARGE;
	return true;
}

bool arch_vm_getmap(struct vm_context *ctx,
  uintptr_t virt,
  uintptr_t *phys,
  int *level,
  uint64_t *flags)
{
	if(ctx == NULL) {
		ctx = current_thread ? current_thread->ctx : &kernel_ctx;
	}
	int pml4_idx = PML4_IDX(virt);
	int pdpt_idx = PDPT_IDX(virt);
	bool is_kernel = VADDR_IS_KERNEL(virt);

	uintptr_t p = 0, f = 0;
	int l = 0;
	if(ctx->arch.pml4[pml4_idx] == 0) {
		return false;
	}

	uint64_t **table = is_kernel ? ctx->arch.kernel_pdpts : ctx->arch.user_pdpts;
	uintptr_t *pdpt = table[is_kernel ? pml4_idx / 2 : pml4_idx];
	assert(pdpt != NULL);
	if(pdpt[pdpt_idx] == 0) {
		return false;
	} else if(pdpt[pdpt_idx] & PAGE_LARGE) {
		p = pdpt[pdpt_idx] & VM_PHYS_MASK;
		f = pdpt[pdpt_idx] & ~VM_PHYS_MASK;
		l = 2;
	}

	f &= ~PAGE_LARGE;
	f ^= VM_MAP_EXEC;
	if(phys)
		*phys = p;
	if(flags)
		*flags = f;
	if(level)
		*level = l;

	return true;
}

bool arch_vm_unmap(struct vm_context *ctx, uintptr_t virt)
{
	if(ctx == NULL) {
		ctx = current_thread ? current_thread->ctx : &kernel_ctx;
	}
	int pml4_idx = PML4_IDX(virt);
	int pdpt_idx = PDPT_IDX(virt);
	bool is_kernel = VADDR_IS_KERNEL(virt);

	if(ctx->arch.pml4[pml4_idx] == 0) {
		return false;
	}

	uint64_t **table = is_kernel ? ctx->arch.kernel_pdpts : ctx->arch.user_pdpts;
	uintptr_t *pdpt = table[is_kernel ? pml4_idx / 2 : pml4_idx];
	assert(pdpt != NULL);
	if(pdpt[pdpt_idx] == 0) {
		return false;
	} else if(pdpt[pdpt_idx] & PAGE_LARGE) {
		pdpt[pdpt_idx] = 0;
	}

	return true;
}

bool arch_vm_map(struct vm_context *ctx, uintptr_t virt, uintptr_t phys, int level, uint64_t flags)
{
	if(ctx == NULL) {
		ctx = current_thread ? current_thread->ctx : &kernel_ctx;
	}
	return __do_vm_map(ctx, virt, phys, level, flags);
}

#include <object.h>
#include <slots.h>
#define MB (1024ul * 1024ul)
/* So, these should probably not be arch-specific. Also, we should keep track of
 * slots, maybe? Refcounts? */
void arch_vm_map_object(struct vm_context *ctx, struct vmap *map, struct slot *slot)
{
	uintptr_t vaddr = (uintptr_t)SLOT_TO_VADDR(map->slot);
	uintptr_t oaddr = SLOT_TO_OADDR(slot->num);

	/* TODO: map protections */
	if(arch_vm_map(ctx, vaddr, oaddr, MAX_PGLEVEL, VM_MAP_USER | VM_MAP_EXEC | VM_MAP_WRITE)
	   == false) {
		panic("map fail");
	}
}

void arch_vm_unmap_object(struct vm_context *ctx, struct vmap *map)
{
	uintptr_t vaddr = (uintptr_t)SLOT_TO_VADDR(map->slot);

	if(arch_vm_unmap(ctx, vaddr) == false) {
		/* TODO (major): is this a problem? */
	}
}

#define PHYS_LOAD_ADDRESS (KERNEL_PHYSICAL_BASE + KERNEL_LOAD_OFFSET)
#define PHYS_ADDR_DELTA (KERNEL_VIRTUAL_BASE + KERNEL_LOAD_OFFSET - PHYS_LOAD_ADDRESS)
#define PHYS(x) ((x)-PHYS_ADDR_DELTA)
void arch_mm_switch_context(struct vm_context *ctx)
{
	asm volatile("mov %0, %%cr3" ::"r"(ctx->arch.pml4_phys) : "memory");
}

void x86_64_vm_kernel_context_init(void)
{
	static bool _init = false;
	if(!_init) {
		_init = true;

		uintptr_t pml4_phys = mm_physical_early_alloc();
		uint64_t *pml4 = (uint64_t *)mm_ptov(pml4_phys);
		memset(pml4, 0, mm_page_size(0));

		/* init remap */
		uintptr_t pdpt_phys = mm_physical_early_alloc();
		uint64_t *pdpt = mm_ptov(pdpt_phys);
		memset(pdpt, 0, mm_page_size(0));
		size_t pml4_slot = KVSLOT_KERNEL_IMAGE / 512ul;
		assert(KVSLOT_BOOTSTRAP / 512ul == pml4_slot);
		size_t pdpt_slot = KVSLOT_KERNEL_IMAGE % 512ul;
		pml4[pml4_slot] = pdpt_phys | VM_MAP_WRITE | PAGE_PRESENT; /* TODO: W^X */
		pdpt[pdpt_slot] = VM_MAP_WRITE | VM_MAP_GLOBAL | PAGE_LARGE | PAGE_PRESENT;

		pdpt_slot = KVSLOT_BOOTSTRAP % 512ul;
		pdpt[pdpt_slot] = VM_MAP_WRITE | VM_MAP_GLOBAL | PAGE_LARGE | PAGE_PRESENT;

		kernel_virts_pdpt[pml4_slot / 2] = pdpt;

		kernel_ctx.arch.kernel_pdpts = kernel_virts_pdpt;
		kernel_ctx.arch.pml4 = pml4;
		kernel_ctx.arch.pml4_phys = pml4_phys;
	}

	asm volatile("mov %0, %%cr3" ::"r"(kernel_ctx.arch.pml4_phys) : "memory");
}

void arch_mm_context_init(struct vm_context *ctx)
{
	ctx->arch.pml4 = (void *)mm_memory_alloc(0x1000, PM_TYPE_DRAM, true);
	ctx->arch.pml4_phys = mm_vtoo(ctx->arch.pml4);
	for(int i = 0; i < 256; i++) {
		ctx->arch.pml4[i] = 0;
	}
	for(int i = 256; i < 512; i++) {
		ctx->arch.pml4[i] = ((uint64_t *)kernel_ctx.arch.pml4)[i];
	}

	ctx->arch.kernel_pdpts = kernel_virts_pdpt;
	ctx->arch.user_pdpts = (void *)mm_memory_alloc(256 * sizeof(void *), PM_TYPE_DRAM, true);
}
