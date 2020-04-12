#include <stdio.h>
#include <twz/_kso.h>
#include <twz/driver/bus.h>
#include <twz/driver/device.h>
#include <twz/driver/processor.h>
#include <twz/driver/system.h>
#include <twz/obj.h>

void print_system(twzobj *sys)
{
	struct bus_repr *rep = twz_bus_getrepr(sys);
	// struct system_header *sh = twz_bus_getbs(sys);
	for(size_t i = 0; i < rep->max_children; i++) {
		twzobj cpu;
		twz_bus_open_child(sys, &cpu, i, FE_READ);

		struct device_repr *dr = twz_device_getrepr(&cpu);
		// struct processor_header *ph = twz_device_getds(&cpu);
		if((dr->device_id >> 24) == 1) {
			printf("MEM\n");
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
					print_system(&bus);
				}
				break;
		}
	}
	return 0;
}
