#include <arch/x86_64-acpi.h>
#include <debug.h>
#include <init.h>
#include <machine/pc-pcie.h>
#include <memory.h>
#include <page.h>
#include <slab.h>
#include <syscall.h>
#include <system.h>

/* The PCI subsystem. We make some simplifying assumptions:
 *   - Our system bus is PCIe. Thus everything is memory-mapped. We do not support IO-based access.
 *   - Devices that we want to work on support MSI or MSI-X. Thus we don't need to worry about
 *     old-style pin-based interrupts and all that complexity.
 *
 * Each PCIe function has a configuration space (a region of memory that controls the PCI-side of
 * the device) and a set of memory-mapped registers that are device-specific (which control the
 * device itself). The configuration space for each segment of the PCIe system is mapped into a
 * single object that can be controlled from userspace. However, there are things we need to do
 * here:
 *   - Some initialization per-device
 *
 * The goal is to have as little PCI configuration in here as possible.
 */

/* ACPI MCFG table contains an array of entries that describe information for a particular segment
 * of PCIe devices */
__packed struct mcfg_desc_entry {
	uint64_t ba;
	uint16_t pci_seg_group_nr;
	uint8_t start_bus_nr;
	uint8_t end_bus_nr;
	uint32_t _resv;
};

/* ACPI table for configuring PCIe memory-mapped config space */
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

	int r;
	objid_t psid;
	/* TODO: restrict write access. In fact, do this for ALL KSOs. */
	r = syscall_ocreate(0, 0, 0, 0, MIP_DFL_READ | MIP_DFL_WRITE, &psid);
	if(r < 0)
		panic("failed to create PCIe space object: %d", r);

	uintptr_t start_addr = space->ba;
	uintptr_t end_addr =
	  space->ba + ((space->end_bus_nr - space->start_bus_nr) << 20 | 32 << 15 | 8 << 12);

	struct object *obj = obj_lookup(psid);
	assert(obj != NULL);
	size_t idx = mm_page_size(1) / mm_page_size(0);

	for(uintptr_t p = start_addr; p < end_addr; p += mm_page_size(1)) {
		struct page *pg = page_alloc_nophys();
		pg->addr = p;
		pg->type = PAGE_TYPE_MMIO;
		pg->flags |= PAGE_CACHE_UC;
		pg->level = 1;
		obj_cache_page(obj, idx, pg);
		idx += mm_page_size(1) / mm_page_size(0);
	}

	struct pcie_bus_header hdr = {
		.magic = PCIE_BUS_HEADER_MAGIC,
		.start_bus = space->start_bus_nr,
		.end_bus = space->end_bus_nr,
		.segnr = space->pci_seg_group_nr,
		.spaces = (void *)(mm_page_size(1)),
	};
	snprintf(hdr.hdr.name,
	  KSO_NAME_MAXLEN,
	  "PCIe bus %.2x::%.2x-%.2x",
	  hdr.segnr,
	  hdr.start_bus,
	  hdr.end_bus);
	obj_write_data(obj, 0, sizeof(struct pcie_bus_header), &hdr);
	kso_root_attach(obj, 0, KSO_DEVBUS);

	printk("[pcie] attached PCIe bus KSO: " IDFMT "\n", IDPR(psid));
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
