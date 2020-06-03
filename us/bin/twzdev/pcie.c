#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <twz/debug.h>
#include <twz/driver/bus.h>
#include <twz/driver/device.h>
#include <twz/objctl.h>
#include <twz/sys.h>

#include "pcie.h"

twzobj pcie_cs_obj;
objid_t pcie_cs_oid;
static twzobj pids;

static struct pcie_function *pcie_list = NULL;
int create_pty_pair(char *server, char *client);

static void pcie_print_function(struct pcie_function *pf, bool nums)
{
	static bool _init = false;
	if(!_init && !nums) {
		if(twz_object_init_name(&pids, "/usr/share/pcieids", FE_READ)) {
			fprintf(stderr, "failed to open pcieids\n");
			return;
		}
		_init = true;
	}

	char *vname = NULL, *dname = NULL;
	uint16_t vendor = pf->config->header.vendor_id;
	uint16_t device = pf->config->header.device_id;
	uint16_t class = pf->config->header.class_code;
	uint16_t subclass = pf->config->header.subclass;
	uint16_t progif = pf->config->header.progif;
	const char *str = twz_object_base(&pids);
	const char *line;
	for(line = str; line && *line; line = strchr(line, '\n')) {
		while(*line == '\n')
			line++;
		if(*line == '#')
			continue;

		if(*line != '\t') {
			char *endptr = NULL;
			if(strtol((char *)line, &endptr, 16) == vendor) {
				char *next = strchr(line, '\n');
				while(*endptr == ' ')
					endptr++;
				vname = strndup(endptr, next - endptr);

				line = next + 1;
				for(; line && *line; line = strchr(line, '\n')) {
					while(*line == '\n')
						line++;
					if(*line == '#')
						continue;
					if(*line != '\t')
						break;
					if(strtol((char *)line + 1, &endptr, 16) == device) {
						char *next = strchr(line, '\n');
						while(*endptr == ' ')
							endptr++;
						dname = strndup(endptr, next - endptr);
					}
				}
				break;
			}
		}
	}

	char *cname = NULL, *sname = NULL, *pname = NULL;

	char *cstart = strstr(str, "\nC ");

	for(line = cstart ? cstart + 1 : NULL; line && *line; line = strchr(line, '\n')) {
		while(*line == '\n')
			line++;
		if(*line == '#')
			continue;

		if(*line == 'C') {
			char *endptr = NULL;
			if(strtol((char *)line + 2, &endptr, 16) == class) {
				char *next = strchr(line, '\n');
				while(*endptr == ' ')
					endptr++;
				cname = strndup(endptr, next - endptr);

				line = next + 1;
				for(; line && *line; line = strchr(line, '\n')) {
					while(*line == '\n')
						line++;
					if(*line == '#')
						continue;
					if(*line != '\t')
						break;
					if(*(line + 1) == '\t')
						continue;
					if(strtol((char *)line + 1, &endptr, 16) == subclass) {
						char *next = strchr(line, '\n');
						while(*endptr == ' ')
							endptr++;
						sname = strndup(endptr, next - endptr);

						line = next + 1;

						for(; line && *line; line = strchr(line, '\n')) {
							while(*line == '\n')
								line++;
							if(*line == '#')
								continue;
							if(*line != '\t')
								break;
							if(*(line + 1) != '\t')
								break;
							if(strtol((char *)line + 2, &endptr, 16) == progif) {
								char *next = strchr(line, '\n');
								while(*endptr == ' ')
									endptr++;
								pname = strndup(endptr, next - endptr);

								line = next + 1;
							}
						}
					}
				}
				break;
			}
		}
	}

#if 0
	// printf("%x %x %x : %x %x\n", class, subclass, progif, vendor, device);
	printf("[pcie] [%d:%d:%d] (%x:%x:%x) %s ",
	  pf->bus,
	  pf->device,
	  pf->function,
	  class,
	  subclass,
	  progif,
	  sname ? sname : cname);
	if(pname)
		printf("[%s] ", pname);
#else
	(void)cname;
	(void)pname;
	(void)sname;
	(void)dname;
	(void)vname;
#endif
}

static int pcie_init_function(struct pcie_function *pf)
{
	uint64_t wc = 0;
	if(pf->config->header.vendor_id == 0x1234 && pf->config->header.device_id == 0x1111) {
		/* TODO: generalize */
		wc = 1;
	}
	int r;
	if((r = twz_object_kaction(&pcie_cs_obj,
	      KACTION_CMD_PCIE_INIT_DEVICE,
	      pf->segment << 16 | pf->bus << 8 | pf->device << 3 | pf->function | (wc << 32)))
	   < 0) {
		fprintf(stderr, "kaction: %d\n", r);
		return r;
	}
	uint32_t fid = pf->bus << 8 | pf->device << 3 | pf->function;
	if(twz_bus_open_child(&pcie_cs_obj, &pf->cobj, fid, FE_READ | FE_WRITE)) {
		return -1;
	}
	kso_set_name(
	  &pf->cobj, "pcie device %x:%x:%x.%x", pf->segment, pf->bus, pf->device, pf->function);
	pf->cid = twz_object_guid(&pf->cobj);
	return 0;
}

static struct pcie_function *pcie_register_device(struct pcie_bus_header *space,
  volatile struct pcie_config_space *config,
  unsigned int bus,
  unsigned int device,
  unsigned int function)
{
	struct pcie_function *pf = malloc(sizeof(*pf));
	pf->config = config;
	pf->segment = space->segnr;
	pf->bus = bus;
	pf->device = device;
	pf->function = function;
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

	/* XXX - HACK to get real hw working */
	// return;
	if(space->end_bus < 0xff)
		return;
	/* brute-force scan. We _could_ detect areas to look in based on bridges and stuff, but this
	 * doesn't take much longer and is still fast. */
	for(unsigned bus = space->start_bus; bus < space->end_bus; bus++) {
		for(unsigned device = 0; device < 32; device++) {
			uintptr_t addr = ((bus - space->start_bus) << 20 | device << 15);
			volatile struct pcie_config_space *config =
			  (void *)twz_object_lea(&pcie_cs_obj, (char *)space->spaces + addr);
			/* if a device isn't plugged in, the lines go high */
			// printf(":: %d %d %lx -> %x\n", bus, device, addr, config->header.vendor_id);
			if(config->header.vendor_id == 0xffff) {
				continue;
			}
			if(config->header.header_type & HEADER_MULTIFUNC) {
				/* for a multi-function device, brute-force scan all functions. We check for
				 * this
				 * before brute-force scanning all functions to reduce time by a factor of 8 */
				for(unsigned function = 0; function < 8; function++) {
					addr = ((bus - space->start_bus) << 20 | device << 15 | function << 12);
					config = (void *)twz_object_lea(&pcie_cs_obj, (char *)space->spaces + addr);
					if(config->header.vendor_id != 0xffff) {
						struct pcie_function *f =
						  pcie_register_device(space, config, bus, device, function);
						f->next = pcie_list;
						pcie_list = f;
					}
				}
			} else {
				/* not multi-function (function 0 is the only valid function) */
				struct pcie_function *f = pcie_register_device(space, config, bus, device, 0);
				f->next = pcie_list;
				pcie_list = f;
			}
		}
	}
}

#include <twz/name.h>
static void pcie_load_driver(struct pcie_function *pf)
{
	struct pcie_function_header *hdr = twz_device_getds(&pf->cobj);
	if(hdr->vendorid == 0x1234 && hdr->deviceid == 0x1111) {
		//	create_pty_pair("/dev/pty/pty0", "/dev/pty/ptyc0");
		//	twz_name_assign(pf->cid, "/dev/framebuffer");
		return;
	}
	if(hdr->classid == 1 && hdr->subclassid == 8 && hdr->progif == 2) {
		twz_name_assign(pf->cid, "/dev/nvme");
		return;
	}
}

void pcie_load_bus(twzobj *obj)
{
	pcie_cs_obj = *obj;
	pcie_init_space(twz_bus_getbs(&pcie_cs_obj));

	for(struct pcie_function *pf = pcie_list; pf; pf = pf->next) {
		pcie_print_function(pf, false);
		if(pcie_init_function(pf) == 0)
			pcie_load_driver(pf);
	}
}
