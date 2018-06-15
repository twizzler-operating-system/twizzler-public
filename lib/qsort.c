#include <string.h>

static void swap(void *base, size_t size, int a, int b)
{
	char data[size];
	memcpy(data, (void *)((uintptr_t)base + a * size), size);
	memcpy((void *)((uintptr_t)base + a * size), (void *)((uintptr_t)base + b * size), size);
	memcpy((void *)((uintptr_t)base + b * size), data, size);

}

void qsort(void* base, size_t num, size_t size, int (*compar)(const void*,const void*))
{
	if(num <= 1)
		return;
	/* TODO (perf) */
	//int pivot = random_u32() % num;
	int pivot = 0;

	size_t part = 0;
	swap(base, size, pivot, 0);
	for(size_t i=1;i<num;i++) {
		void *a = (void *)((uintptr_t)base + size * i);

		if(compar(a, base) < 0) {
			swap(base, size, ++part, i);
		}
	}
	swap(base, size, 0, part);
	if(part > 0)
		qsort(base, part, size, compar);
	if(num > part)
		qsort((void *)((uintptr_t)base + size * (part + 1)), (num - part) - 1, size, compar);
}

