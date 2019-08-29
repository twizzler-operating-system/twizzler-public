#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <twz/debug.h>
#include <twz/thread.h>

#include "pcie.h"

struct object pcie_cs_obj;
objid_t pcie_cs_oid;

static bool pcief_capability_get(struct pcie_function *pf, int id, union pcie_capability_ptr *cap)
{
	if(pf->config->device.cap_ptr) {
		size_t offset = pf->config->device.cap_ptr;
		do {
			cap->header = (struct pcie_capability_header *)((char *)pf->config + offset);

			if(cap->header->capid == id)
				return true;
			offset = cap->header->next;
		} while(offset != 0);
	}
	/* TODO: pcie extended caps? */
	return false;
}

static struct pcie_function *pcie_register_device(struct pcie_bus_header *space,
  struct pcie_config_space *config,
  unsigned int bus,
  unsigned int device,
  unsigned int function)
{
	printf("[pcie] found %.2x:%.2x.%.2x\n", bus, device, function);

	struct pcie_function *pf = malloc(sizeof(*pf));
	pf->config = config;
	pf->segment = space->segnr;
	pf->bus = bus;
	pf->device = device;
	pf->function = function;

	printf("  vendor=%x, device=%x, subclass=%x, class=%x, progif=%x, type=%d\n",
	  config->header.vendor_id,
	  config->header.device_id,
	  config->header.subclass,
	  config->header.class_code,
	  config->header.progif,
	  HEADER_TYPE(config->header.header_type));

	if(HEADER_TYPE(config->header.header_type)) {
		printf("[pcie] WARNING -- unimplemented: header_type 1\n");
		return pf;
	}

	printf("  cap_ptr: %x, bar0: %x bar1: %x bar2: %x bar3: %x bar4: %x bar5: %x\n",
	  config->device.cap_ptr,
	  config->device.bar[0],
	  config->device.bar[1],
	  config->device.bar[2],
	  config->device.bar[3],
	  config->device.bar[4],
	  config->device.bar[5]);

	return pf;
}

static void pcie_init_space(struct pcie_bus_header *space)
{
	if(space->magic != PCIE_BUS_HEADER_MAGIC) {
		printf("[pcie]: invalid PCIe bus header\n");
		return;
	}
	printf("[pcie] initializing PCIe configuration space covering %.4d:%.2x-%.2x\n",
	  space->segnr,
	  space->start_bus,
	  space->end_bus);

	/* brute-force scan. We _could_ detect areas to look in based on bridges and stuff, but this
	 * doesn't take much longer and is still fast. */
	for(unsigned bus = space->start_bus; bus < space->end_bus; bus++) {
		for(unsigned device = 0; device < 32; device++) {
			uintptr_t addr = ((bus - space->start_bus) << 20 | device << 15);
			struct pcie_config_space *config =
			  (void *)twz_ptr_lea(&pcie_cs_obj, (char *)space->spaces + addr);
			/* if a device isn't plugged in, the lines go high */
			if(config->header.vendor_id == 0xffff) {
				continue;
			}
			if(config->header.header_type & HEADER_MULTIFUNC) {
				/* for a multi-function device, brute-force scan all functions. We check for this
				 * before brute-force scanning all functions to reduce time by a factor of 8 */
				for(unsigned function = 0; function < 8; function++) {
					addr = ((bus - space->start_bus) << 20 | device << 15 | function << 12);
					config = (void *)twz_ptr_lea(&pcie_cs_obj, (char *)space->spaces + addr);
					if(config->header.vendor_id != 0xffff) {
						struct pcie_function *f =
						  pcie_register_device(space, config, bus, device, function);
					}
				}
			} else {
				/* not multi-function (function 0 is the only valid function) */
				struct pcie_function *f = pcie_register_device(space, config, bus, device, 0);
			}
		}
	}
}

int main(int argc, char **argv)
{
	printf("PCIE init:: %s\n", argv[1]);

	if(!objid_parse(argv[1], strlen(argv[1]), &pcie_cs_oid)) {
		printf("invalid object ID: %s\n", argv[1]);
		exit(1);
	}

	twz_object_open(&pcie_cs_obj, pcie_cs_oid, FE_READ | FE_WRITE);

	pcie_init_space(twz_obj_base(&pcie_cs_obj));

	int r;
	if((r = twz_thread_ready(NULL, THRD_SYNC_READY, 0))) {
		printf("failed to mark ready");
		exit(1);
	}

	for(;;)
		;
}
