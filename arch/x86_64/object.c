#include <object.h>
#include <page.h>
#include <processor.h>
#include <secctx.h>
#include <slots.h>

#include <arch/x86_64-vmx.h>

/* TODO: get rid of this */
extern struct object_space _bootstrap_object_space;

bool arch_object_getmap_slot_flags(struct object_space *space, struct slot *slot, uint64_t *flags)
{
	uint64_t ef = 0;

	if(!space)
		space = current_thread ? &current_thread->active_sc->space : &_bootstrap_object_space;

	uintptr_t virt = SLOT_TO_OADDR(slot->num);
	int pml4_idx = PML4_IDX(virt);
	int pdpt_idx = PDPT_IDX(virt);

	if(!space->arch.ept[pml4_idx])
		return false;

	uintptr_t *pdpt = space->arch.pdpts[pml4_idx];
	if(!pdpt[pdpt_idx])
		return false;
	ef = pdpt[pdpt_idx] & ~EPT_PAGE_MASK;
	if(flags) {
		*flags = 0;
		if(ef & EPT_READ)
			*flags |= OBJSPACE_READ;
		if(ef & EPT_WRITE)
			*flags |= OBJSPACE_WRITE;
		if(ef & EPT_EXEC)
			*flags |= OBJSPACE_EXEC_U;
	}
	return true;
}

void arch_object_unmap_all(struct object *obj)
{
	assert(atomic_load(&obj->mapcount.count) == 0);
	assert(obj->slot == NULL);
	assert(obj->kslot == NULL);
	/* TODO: free things */
	for(int pd_idx = 0; pd_idx < 512; pd_idx++) {
		if(obj->arch.pd[pd_idx] & PAGE_LARGE) {
			obj->arch.pd[pd_idx] = 0;
		} else {
			uint64_t *pt = obj->arch.pts[pd_idx];
			if(pt) {
				for(int pt_idx = 0; pt_idx < 512; pt_idx++) {
					pt[pt_idx] = 0;
				}
			}
		}
	}
	int x;
	asm volatile("invept %0, %%rax" ::"m"(x), "r"(0));
}

void arch_object_remap_cow(struct object *obj)
{
	for(int pd_idx = 0; pd_idx < 512; pd_idx++) {
		if(obj->arch.pd[pd_idx] & PAGE_LARGE) {
			obj->arch.pd[pd_idx] &= ~EPT_WRITE;
		} else {
			uint64_t *pt = obj->arch.pts[pd_idx];
			if(pt) {
				for(int pt_idx = 0; pt_idx < 512; pt_idx++) {
					pt[pt_idx] &= ~EPT_WRITE;
				}
			}
		}
	}
	int x;
	asm volatile("invept %0, %%rax" ::"m"(x), "r"(0));
	asm volatile("mov %%cr3, %%rax; mov %%rax, %%cr3" ::: "memory", "rax");
	/* TODO */
	if(current_thread)
		processor_send_ipi(
		  PROCESSOR_IPI_DEST_OTHERS, PROCESSOR_IPI_SHOOTDOWN, NULL, PROCESSOR_IPI_NOWAIT);
}

void arch_object_unmap_slot(struct object_space *space, struct slot *slot)
{
	if(!space)
		space = current_thread ? &current_thread->active_sc->space : &_bootstrap_object_space;

	uintptr_t virt = SLOT_TO_OADDR(slot->num);
	int pml4_idx = PML4_IDX(virt);
	int pdpt_idx = PDPT_IDX(virt);

	if(!space->arch.ept[pml4_idx]) {
		return;
	}
	uint64_t *pdpt = space->arch.pdpts[pml4_idx];
	if(pdpt[pdpt_idx])
		space->arch.counts[pml4_idx]--;
	pdpt[pdpt_idx] = 0;
	int x;
	asm volatile("invept %0, %%rax" ::"m"(x), "r"(0));
	if(space->arch.counts[pml4_idx] == 0) {
		mm_memory_dealloc(space->arch.pdpts[pml4_idx]);
		space->arch.ept[pml4_idx] = 0;
	}
}

void arch_object_map_slot(struct object_space *space,
  struct object *obj,
  struct slot *slot,
  uint64_t flags)
{
	uint64_t ef = 0;
	if(flags & OBJSPACE_READ)
		ef |= EPT_READ;
	if(flags & OBJSPACE_WRITE)
		ef |= EPT_WRITE;
	if(flags & OBJSPACE_EXEC_U)
		ef |= EPT_EXEC;

	if(!space)
		space = current_thread ? &current_thread->active_sc->space : &_bootstrap_object_space;

	uintptr_t virt = SLOT_TO_OADDR(slot->num);
	int pml4_idx = PML4_IDX(virt);
	int pdpt_idx = PDPT_IDX(virt);

	if(!space->arch.ept[pml4_idx]) {
		space->arch.pdpts[pml4_idx] = (void *)mm_memory_alloc(0x1000, PM_TYPE_DRAM, true);
		space->arch.ept[pml4_idx] =
		  mm_vtop(space->arch.pdpts[pml4_idx]) | EPT_READ | EPT_WRITE | EPT_EXEC;
	}
	uint64_t *pdpt = space->arch.pdpts[pml4_idx];
	if(pdpt[pdpt_idx] == 0) {
		space->arch.counts[pml4_idx]++;
	}
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
	int x;
	/* TODO: better invalidation scheme */
	asm volatile("invept %0, %%rax" ::"m"(x), "r"(0));
	if(current_thread)
		processor_send_ipi(
		  PROCESSOR_IPI_DEST_OTHERS, PROCESSOR_IPI_SHOOTDOWN, NULL, PROCESSOR_IPI_NOWAIT);
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

bool arch_object_premap_page(struct object *obj, int idx, int level)
{
	if(level == 1)
		return true;
	uintptr_t virt = idx * mm_page_size(level);
	int pd_idx = PD_IDX(virt);
	/* map with ALL permissions; we'll restrict permissions at a higher level */

	if(!obj->arch.pts[pd_idx]) {
		obj->arch.pts[pd_idx] = (void *)mm_memory_alloc(0x1000, PM_TYPE_DRAM, true);
		obj->arch.pd[pd_idx] = mm_vtop(obj->arch.pts[pd_idx]) | EPT_READ | EPT_WRITE | EPT_EXEC;
	}
	return true;
}

bool arch_object_getmap(struct object *obj,
  uintptr_t off,
  uintptr_t *phys,
  int *level,
  uint64_t *flags)
{
	int pd_idx = PD_IDX(off);
	int pt_idx = PT_IDX(off);
	if(!(obj->arch.pd[pd_idx]))
		return false;
	uintptr_t p = 0;
	uint64_t f = 0;
	int l = 0;
	if(obj->arch.pd[pd_idx] & PAGE_LARGE) {
		l = 1;
		p = obj->arch.pd[pd_idx] & EPT_PAGE_MASK;
		f = obj->arch.pd[pd_idx] & ~EPT_PAGE_MASK;
	} else {
		uint64_t *pt = obj->arch.pts[pd_idx];
		if(!(pt[pt_idx]))
			return false;
		l = 0;
		p = pt[pt_idx] & EPT_PAGE_MASK;
		f = pt[pt_idx] & ~EPT_PAGE_MASK;
	}
	if(level)
		*level = l;
	if(flags)
		*flags = f;
	if(phys)
		*phys = p;
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
	if((op->flags & OBJPAGE_COW) && !(obj->flags & OF_KERNEL)) {
		flags &= ~EPT_WRITE;
	}

	if(flags & EPT_WRITE) {
		op->page->flags &= ~PAGE_ZERO;
	}

	if(op->page->level == 1) {
		obj->arch.pd[pd_idx] = op->page->addr | flags | PAGE_LARGE;
	} else {
		if(!obj->arch.pts[pd_idx]) {
			obj->arch.pts[pd_idx] = (void *)mm_memory_alloc(0x1000, PM_TYPE_DRAM, true);
			obj->arch.pd[pd_idx] = mm_vtop(obj->arch.pts[pd_idx]) | EPT_READ | EPT_WRITE | EPT_EXEC;
			//	printk(";; %lx\n", obj->arch.pd[pd_idx]);
		}
		uint64_t *pt = obj->arch.pts[pd_idx];
		pt[pt_idx] = op->page->addr | flags;
		// printk("MAPPED %d %d %lx->%lx\n", pd_idx, pt_idx, virt, op->page->addr | flags);
	}
	return true;
}

void arch_object_init(struct object *obj)
{
	obj->arch.pd = (void *)mm_memory_alloc(0x1000, PM_TYPE_DRAM, true);
	obj->arch.pt_root = mm_vtop(obj->arch.pd);
	// printk("objinit: %p %lx\n", obj->arch.pd, obj->arch.pt_root);
	obj->arch.pts = (void *)mm_memory_alloc(512 * sizeof(void *), PM_TYPE_DRAM, true);
}
/*
uint64_t *pml4 = mm_ptov(pml4phys);
    memset(pml4, 0, mm_page_size(0));
    pml4[0] = mm_physical_early_alloc();
    uint64_t *pdpt = mm_ptov(pml4[0]);
    pml4[0] |= EPT_READ | EPT_WRITE | EPT_EXEC;
    memset(pdpt, 0, mm_page_size(0));
    pdpt[0] |= EPT_IGNORE_PAT | EPT_MEMTYPE_WB | EPT_READ | EPT_WRITE | EPT_EXEC | PAGE_LARGE;

    _bootstrap_object_space.arch.ept = pml4;
    _bootstrap_object_space.arch.ept_phys = pml4phys;
    _bootstrap_object_space.arch.pdpts = mm_virtual_early_alloc();
    _bootstrap_object_space.arch.pdpts[0] = pdpt;
*/
void arch_object_space_init(struct object_space *space)
{
	space->arch.ept = mm_memory_alloc(0x1000, PM_TYPE_DRAM, true);
	space->arch.ept_phys = mm_vtop(space->arch.ept);
	space->arch.pdpts = (void *)mm_memory_alloc(512 * sizeof(void *), PM_TYPE_DRAM, true);
	/* 0th slot is bootstrap, we'll need that one. */
	space->arch.pdpts[0] = mm_memory_alloc(0x1000, PM_TYPE_DRAM, true);
	space->arch.counts = mm_memory_alloc(512 * sizeof(size_t), PM_TYPE_DRAM, true);
	space->arch.ept[0] = mm_vtop(space->arch.pdpts[0]) | EPT_READ | EPT_WRITE | EPT_EXEC;
	space->arch.pdpts[0][0] =
	  EPT_READ | EPT_WRITE | EPT_EXEC | EPT_IGNORE_PAT | EPT_MEMTYPE_WB | PAGE_LARGE;
	// printk(":: ept[0]=%lx ; pdpt[0]=%lx\n", space->arch.ept[0], space->arch.pdpts[0][0]);
	// debug_print_backtrace();
}

void arch_object_space_destroy(struct object_space *space)
{
	panic("TODO");
	mm_memory_dealloc(space->arch.ept);
	mm_memory_dealloc(space->arch.pdpts);
}

void arch_object_destroy(struct object *obj)
{
	for(int i = 0; i < 512; i++) {
		if(obj->arch.pts[i]) {
			mm_memory_dealloc(obj->arch.pts[i]);
			obj->arch.pts[i] = NULL;
		}
	}
	mm_memory_dealloc(obj->arch.pd);
	mm_memory_dealloc(obj->arch.pts);
	obj->arch.pts = NULL;
	obj->arch.pd = NULL;
	obj->arch.pt_root = 0;
}
