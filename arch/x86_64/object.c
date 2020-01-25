#include <object.h>
#include <page.h>
#include <processor.h>
#include <secctx.h>
#include <slots.h>

#include <arch/x86_64-vmx.h>

/* TODO: get rid of this */
extern struct object_space _bootstrap_object_space;

static inline void test_and_allocate(uintptr_t *loc, uint64_t attr)
{
	if(!*loc) {
		*loc = (uintptr_t)mm_physical_alloc(0x1000, PM_TYPE_DRAM, true) | (attr & RECUR_ATTR_MASK);
	}
}

bool arch_object_getmap_slot_flags(struct object *obj, uint64_t *flags)
{
	uint64_t ef = 0;

	struct object_space *space =
	  current_thread ? &current_thread->active_sc->space : &_bootstrap_object_space;
	if(!obj->slot)
		return false;
	uintptr_t virt = obj->slot->num * OBJ_MAXSIZE;
	int pml4_idx = PML4_IDX(virt);
	int pdpt_idx = PDPT_IDX(virt);

	if(!space->arch.ept[pml4_idx])
		return false;

	uintptr_t *pdpt = space->arch.pdpts[pml4_idx];
	if(!pdpt[pdpt_idx])
		return false;
	assert(obj->arch.pt_root == (pdpt[pdpt_idx] & EPT_PAGE_MASK));
	ef = pdpt[pdpt_idx] & ~EPT_PAGE_MASK;
	if(flags) {
		if(ef & EPT_READ)
			*flags |= OBJSPACE_READ;
		if(ef & EPT_WRITE)
			*flags |= OBJSPACE_WRITE;
		if(ef & EPT_EXEC)
			*flags |= OBJSPACE_EXEC_U;
	}
	return true;
}

void arch_object_map_slot(struct object *obj, uint64_t flags)
{
	uint64_t ef = 0;
	if(flags & OBJSPACE_READ)
		ef |= EPT_READ;
	if(flags & OBJSPACE_WRITE)
		ef |= EPT_WRITE;
	if(flags & OBJSPACE_EXEC_U)
		ef |= EPT_EXEC;

	struct object_space *space =
	  current_thread ? &current_thread->active_sc->space : &_bootstrap_object_space;
	assert(obj->slot);
	uintptr_t virt = obj->slot->num * OBJ_MAXSIZE;
	int pml4_idx = PML4_IDX(virt);
	int pdpt_idx = PDPT_IDX(virt);

	if(!space->arch.ept[pml4_idx]) {
		space->arch.pdpts[pml4_idx] = mm_memory_alloc(0x1000, PM_TYPE_DRAM, true);
		space->arch.ept[pml4_idx] =
		  mm_vtop(space->arch.pdpts[pml4_idx]) | EPT_READ | EPT_WRITE | EPT_EXEC;
	}
	uint64_t *pdpt = space->arch.pdpts[pml4_idx];
	pdpt[pdpt_idx] = obj->arch.pt_root | ef;
}

/* TODO: switch to passing an objpage */
void arch_object_unmap_page(struct object *obj, size_t idx)
{
	uintptr_t virt = idx * mm_page_size(0);
	int pd_idx = PD_IDX(virt);
	int pt_idx = PT_IDX(virt);
	if(obj->arch.pd[pd_idx] & PAGE_LARGE) {
		obj->arch.pd[pd_idx] = 0;
	} else {
		uint64_t *pt = obj->arch.pts[pd_idx];
		if(pt) {
			pt[pt_idx] = 0;
		}
	}
}

bool arch_object_map_flush(struct object *obj, size_t virt)
{
	int pd_idx = PD_IDX(virt);
	int pt_idx = PT_IDX(virt);

	if(obj->arch.pd[pd_idx] & PAGE_LARGE) {
		arch_processor_clwb(obj->arch.pd[pd_idx]);
	} else if(obj->arch.pd[pd_idx]) {
		uint64_t *pt = obj->arch.pts[pd_idx];
		arch_processor_clwb(pt[pt_idx]);
	}
	return true;
}

bool arch_object_map_page(struct object *obj, struct objpage *op)
{
	assert(op->page->level == 0 || op->page->level == 1);
	assert(op->page->addr);
	assert((op->page->addr & (mm_page_size(op->page->level) - 1)) == 0);
	uintptr_t virt = op->idx * mm_page_size(op->page->level);
	int pd_idx = PD_IDX(virt);
	int pt_idx = PT_IDX(virt);
	uint64_t flags = 0;
	switch(PAGE_CACHE_TYPE(op->page)) {
		default:
		case PAGE_CACHE_WB:
			flags = EPT_MEMTYPE_WB;
			break;
		case PAGE_CACHE_UC:
			flags = EPT_MEMTYPE_UC;
			break;
		case PAGE_CACHE_WT:
			flags = EPT_MEMTYPE_WT;
			break;
		case PAGE_CACHE_WC:
			flags = EPT_MEMTYPE_WC;
			break;
	}

	/* map with ALL permissions; we'll restrict permissions at a higher level */
	flags |= EPT_READ | EPT_WRITE | EPT_EXEC | EPT_IGNORE_PAT;

	if(op->page->level == 1) {
		obj->arch.pd[pd_idx] = op->page->addr | flags | PAGE_LARGE;
	} else {
		if(!obj->arch.pts[pd_idx]) {
			obj->arch.pts[pd_idx] = mm_memory_alloc(0x1000, PM_TYPE_DRAM, true);
			obj->arch.pd[pd_idx] = mm_vtop(obj->arch.pts[pd_idx]) | EPT_READ | EPT_WRITE | EPT_EXEC;
		}
		uint64_t *pt = obj->arch.pts[pd_idx];
		pt[pt_idx] = op->page->addr | flags;
	}
	return true;
}

void arch_object_init(struct object *obj)
{
	obj->arch.pd = mm_memory_alloc(0x1000, PM_TYPE_DRAM, true);
	obj->arch.pt_root = mm_vtop(obj->arch.pd);
	obj->arch.pts = mm_memory_alloc(512 * sizeof(void *), PM_TYPE_DRAM, true);
}

void arch_object_space_init(struct object_space *space)
{
	space->arch.ept = mm_memory_alloc(0x1000, PM_TYPE_DRAM, true);
	space->arch.ept_phys = mm_vtop(space->arch.ept);
	space->arch.pdpts = mm_memory_alloc(512 * sizeof(void *), PM_TYPE_DRAM, true);
}

void arch_object_space_destroy(struct object_space *space)
{
	panic("TODO");
	mm_memory_dealloc(space->arch.ept);
	mm_memory_dealloc(space->arch.pdpts);
}

void arch_object_destroy(struct object *obj)
{
	panic("TODO");
}
