#include <stdio.h>
#include <twz/_kso.h>
#include <twz/driver/bus.h>
#include <twz/driver/device.h>
#include <twz/driver/processor.h>
#include <twz/driver/system.h>
#include <twz/obj.h>

void print_system(struct object *sys)
{
	struct bus_repr *rep = twz_bus_getrepr(sys);
	struct system_header *sh = twz_bus_getbs(sys);
	for(size_t i = 0; i < rep->max_children; i++) {
		struct object cpu;
		twz_bus_open_child(sys, &cpu, i, FE_READ);

		struct device_repr *dr = twz_device_getrepr(&cpu);
		struct processor_header *ph = twz_device_getds(&cpu);
		printf("CPU %d\n", dr->device_id);

		printf("  thr_switch : %-ld\n", ph->stats.thr_switch);
		printf("  ext_intr   : %-ld\n", ph->stats.ext_intr);
		printf("  int_intr   : %-ld\n", ph->stats.int_intr);
		printf("  running    : %-ld\n", ph->stats.running);
		printf("  sctx_switch: %-ld\n", ph->stats.sctx_switch);
		printf("  shootdowns : %-ld\n", ph->stats.shootdowns);
		printf("  syscalls   : %-ld\n", ph->stats.syscalls);
	}
}

int main()
{
	struct object root, bus;
	twz_object_open(&root, 1, FE_READ);

	struct kso_root_repr *r = twz_obj_base(&root);
	for(size_t i = 0; i < r->count; i++) {
		struct kso_attachment *k = &r->attached[i];
		if(!k->id || !k->type)
			continue;
		switch(k->type) {
			case KSO_DEVBUS:
				twz_object_open(&bus, k->id, FE_READ);
				struct bus_repr *rep = twz_bus_getrepr(&bus);
				struct system_header *sh = twz_bus_getbs(&bus);
				if(rep->bus_type == DEVICE_BT_SYSTEM) {
					print_system(&bus);
				}
		}
	}
	return 0;
}
