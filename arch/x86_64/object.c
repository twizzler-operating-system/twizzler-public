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

static bool x86_64_ept_getmap(uintptr_t ept_phys,
  uintptr_t virt,
  uintptr_t *phys,
  int *level,
  uint64_t *flags)
{
	int pml4_idx = PML4_IDX(virt);
	int pdpt_idx = PDPT_IDX(virt);
	int pd_idx = PD_IDX(virt);
	int pt_idx = PT_IDX(virt);

	uintptr_t *pml4 = GET_VIRT_TABLE(ept_phys);
	if(!pml4[pml4_idx])
		return false;

	uintptr_t *pdpt = GET_VIRT_TABLE(pml4[pml4_idx]);

	if(!pdpt[pdpt_idx])
		return false;
	else if(pdpt[pdpt_idx] & PAGE_LARGE) {
		if(phys)
			*phys = pdpt[pdpt_idx] & VM_PHYS_MASK;
		if(flags)
			*flags = pdpt[pdpt_idx] & ~VM_PHYS_MASK;
		if(level)
			*level = 2;
		return true;
	}

	uintptr_t *pd = GET_VIRT_TABLE(pdpt[pdpt_idx]);

	if(!pd[pd_idx])
		return false;
	else if(pd[pd_idx] & PAGE_LARGE) {
		if(phys)
			*phys = pd[pd_idx] & VM_PHYS_MASK;
		if(flags)
			*flags = pd[pd_idx] & ~VM_PHYS_MASK;
		if(level)
			*level = 1;
		return true;
	}

	uintptr_t *pt = GET_VIRT_TABLE(pd[pd_idx]);

	if(!pt[pt_idx])
		return false;
	if(phys)
		*phys = pt[pt_idx] & VM_PHYS_MASK;
	if(flags)
		*flags = pt[pt_idx] & ~VM_PHYS_MASK;
	if(level)
		*level = 0;
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
	test_and_allocate(&pml4[pml4_idx], flags);

	uintptr_t *pdpt = GET_VIRT_TABLE(pml4[pml4_idx]);
	// printk("mapping slot %ld -> %lx\n", obj->slot, obj->arch.pt_root | ef);
	pdpt[pdpt_idx] = obj->arch.pt_root | ef;
}

void arch_object_unmap_page(struct object *obj, size_t idx)
{
	uintptr_t *pd = mm_ptov(obj->arch.pt_root);
	uintptr_t virt = idx * mm_page_size(0);
	int pd_idx = PD_IDX(virt);
	int pt_idx = PT_IDX(virt);
	uint64_t flags = 0;
	if(pd[pd_idx] & PAGE_LARGE) {
		pd[pd_idx] = 0;
	} else {
		uint64_t *pt = GET_VIRT_TABLE(pd[pd_idx]);
		if(pt) {
			pt[pt_idx] = 0;
		}
	}
}

bool arch_object_map_page(struct object *obj, struct page *page, size_t idx)
{
	uintptr_t *pd = mm_ptov(obj->arch.pt_root);
	assert(page->level == 0 || page->level == 1);
	assert(page->addr);
	uintptr_t virt = idx * mm_page_size(0);
	int pd_idx = PD_IDX(virt);
	int pt_idx = PT_IDX(virt);
	uint64_t flags = 0;
	// printk("map_page: %ld (%lx)\n", idx, virt);
	switch(PAGE_CACHE_TYPE(page)) {
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
	// printk("  fl = %lx %llx %llx %llx %x\n", flags, EPT_READ, EPT_WRITE, EPT_EXEC,
	// EPT_IGNORE_PAT);

	if(page->level == 1) {
		pd[pd_idx] = page->addr | flags | PAGE_LARGE;
		//	printk("  writing pd[%d] = %llx\n", pd_idx, page->addr | flags | PAGE_LARGE);
	} else {
		test_and_allocate(&pd[pd_idx], EPT_READ | EPT_WRITE | EPT_EXEC);
		//	printk("  pd[%d] = %lx\n", pd_idx, pd[pd_idx]);
		uint64_t *pt = GET_VIRT_TABLE(pd[pd_idx]);
		pt[pt_idx] = page->addr | flags;
		//	printk("  writing pt[%d] = %lx (%lx %lx)\n", pt_idx, page->addr | flags, page->addr,
		//flags);
	}
	return true;
}

void arch_object_init(struct object *obj)
{
	obj->arch.pt_root = mm_physical_alloc(0x1000, PM_TYPE_DRAM, true);
}