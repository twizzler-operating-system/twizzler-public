#include <arch/x86_64-acpi.h>
#include <debug.h>
#include <init.h>
#include <machine/pc-pcie.h>
#include <memory.h>
#include <slab.h>
#include <system.h>
__packed struct mcfg_desc_entry {
	uint64_t ba;
	uint16_t pci_seg_group_nr;
	uint8_t start_bus_nr;
	uint8_t end_bus_nr;
	uint32_t _resv;
};

struct __packed mcfg_desc {
	struct sdt_header header;
	uint64_t _resv;
	struct mcfg_desc_entry spaces[];
};

static struct mcfg_desc *mcfg;
static size_t mcfg_entries;

struct slabcache sc_pcief;
static DECLARE_LIST(pcief_list);
struct spinlock pcielock = SPINLOCK_INIT;

static void _pf_ctor(void *_u, void *ptr)
{
	(void)_u;
	struct pcie_function *pf = ptr;
	pf->flags = 0;
	pf->lock = SPINLOCK_INIT;
}

static void _pf_dtor(void *_u, void *ptr)
{
	(void)_u;
	(void)ptr;
}

__initializer static void _init_objs(void)
{
	slabcache_init(&sc_pcief, sizeof(struct pcie_function), _pf_ctor, _pf_dtor, NULL);
}

#include <lib/iter.h>
static struct pcie_function *pcief_lookup(uint16_t space,
  uint8_t bus,
  uint8_t device,
  uint8_t function)
{
	foreach(_pf, list, &pcief_list) {
		struct pcie_function *pf = list_entry(_pf, struct pcie_function, entry);
		if(pf->segment == space && pf->bus == bus && pf->device == device
		   && pf->function == function) {
			return pf;
		}
	}
	return NULL;
}

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

static struct pcie_function *pcie_register_device(struct mcfg_desc_entry *space,
  struct pcie_config_space *config,
  unsigned int bus,
  unsigned int device,
  unsigned int function)
{
	printk("[pcie] found %.2x:%.2x.%.2x\n", bus, device, function);

	struct pcie_function *pf = slabcache_alloc(&sc_pcief);
	pf->config = config;
	pf->segment = space->pci_seg_group_nr;
	pf->bus = bus;
	pf->device = device;
	pf->function = function;

	printk("  vendor=%x, device=%x, subclass=%x, class=%x, progif=%x, type=%d\n",
	  config->header.vendor_id,
	  config->header.device_id,
	  config->header.subclass,
	  config->header.class_code,
	  config->header.progif,
	  HEADER_TYPE(config->header.header_type));

	if(HEADER_TYPE(config->header.header_type)) {
		printk("[pcie] WARNING -- unimplemented: header_type 1\n");
		return pf;
	}

	printk("  cap_ptr: %x, bar0: %x bar1: %x bar2: %x bar3: %x bar4: %x bar5: %x\n",
	  config->device.cap_ptr,
	  config->device.bar[0],
	  config->device.bar[1],
	  config->device.bar[2],
	  config->device.bar[3],
	  config->device.bar[4],
	  config->device.bar[5]);

	return pf;
}

static void pcie_init_space(struct mcfg_desc_entry *space)
{
	printk("[pcie] initializing PCIe configuration space at %lx covering %.4d:%.2x-%.2x\n",
	  space->ba,
	  space->pci_seg_group_nr,
	  space->start_bus_nr,
	  space->end_bus_nr);

	for(unsigned bus = space->start_bus_nr; bus < space->end_bus_nr; bus++) {
		for(unsigned device = 0; device < 32; device++) {
			struct pcie_config_space *config =
			  mm_ptov(space->ba + ((bus - space->start_bus_nr) << 20 | device << 15));
			if(config->header.vendor_id == 0xffff) {
				continue;
			}
			if(config->header.header_type & HEADER_MULTIFUNC) {
				for(unsigned function = 0; function < 8; function++) {
					config = mm_ptov(
					  space->ba
					  + ((bus - space->start_bus_nr) << 20 | device << 15 | function << 12));
					if(config->header.vendor_id != 0xffff) {
						struct pcie_function *f =
						  pcie_register_device(space, config, bus, device, function);
						list_insert(&pcief_list, &f->entry);
					}
				}
			} else {
				struct pcie_function *f = pcie_register_device(space, config, bus, device, 0);
				list_insert(&pcief_list, &f->entry);
			}
		}
	}

	printk("done\n");

	for(;;)
		;
}

static void __pcie_init(void *arg __unused)
{
	for(size_t i = 0; i < mcfg_entries; i++) {
		pcie_init_space(&mcfg->spaces[i]);
	}
}

__orderedinitializer(__orderedafter(ACPI_INITIALIZER_ORDER)) static void mcfg_init(void)
{
	if(!(mcfg = acpi_find_table("MCFG"))) {
		printk("[pcie] WARNING - no PCI Extended Configuration Space found!\n");
		return;
	}

	mcfg_entries =
	  (mcfg->header.length - sizeof(struct mcfg_desc)) / sizeof(struct mcfg_desc_entry);

	printk("[pcie] found MCFG table with %ld entries\n", mcfg_entries);

	post_init_call_register(false, __pcie_init, NULL);
}
