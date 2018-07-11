#include <debug.h>

#include <twzkv.h>
#include <twzobj.h>

static __inline__ unsigned long long rdtsc(void)
{
  unsigned hi, lo;
  __asm__ __volatile__ ("mfence; rdtscp" : "=a"(lo), "=d"(hi));
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

int main()
{
	debug_printf("init - starting\n");

	struct twzkv_item k = {
		.data = "foo",
		.length = 3,
	};

	struct twzkv_item v = {
		.data = "bar",
		.length = 3,
	};

	struct twzkv_item rv;

	struct object index;
	struct object data;
	if(twzkv_create_index(&index)) {
		debug_printf("Failed to create index");
		return 1;
	}
	if(twzkv_create_data(&data)) {
		debug_printf("Failed to create data");
		return 1;
	}

	int r = twzkv_put(&index, &data, &k, &v);
	debug_printf("put: %d\n", r);
	uint64_t start = rdtsc();
	int i;
	for(i=0;i<10000000;i++) {
		r = twzkv_get(&index, &k, &rv);
	}
	uint64_t end = rdtsc();
	debug_printf("(%ld, %ld) diff: %ld (%ld per)\n", end, start, end - start, (end - start) / i);

	debug_printf(":: %s\n", rv.data);
	

	for(;;);
}

