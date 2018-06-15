#include <memory.h>
#include <processor.h>

#define PML4_IDX(v) (((v) >> 39) & 0x1FF)
#define PDPT_IDX(v) (((v) >> 30) & 0x1FF)
#define PD_IDX(v)   (((v) >> 21) & 0x1FF)
#define PT_IDX(v)   (((v) >> 12) & 0x1FF)
#define PAGE_PRESENT (1ull << 0)
#define PAGE_LARGE   (1ull << 7)

#define RECUR_ATTR_MASK (VM_MAP_EXEC | VM_MAP_USER | VM_MAP_WRITE | VM_MAP_ACCESSED | VM_MAP_DIRTY)

static inline void test_and_allocate(uintptr_t *loc, uint64_t attr)
{
	if(!*loc) {
		*loc = (uintptr_t)mm_physical_alloc(0x1000, PM_TYPE_DRAM, true) | (attr & RECUR_ATTR_MASK) | PAGE_PRESENT;
	}
}

#define GET_VIRT_TABLE(x) ((uintptr_t *)mm_ptov(((x) & VM_PHYS_MASK)))

static bool __do_vm_map(uintptr_t pml4_phys, uintptr_t virt, uintptr_t phys, int level, uint64_t flags)
{
	/* translate flags for NX bit (toggle) */
	flags ^= VM_MAP_EXEC;

	int pml4_idx = PML4_IDX(virt);
	int pdpt_idx = PDPT_IDX(virt);
	int pd_idx   = PD_IDX(virt);
	int pt_idx   = PT_IDX(virt);

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

bool arch_vm_getmap(struct vm_context *ctx, uintptr_t virt, uintptr_t *phys, int *level, uint64_t *flags)
{
	if(ctx == NULL) {
		ctx = current_thread->ctx;
	}
	int pml4_idx = PML4_IDX(virt);
	int pdpt_idx = PDPT_IDX(virt);
	int pd_idx   = PD_IDX(virt);
	int pt_idx   = PT_IDX(virt);

	uintptr_t p, f;
	int l;
	uintptr_t *pml4 = GET_VIRT_TABLE(ctx->arch.pml4_phys);
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
	if(phys) *phys = p;
	if(flags) *flags = f;
	if(level) *level = l;

	return true;
}

bool arch_vm_unmap(struct vm_context *ctx, uintptr_t virt)
{
	if(ctx == NULL) {
		ctx = current_thread->ctx;
	}
	int pml4_idx = PML4_IDX(virt);
	int pdpt_idx = PDPT_IDX(virt);
	int pd_idx   = PD_IDX(virt);
	int pt_idx   = PT_IDX(virt);

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
	if(ctx == NULL) {
		ctx = current_thread->ctx;
	}
	return __do_vm_map(ctx->arch.pml4_phys, virt, phys, level, flags);
}

#include <object.h>
#define MB (1024ul * 1024ul)
void arch_vm_map_object(struct vm_context *ctx, struct vmap *map, struct object *obj)
{
	if(obj->slot == -1) {
		panic("tried to map an unslotted object");
	}
	uintptr_t vaddr = map->slot * mm_page_size(MAX_PGLEVEL);
	uintptr_t oaddr = obj->slot * mm_page_size(obj->pglevel);
	for(int i=0;i<512;i++) {
		//printk(":: %lx -> %lx\n", vaddr + i*(2*MB), oaddr + i*(2*MB));
		/* TODO: also, actually allow null pointers to work */
		if(arch_vm_map(ctx, vaddr + i * (2*MB), oaddr + i * (2*MB), 1, VM_MAP_USER | VM_MAP_EXEC | VM_MAP_WRITE) == false) panic("remap failed"); //TODO: fix flags
	}
}

uint64_t *kernel_pml4;
extern uint64_t boot_pml4;
#define PHYS_LOAD_ADDRESS (KERNEL_PHYSICAL_BASE + KERNEL_LOAD_OFFSET)
#define PHYS_ADDR_DELTA (KERNEL_VIRTUAL_BASE + KERNEL_LOAD_OFFSET - PHYS_LOAD_ADDRESS)
#define PHYS(x) ((x) - PHYS_ADDR_DELTA)
void arch_mm_switch_context(struct vm_context *ctx)
{
	if(ctx == NULL) {
		asm volatile("mov %0, %%cr3" :: "r"(mm_vtop(kernel_pml4)) : "memory");
	} else {
		asm volatile("mov %0, %%cr3" :: "r"(ctx->arch.pml4_phys) : "memory");
	}
}

extern int kernel_end, kernel_start;
static void _remap(void)
{
	kernel_pml4 = (void *)mm_virtual_alloc(0x1000, PM_TYPE_DRAM, true);
	uintptr_t kpml4_phys = mm_vtop(kernel_pml4);

	uintptr_t end = ((uintptr_t)&kernel_end) & ~(mm_page_size(0)-1);
	uintptr_t start = ((uintptr_t)&kernel_start) & ~(mm_page_size(0)-1);
	/* TODO (sec): W^X */
	for(uintptr_t i = start;i <= end;i += mm_page_size(0)) {
		__do_vm_map(kpml4_phys, i, PHYS(i), 0, VM_MAP_EXEC | VM_MAP_WRITE | VM_MAP_GLOBAL);
	}

	start = PHYSICAL_MAP_START;
	end = PHYSICAL_MAP_END & ~(mm_page_size(2)-1);
	for(uintptr_t i = start;i < end;i += mm_page_size(2)) {
		__do_vm_map(kpml4_phys, i, mm_vtop((void *)i), 2, VM_MAP_EXEC | VM_MAP_WRITE | VM_MAP_GLOBAL);
	}

}

__initializer
static void _x86_64_init_vm(void)
{
	_remap();
	printk("Switching to new page tables\n");
	asm volatile("mov %0, %%cr3" :: "r"(mm_vtop(kernel_pml4)) : "memory");
}

void arch_mm_context_init(struct vm_context *ctx)
{
	ctx->arch.pml4_phys = mm_physical_alloc(0x1000, PM_TYPE_DRAM, true);
	uint64_t *pml4 = (uint64_t *)mm_ptov(ctx->arch.pml4_phys);
	for(int i=0;i<256;i++) {
		pml4[i] = 0;
	}
	for(int i=256;i<512;i++) {
		pml4[i] = kernel_pml4[i];
	}
}

