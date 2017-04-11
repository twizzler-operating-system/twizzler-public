#include <memory.h>

#define PML4_IDX(v) (((v) >> 39) & 0x1FF)
#define PDPT_IDX(v) (((v) >> 30) & 0x1FF)
#define PD_IDX(v)   (((v) >> 21) & 0x1FF)
#define PT_IDX(v)   (((v) >> 12) & 0x1FF)

#define RECUR_ATTR_MASK (VM_MAP_EXEC | VM_MAP_USER | VM_MAP_WRITE | VM_MAP_ACCESSED | VM_MAP_DIRTY)

static inline void test_and_allocate(uintptr_t *loc, uint64_t attr)
{
	if(!*loc) {
		*loc = (uintptr_t)mm_physical_alloc(0x1000, PM_TYPE_DRAM, true) | (attr & RECUR_ATTR_MASK);
	}
}

#define GET_VIRT_TABLE(x) ((uintptr_t *)(((x) & VM_PHYS_MASK) + PHYSICAL_MAP_START))

#define PAGE_PRESENT (1ull << 0)
#define PAGE_LARGE   (1ull << 7)

bool arch_vm_map(struct vm_context *ctx, uintptr_t virt, uintptr_t phys, int level, uint64_t flags)
{
	/* translate flags for NX bit (toggle) */
	flags ^= VM_MAP_EXEC;

	int pml4_idx = PML4_IDX(virt);
	int pdpt_idx = PDPT_IDX(virt);
	int pd_idx   = PD_IDX(virt);
	int pt_idx   = PT_IDX(virt);

	if(ctx == NULL) {
		ctx = current_thread->ctx;
	}

	uintptr_t *pml4 = GET_VIRT_TABLE(ctx->arch.pml4_phys);
	test_and_allocate(&pml4[pml4_idx]);
	
	uintptr_t *pdpt = GET_VIRT_TABLE(pml4[pml4_idx]);
	if(level == 2) {
		if(pdpt[pdpt_idx]) {
			return false;
		}
		pdpt[pdpt_idx] = phys | attr | PAGE_PRESENT | PAGE_LARGE;
	} else {
		test_and_allocate(&pdpt[pdpt_idx]);
		uintptr_t *pd = GET_VIRT_TABLE(pdpt[pdpt_idx]);

		if(level == 1) {
			if(pd[pd_idx]) {
				return false;
			}
			pd[pd_idx] = phys | attr | PAGE_PRESENT | PAGE_LARGE;
		} else {
			test_and_allocate(&pd[pd_idx]);
			uintptr_t *pt = GET_VIRT_TABLE(pd[pd_idx]);
			if(pt[pt_idx]) {
				return false;
			}
			pt[pt_idx] = phys | attr | PAGE_PRESENT;
		}
	}
	return true;
}

