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

static void __alloc_bar(struct object *obj,
  size_t start,
  size_t sz,
  int pref,
  int wc,
  uintptr_t addr)
{
	size_t idx = start / mm_page_size(0);
	while(sz > 0) {
		struct page *pg = page_alloc_nophys();
		pg->addr = addr;
		pg->type = PAGE_TYPE_MMIO;
		pg->flags |= (pref ? (wc ? PAGE_CACHE_WC : PAGE_CACHE_WT) : PAGE_CACHE_UC);
		size_t nix;
		if(sz >= mm_page_size(1) && !(addr & (mm_page_size(1) - 1))) {
			pg->level = 1;
			sz -= mm_page_size(1);
			addr += mm_page_size(1);
			nix = mm_page_size(1) / mm_page_size(0);
		} else {
			sz -= mm_page_size(0);
			addr += mm_page_size(0);
			nix = 1;
		}
		obj_cache_page(obj, idx, pg);
		idx += nix;
	}
}

#define MAX_INT_PER_DEV 8
struct kso_pcie_data {
	uint16_t segment;
	uint8_t bus;
	uint8_t device;
	uint8_t function;
	struct interrupt_alloc_req ir[MAX_INT_PER_DEV];
};

void __pcie_fn_interrupt(int v, struct interrupt_handler *ih)
{
	printk("[pcie] interrupt!\n");
}

#include <kalloc.h>
static long __pcie_fn_kaction(struct object *obj, long cmd, long arg)
{
	struct pcie_function_header hdr;
	struct kso_pcie_data *data;
	switch(cmd) {
		case KACTION_CMD_PF_INTERRUPTS_SETUP:
			obj_read_data(obj, 0, sizeof(hdr), &hdr);
			data = obj->data;
			printk("[pcie]: setup interrupts (%x:%x:%x.%x)\n",
			  data->segment,
			  data->bus,
			  data->device,
			  data->function);
			if(hdr.nr_interrupts > MAX_INT_PER_DEV)
				hdr.nr_interrupts = MAX_INT_PER_DEV;
			for(unsigned i = 0; i < hdr.nr_interrupts; i++) {
				struct pcie_function_interrupt irq;
				obj_read_data(obj, sizeof(hdr) + i * sizeof(irq), sizeof(irq), &irq);
				if(irq.flags & PCIE_FUNCTION_INT_ENABLE) {
					data->ir[i].flags |= INTERRUPT_ALLOC_REQ_VALID;
					data->ir[i].handler.fn = __pcie_fn_interrupt;
				}
			}
			if(interrupt_allocate_vectors(hdr.nr_interrupts, data->ir)) {
				return -EIO;
			}
			for(unsigned i = 0; i < hdr.nr_interrupts; i++) {
				struct pcie_function_interrupt irq;
				obj_read_data(obj, sizeof(hdr) + i * sizeof(irq), sizeof(irq), &irq);
				if(irq.flags & PCIE_FUNCTION_INT_ENABLE) {
					if(data->ir[i].flags & INTERRUPT_ALLOC_REQ_ENABLED) {
						irq.vec = data->ir[i].vec;
						obj_write_data(obj, sizeof(hdr) + i * sizeof(irq), sizeof(irq), &irq);
					}
				}
			}
			obj_write_data(obj, 0, sizeof(hdr), &hdr);

			break;
		default:
			return -EINVAL;
	}
	return 0;
}

static long pcie_function_init(struct object *pbobj,
  uint16_t segment,
  int bus,
  int device,
  int function,
  int wc)
{
	uintptr_t ba = 0;
	for(size_t i = 0; i < mcfg_entries; i++) {
		if(mcfg->spaces[i].pci_seg_group_nr == segment) {
			ba = mcfg->spaces[i].ba
			     + ((bus - mcfg->spaces[i].start_bus_nr) << 20 | device << 15 | function << 12);
			break;
		}
	}
	if(!ba) {
		return -ENOENT;
	}
	int r;
	objid_t psid;
	/* TODO: restrict write access. In fact, do this for ALL KSOs. */
	r = syscall_ocreate(0, 0, 0, 0, MIP_DFL_READ | MIP_DFL_WRITE, &psid);
	if(r < 0)
		panic("failed to create PCIe object: %d", r);

	struct object *fobj = obj_lookup(psid);
	assert(fobj != NULL);

	struct pcie_function_header hdr = {
		.bus = bus,
		.device = device,
		.function = function,
		.segment = segment,
	};

	struct pcie_config_space *space = mm_ptov(ba);

	size_t start = mm_page_size(1);
	for(int i = 0; i < 6; i++) {
		uint32_t bar = space->device.bar[i];
		if(!bar)
			continue;
		/* learn size */
		space->device.bar[i] = 0xffffffff;
		/* TODO: the proper way to do this is correctly map all this memory as UC */
		asm volatile("clflush %0" ::"m"(space->device.bar[i]) : "memory");
		uint32_t encsz = space->device.bar[i] & 0xfffffff0;
		space->device.bar[i] = bar;
		asm volatile("clflush %0" ::"m"(space->device.bar[i]) : "memory");
		size_t sz = ~encsz + 1;
		if(sz > 0x10000000) {
			panic("NI - large bar");
		}
		if(bar & 1) {
			/* TODO: I/O spaces? */
			continue;
		}
		int type = (bar >> 1) & 3;
		int pref = (bar >> 3) & 1;
		uint64_t addr = bar & 0xfffffff0;
		if(type == 2) {
			addr |= ((uint64_t)space->device.bar[i + 1]) << 32;
		}

		__alloc_bar(fobj, start, sz, pref, (wc >> i) & 1, addr);
		hdr.bars[i] = (volatile void *)start;
		hdr.prefetch[i] = pref;
		hdr.barsz[i] = sz;
#if 0
		printk("init bar %d for addr %lx at %lx len=%ld, type=%d (p=%d,wc=%d)\n",
		  i,
		  addr,
		  start,
		  sz,
		  type,
		  pref,
		  (wc >> i) & 1);
#endif

		start += sz;
		start = ((start - 1) & ~(mm_page_size(1) - 1)) + mm_page_size(1);

		if(type == 2)
			i++; /* skip the next bar because we used it in this one to make a 64-bit register */
	}
	start += 0x1000;
	__alloc_bar(fobj, start, 0x1000, 0, 0, ba);
	hdr.space = (void *)start;
	obj_write_data(fobj, 0, sizeof(hdr), &hdr);
	struct kso_pcie_data *data = fobj->data = kalloc(sizeof(struct kso_pcie_data));
	data->bus = bus;
	data->device = device;
	data->function = function;
	data->segment = segment;

	fobj->kaction = __pcie_fn_kaction;

	unsigned int fnid = function | device << 3 | bus << 8;
	obj_write_data(pbobj, offsetof(struct pcie_bus_header, functions[fnid]), sizeof(psid), &psid);

	return 0;
}

static long __pcie_kaction(struct object *obj, long cmd, long arg)
{
	switch(cmd) {
		int bus, device, function;
		uint16_t segment;
		uint16_t wc;
		case KACTION_CMD_PCIE_INIT_DEVICE:
			/* arg specifies which bus, device, and function */
			wc = arg >> 32;
			segment = (arg >> 16) & 0xffff;
			bus = (arg >> 8) & 0xff;
			device = (arg >> 3) & 0x1f;
			function = arg & 7;
			return pcie_function_init(obj, segment, bus, device, function, wc);
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

__attribute__((no_sanitize("undefined"))) static void pcie_init_space(struct mcfg_desc_entry *space)
{
	printk("[acpi] found MCFG descriptor table: %p\n", space);
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
	obj->kaction = __pcie_kaction;

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

	printk("[pcie] found MCFG table (%p) with %ld entries\n", mcfg, mcfg_entries);

	post_init_call_register(false, __pcie_init, NULL);
}
