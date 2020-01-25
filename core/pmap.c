#include <memory.h>
#include <slots.h>
#include <spinlock.h>

static _Atomic uintptr_t start = 0;

void *pmap_allocate(uintptr_t phys, size_t len, long flags)
{
	assert(len > 0);
	len = align_up(len, mm_page_size(0));
	uintptr_t s = atomic_fetch_add(&start, len);
	for(uintptr_t i = s; i < s + len; i += mm_page_size(0), phys += mm_page_size(0)) {
		arch_vm_map(NULL, i, phys, 0, flags | VM_MAP_GLOBAL);
	}
	return (void *)s;
}
