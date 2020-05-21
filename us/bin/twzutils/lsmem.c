#include <stdio.h>
#include <twz/_kso.h>
#include <twz/driver/bus.h>
#include <twz/driver/device.h>
#include <twz/driver/memory.h>
#include <twz/driver/system.h>
#include <twz/obj.h>

static void print_value(size_t val, bool hr)
{
	if(!hr) {
		printf("%ld", val);
		return;
	}
	static const char *sstr[] = {
		" B",
		"KB",
		"MB",
		"GB",
		"TB",
		"PB",
	};

	int si = 0;
	while(val >= 1024) {
		val /= 1024;
		si++;
	}

	printf("%4ld %s ", val, sstr[si]);
}

void print_system(twzobj *sys, bool hr)
{
	struct bus_repr *rep = twz_bus_getrepr(sys);
	// struct system_header *sh = twz_bus_getbs(sys);
	for(size_t i = 0; i < rep->max_children; i++) {
		twzobj cpu;
		twz_bus_open_child(sys, &cpu, i, FE_READ);

		struct device_repr *dr = twz_device_getrepr(&cpu);
		struct memory_stats_header *mh = twz_device_getds(&cpu);
		if((dr->device_id >> 24) == 1) {
			printf("early page allocation: %ld pages (", mh->stats.pages_early_used);
			print_value(mh->stats.pages_early_used * 0x1000 /*TODO*/, hr);
			printf(")\n");
			printf("  pmap_used: ");
			print_value(mh->stats.pmap_used, hr);
			printf("\ntmpmap_used: ");
			print_value(mh->stats.tmpmap_used, hr);
			printf("\nkernel memory allocator (%ld objects):\n", mh->stats.memalloc_nr_objects);
			printf("    total: ");
			print_value(mh->stats.memalloc_total, hr);
			printf("\n  unfreed: ");
			print_value(mh->stats.memalloc_unfreed, hr);
			printf("\n     free: ");
			print_value(mh->stats.memalloc_free, hr);
			printf("\n     used: ");
			print_value(mh->stats.memalloc_used, hr);

			printf("\nPage Groups\n");

			for(unsigned i = 0; i < mh->nr_page_groups; i++) {
				struct page_stats *ps = &mh->page_stats[i];
				printf("%2d ", i);

				char flags[3] = { ' ', ' ', 0 };
				if(ps->info & PAGE_STATS_INFO_ZERO)
					flags[0] = 'z';
				if(ps->info & PAGE_STATS_INFO_CRITICAL)
					flags[1] = 'c';
				printf("%s ", flags);

				print_value(ps->page_size, hr);
				size_t avail = ps->avail;
				printf("  %8ld", avail);
				print_value(avail * ps->page_size, hr);
				printf("\n");
			}
			/* is CPU */
			break;
		}
	}
}

int main()
{
	twzobj root, bus;
	twz_object_init_guid(&root, 1, FE_READ);

	struct kso_root_repr *r = twz_object_base(&root);
	for(size_t i = 0; i < r->count; i++) {
		struct kso_attachment *k = &r->attached[i];
		if(!k->id || !k->type)
			continue;
		switch(k->type) {
			case KSO_DEVBUS:
				twz_object_init_guid(&bus, k->id, FE_READ);
				struct bus_repr *rep = twz_bus_getrepr(&bus);
				//	struct system_header *sh = twz_bus_getbs(&bus);
				if(rep->bus_type == DEVICE_BT_SYSTEM) {
					print_system(&bus, true /* TODO */);
				}
				break;
		}
	}
	return 0;
}
