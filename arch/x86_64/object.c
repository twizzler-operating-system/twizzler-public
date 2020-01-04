#include <object.h>
#include <page.h>
#include <processor.h>

#include <arch/x86_64-vmx.h>

static inline void test_and_allocate(uintptr_t *loc, uint64_t attr)
{
	if(!*loc) {
		*loc = (uintptr_t)mm_physical_alloc(0x1000, PM_TYPE_DRAM, true) | (attr & RECUR_ATTR_MASK);
	}
}

bool arch_object_getmap_slot_flags(struct object *obj, uint64_t *flags)
{
	uint64_t ef = 0;

	uintptr_t ept_phys = current_thread->active_sc->arch.ept_root;
	uintptr_t virt = obj->slot * (1024 * 1024 * 1024ull);
	int pml4_idx = PML4_IDX(virt);
	int pdpt_idx = PDPT_IDX(virt);

	uintptr_t *pml4 = GET_VIRT_TABLE(ept_phys);
	if(!pml4[pml4_idx])
		return false;

	uintptr_t *pdpt = GET_VIRT_TABLE(pml4[pml4_idx]);
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

	uintptr_t ept_phys = current_thread->active_sc->arch.ept_root;
	uintptr_t virt = obj->slot * (1024 * 1024 * 1024ull);
	int pml4_idx = PML4_IDX(virt);
	int pdpt_idx = PDPT_IDX(virt);

	uintptr_t *pml4 = GET_VIRT_TABLE(ept_phys);
	test_and_allocate(&pml4[pml4_idx], EPT_READ | EPT_WRITE | EPT_EXEC);

	uintptr_t *pdpt = GET_VIRT_TABLE(pml4[pml4_idx]);
	pdpt[pdpt_idx] = obj->arch.pt_root | ef;
}

void arch_object_unmap_page(struct object *obj, size_t idx)
{
	uintptr_t *pd = mm_ptov(obj->arch.pt_root);
	uintptr_t virt = idx * mm_page_size(0);
	int pd_idx = PD_IDX(virt);
	int pt_idx = PT_IDX(virt);
	if(pd[pd_idx] & PAGE_LARGE) {
		pd[pd_idx] = 0;
	} else {
		uint64_t *pt = GET_VIRT_TABLE(pd[pd_idx]);
		if(pt) {
			pt[pt_idx] = 0;
		}
	}
}

bool arch_object_map_flush(struct object *obj, size_t virt)
{
	uintptr_t *pd = mm_ptov(obj->arch.pt_root);
	int pd_idx = PD_IDX(virt);
	int pt_idx = PT_IDX(virt);

	if(pd[pd_idx] & PAGE_LARGE) {
		arch_processor_clwb(pd[pd_idx]);
	} else if(pd[pd_idx]) {
		uint64_t *pt = GET_VIRT_TABLE(pd[pd_idx]);
		arch_processor_clwb(pt[pt_idx]);
	}
	return true;
}

bool arch_object_map_page(struct object *obj, struct objpage *op)
{
	uintptr_t *pd = mm_ptov(obj->arch.pt_root);
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
		pd[pd_idx] = op->page->addr | flags | PAGE_LARGE;
	} else {
		test_and_allocate(&pd[pd_idx], EPT_READ | EPT_WRITE | EPT_EXEC);
		uint64_t *pt = GET_VIRT_TABLE(pd[pd_idx]);
		pt[pt_idx] = op->page->addr | flags;
	}
	return true;
}

void arch_object_init(struct object *obj)
{
	obj->arch.pt_root = mm_physical_alloc(0x1000, PM_TYPE_DRAM, true);
}
