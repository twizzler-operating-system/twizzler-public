#include <memory.h>
#include <processor.h>

#define PML4_IDX(v) (((v) >> 39) & 0x1FF)
#define PDPT_IDX(v) (((v) >> 30) & 0x1FF)
#define PD_IDX(v) (((v) >> 21) & 0x1FF)
#define PT_IDX(v) (((v) >> 12) & 0x1FF)
#define PAGE_PRESENT (1ull << 0)
#define PAGE_LARGE (1ull << 7)

static uint64_t kernel_virts_pdpt[256];
static uint64_t kernel_virts_pd[256];
static uint64_t kernel_virts_pt[256];

#define RECUR_ATTR_MASK (VM_MAP_EXEC | VM_MAP_USER | VM_MAP_WRITE | VM_MAP_ACCESSED | VM_MAP_DIRTY)

static inline void test_and_allocate(uintptr_t *loc, uint64_t attr)
{
	if(!*loc) {
		*loc = (uintptr_t)mm_physical_alloc(0x1000, PM_TYPE_DRAM, true) | (attr & RECUR_ATTR_MASK)
		       | PAGE_PRESENT;
	}
}

#define GET_VIRT_TABLE(x) ((uintptr_t *)mm_ptov(((x)&VM_PHYS_MASK)))

static bool __do_vm_map(uintptr_t pml4_phys,
  uintptr_t virt,
  uintptr_t phys,
  int level,
  uint64_t flags)
{
	/* translate flags for NX bit (toggle) */
	flags ^= VM_MAP_EXEC;

	int pml4_idx = PML4_IDX(virt);
	int pdpt_idx = PDPT_IDX(virt);
	int pd_idx = PD_IDX(virt);
	int pt_idx = PT_IDX(virt);

	uintptr_t *pml4 = GET_VIRT_TABLE(pml4_phys);
	test_and_allocate(&pml4[pml4_idx], flags);

	uintptr_t *pdpt = GET_VIRT_TABLE(pml4[pml4_idx]);
	if(level == 2) {
		if(pdpt[pdpt_idx]) {
			return false;
		}
		pdpt[pdpt_idx] = phys | flags | PAGE_PRESENT | PAGE_LARGE;
	} else {
		test_and_allocate(&pdpt[pdpt_idx], flags);
		uintptr_t *pd = GET_VIRT_TABLE(pdpt[pdpt_idx]);

		if(level == 1) {
			if(pd[pd_idx]) {
				return false;
			}
			pd[pd_idx] = phys | flags | PAGE_PRESENT | PAGE_LARGE;
		} else {
			test_and_allocate(&pd[pd_idx], flags);
			uintptr_t *pt = GET_VIRT_TABLE(pd[pd_idx]);
			if(pt[pt_idx]) {
				return false;
			}
			pt[pt_idx] = phys | flags | PAGE_PRESENT;
		}
	}
	return true;
}

bool arch_vm_getmap(struct vm_context *ctx,
  uintptr_t virt,
  uintptr_t *phys,
  int *level,
  uint64_t *flags)
{
	uintptr_t table_phys;
	if(ctx == NULL) {
		if(current_thread) {
			table_phys = current_thread->ctx->arch.pml4_phys;
		} else {
			asm volatile("mov %%cr3, %0" : "=r"(table_phys));
			table_phys &= VM_PHYS_MASK;
		}
	} else {
		table_phys = ctx->arch.pml4_phys;
	}
	int pml4_idx = PML4_IDX(virt);
	int pdpt_idx = PDPT_IDX(virt);
	int pd_idx = PD_IDX(virt);
	int pt_idx = PT_IDX(virt);

	uintptr_t p, f;
	int l;
	uintptr_t *pml4 = GET_VIRT_TABLE(table_phys);
	if(pml4[pml4_idx] == 0) {
		return false;
	}

	uintptr_t *pdpt = GET_VIRT_TABLE(pml4[pml4_idx]);
	if(pdpt[pdpt_idx] == 0) {
		return false;
	} else if(pdpt[pdpt_idx] & PAGE_LARGE) {
		p = pdpt[pdpt_idx] & VM_PHYS_MASK;
		f = pdpt[pdpt_idx] & ~VM_PHYS_MASK;
		l = 2;
	} else {
		uintptr_t *pd = GET_VIRT_TABLE(pdpt[pdpt_idx]);
		if(pd[pd_idx] == 0) {
			return false;
		} else if(pd[pd_idx] & PAGE_LARGE) {
			p = pd[pd_idx] & VM_PHYS_MASK;
			f = pd[pd_idx] & ~VM_PHYS_MASK;
			l = 1;
		} else {
			uintptr_t *pt = GET_VIRT_TABLE(pd[pd_idx]);
			if(pt[pt_idx] == 0) {
				return false;
			}
			p = pt[pt_idx] & VM_PHYS_MASK;
			f = pt[pt_idx] & ~VM_PHYS_MASK;
			l = 0;
		}
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
		ctx = current_thread->ctx;
	}
	int pml4_idx = PML4_IDX(virt);
	int pdpt_idx = PDPT_IDX(virt);
	int pd_idx = PD_IDX(virt);
	int pt_idx = PT_IDX(virt);

	uintptr_t *pml4 = GET_VIRT_TABLE(ctx->arch.pml4_phys);
	if(pml4[pml4_idx] == 0) {
		return false;
	}

	uintptr_t *pdpt = GET_VIRT_TABLE(pml4[pml4_idx]);
	if(pdpt[pdpt_idx] == 0) {
		return false;
	} else if(pdpt[pdpt_idx] & PAGE_LARGE) {
		pdpt[pdpt_idx] = 0;
	} else {
		uintptr_t *pd = GET_VIRT_TABLE(pdpt[pdpt_idx]);
		if(pd[pd_idx] == 0) {
			return false;
		} else if(pd[pd_idx] & PAGE_LARGE) {
			pd[pd_idx] = 0;
		} else {
			uintptr_t *pt = GET_VIRT_TABLE(pd[pd_idx]);
			if(pt[pt_idx] == 0) {
				return false;
			}
			pt[pt_idx] = 0;
		}
	}

	return true;
}

bool arch_vm_map(struct vm_context *ctx, uintptr_t virt, uintptr_t phys, int level, uint64_t flags)
{
	uintptr_t tblphys;
	if(ctx == NULL) {
		if(current_thread) {
			tblphys = current_thread->ctx->arch.pml4_phys;
		} else {
			asm volatile("mov %%cr3, %0" : "=r"(tblphys));
			tblphys &= VM_PHYS_MASK;
		}
	} else {
		tblphys = ctx->arch.pml4_phys;
	}
	return __do_vm_map(tblphys, virt, phys, level, flags);
}

#include <object.h>
#include <slots.h>
#define MB (1024ul * 1024ul)
/* So, these should probably not be arch-specific. Also, we should keep track of
 * slots, maybe? Refcounts? */
void arch_vm_map_object(struct vm_context *ctx, struct vmap *map, struct object *obj)
{
	if(obj->slot == NULL) {
		panic("tried to map an unslotted object");
	}
	uintptr_t vaddr = map->slot * mm_page_size(MAX_PGLEVEL);
	uintptr_t oaddr = obj->slot->num * mm_page_size(obj->pglevel);

	/* TODO: map protections */
	if(arch_vm_map(ctx, vaddr, oaddr, MAX_PGLEVEL, VM_MAP_USER | VM_MAP_EXEC | VM_MAP_WRITE)
	   == false) {
		panic("map fail");
	}
}

void arch_vm_unmap_object(struct vm_context *ctx, struct vmap *map, struct object *obj)
{
	if(obj->slot == -1) {
		panic("tried to map an unslotted object");
	}
	uintptr_t vaddr = map->slot * mm_page_size(MAX_PGLEVEL);

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

extern struct vm_context kernel_ctx;
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

		kernel_virts_pdpt[pml4_slot / 2] = (uint64_t)pdpt;

		kernel_ctx.arch.levels[0].kernel_virts = kernel_virts_pdpt;
		kernel_ctx.arch.levels[1].kernel_virts = kernel_virts_pd;
		kernel_ctx.arch.levels[2].kernel_virts = kernel_virts_pt;
		kernel_ctx.arch.pml4 = pml4;
		kernel_ctx.arch.pml4_phys = pml4_phys;
	}

	asm volatile("mov %0, %%cr3" ::"r"(kernel_ctx.arch.pml4_phys) : "memory");
}

void arch_mm_context_init(struct vm_context *ctx)
{
	ctx->arch.pml4_phys = mm_physical_alloc(0x1000, PM_TYPE_DRAM, true);
	uint64_t *pml4 = (uint64_t *)mm_ptov(ctx->arch.pml4_phys);
	for(int i = 0; i < 256; i++) {
		pml4[i] = 0;
	}
	for(int i = 256; i < 512; i++) {
		pml4[i] = ((uint64_t *)kernel_ctx.arch.pml4)[i];
	}

	ctx->arch.levels[0].kernel_virts = kernel_virts_pdpt;
	ctx->arch.levels[1].kernel_virts = kernel_virts_pd;
	ctx->arch.levels[2].kernel_virts = kernel_virts_pt;
	ctx->arch.pml4 = pml4;
}
