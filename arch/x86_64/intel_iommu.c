#include <arch/x86_64-acpi.h>
#include <debug.h>
#include <init.h>
#include <memory.h>
#include <system.h>

struct __packed device_scope {
	uint8_t type;
	uint8_t length;
	uint16_t resv;
	uint8_t enumer_id;
	uint8_t start_bus_nr;
	uint16_t path[];
};

struct __packed dmar_remap {
	uint16_t type;
	uint16_t length;
	uint8_t flags;
	uint8_t resv;
	uint16_t segnr;
	uint64_t reg_base_addr;
	struct device_scope scopes[];
};

struct __packed dmar_desc {
	struct sdt_header header;
	uint8_t host_addr_width;
	uint8_t flags;
	uint8_t resv[10];
	struct dmar_remap remaps[];
};

static struct dmar_desc *dmar;
size_t remap_entries = 0;

struct iommu {
	uint64_t base;
	uint16_t pcie_seg;
	uint16_t id;
	uintptr_t root_table;
	uint64_t cap;
	bool init;
};

#define MAX_IOMMUS 16
static struct iommu iommus[MAX_IOMMUS] = {};

#define IOMMU_REG_VERS 0
#define IOMMU_REG_CAP 8
#define IOMMU_REG_EXCAP 0x10
#define IOMMU_REG_GCMD 0x18
#define IOMMU_REG_GSTS 0x1c
#define IOMMU_REG_RTAR 0x20
#define IOMMU_REG_CCMD 0x28
#define IOMMU_REG_FSR 0x34
#define IOMMU_REG_FEC 0x38
#define IOMMU_REG_FED 0x3c
#define IOMMU_REG_FEA 0x40
#define IOMMU_REG_FEUA 0x44
#define IOMMU_REG_AFL 0x58
#define IOMMU_REG_IQH 0x80
#define IOMMU_REG_IQT 0x88
#define IOMMU_REG_IQA 0x90
#define IOMMU_REG_ICS 0x9c
#define IOMMU_REG_ICEC 0xa0
#define IOMMU_REG_ICED 0xa4
#define IOMMU_REG_ICEA 0xa8
#define IOMMU_REG_ICEUA 0xac
#define IOMMU_REG_IQER 0xb0
#define IOMMU_REG_IRTADDR 0xb8
#define IOMMU_REG_PRQH 0xc0
#define IOMMU_REG_PRQT 0xc8
#define IOMMU_REG_PRQA 0xd0
#define IOMMU_REG_PRSR 0xdc
#define IOMMU_REG_PREC 0xe0
#define IOMMU_REG_PRED 0xe4
#define IOMMU_REG_PREA 0xe8
#define IOMMU_REG_PREUA 0xec

#define IOMMU_CAP_NFR(x) ((((x) >> 40) & 0xff) + 1)
#define IOMMU_CAP_SLLPS(x) (((x) >> 34) & 0xf)
#define IOMMU_CAP_FRO(x) ((((x) >> 24) & 0x3ff) * 16)
#define IOMMU_CAP_CM (1 << 7)
#define IOMMU_CAP_ND(x) (((x)&7) * 2 + 4)

#define IOMMU_EXCAP_DT (1 << 2)

#define IOMMU_GCMD_TE (1ul << 31)
#define IOMMU_GCMD_SRTP (1ul << 30)

#define IOMMU_RTAR_TTM_LEGACY 0
#define IOMMU_RTAR_TTM_SCALABLE 1

#define IOMMU_CCMD_ICC (1ul << 63)
#define IOMMU_CCMD_SRC(x) ((x) << 16)
#define IOMMU_CCMD_DID(x) ((x))

#define IOMMU_FSR_FRI (((x) >> 8) & 0xff)
#define IOMMU_FSR_PPF (1 << 1)
#define IOMMU_FSR_PFO (1 << 0)
#define IOMMU_FEC_IM (1 << 31)
#define IOMMU_FEC_IP (1 << 30)

#define IOMMU_FRRH_FAULT (1ul << 63)
#define IOMMU_FRRH_TYPE1 (1ul << 62)
#define IOMMU_FRRH_FR(x) (((x) >> 32 & 0xFF))
#define IOMMU_FRRH_EXE (1ul << 30)
#define IOMMU_FRRH_SID(x) ((x)&0xFFFF)
#define IOMMU_FRRH_TYPE2 (1ul << 28)

#define IOMMU_CTXE_PRESENT 1
#define IOMMU_CTXE_AW48 2
#define IOMMU_CTXE_AW39 1

#define IOMMU_RTE_PRESENT 1
#define IOMMU_RTE_MASK 0xFFFFFFFFFFFFF000

struct iommu_rte {
	uint64_t lo;
	uint64_t hi;
};

struct iommu_ctxe {
	uint64_t lo;
	uint64_t hi;
};

static uint32_t iommu_read32(struct iommu *im, int reg)
{
	return *(volatile uint32_t *)(im->base + reg);
}

static void iommu_write32(struct iommu *im, int reg, uint32_t val)
{
	*(volatile uint32_t *)(im->base + reg) = val;
	asm volatile("mfence" ::: "memory");
}

static uint64_t iommu_read64(struct iommu *im, int reg)
{
	return *(volatile uint64_t *)(im->base + reg);
}

static void iommu_write64(struct iommu *im, int reg, uint64_t val)
{
	*(volatile uint64_t *)(im->base + reg) = val;
	asm volatile("mfence" ::: "memory");
}

static void iommu_status_wait(struct iommu *im, uint32_t ws, bool set)
{
	if(set) {
		while(!(iommu_read32(im, IOMMU_REG_GSTS) & ws))
			asm("pause");
	} else {
		while((iommu_read32(im, IOMMU_REG_GSTS) & ws))
			asm("pause");
	}
}

static void iommu_set_context_entry(struct iommu *im,
  uint8_t bus,
  uint8_t dfn,
  uintptr_t ptroot,
  uint16_t did)
{
	struct iommu_rte *rt = mm_ptov(im->root_table);
	if(!(rt[bus].lo & IOMMU_RTE_PRESENT)) {
		rt[bus].lo = mm_physical_alloc(0x1000, PM_TYPE_DRAM, true) | IOMMU_RTE_PRESENT;
		asm volatile("clflush %0" ::"m"(rt[bus]));
	}
	struct iommu_ctxe *ct = mm_ptov(rt[bus].lo & IOMMU_RTE_MASK);
	if(!(ct[dfn].lo & IOMMU_CTXE_PRESENT)) {
		ct[dfn].lo = ptroot | IOMMU_CTXE_PRESENT;
		ct[dfn].hi = IOMMU_CTXE_AW48 | (did << 8);
		asm volatile("clflush %0" ::"m"(ct[dfn]));
	}
}

#include <arch/x86_64-vmx.h>
#include <object.h>
#include <processor.h>

static inline void test_and_allocate(uintptr_t *loc, uint64_t attr)
{
	if(!*loc) {
		*loc = (uintptr_t)mm_physical_alloc(0x1000, PM_TYPE_DRAM, true) | (attr & RECUR_ATTR_MASK);
		asm volatile("clflush %0" ::"m"(*loc));
	}
}

static uintptr_t ept_phys;

static void do_iommu_object_map_slot(struct object *obj, uint64_t flags)
{
	uintptr_t virt = obj->slot * (1024 * 1024 * 1024ull);
	int pml4_idx = PML4_IDX(virt);
	int pdpt_idx = PDPT_IDX(virt);

	uintptr_t *pml4 = GET_VIRT_TABLE(ept_phys);
	test_and_allocate(&pml4[pml4_idx], EPT_READ | EPT_WRITE | EPT_EXEC);

	uintptr_t *pdpt = GET_VIRT_TABLE(pml4[pml4_idx]);
	pdpt[pdpt_idx] = obj->arch.pt_root | 7;
	asm volatile("clflush %0" ::"m"(pdpt[pdpt_idx]));
}

#include <device.h>
void iommu_object_map_slot(struct device *dev, struct object *obj)
{
	if(obj)
		do_iommu_object_map_slot(obj, 0);
	if(dev) {
		uint16_t seg = (dev->uid >> 16) & 0xffff;
		uint16_t sid = dev->uid & 0xffff;
		for(size_t i = 0; i < MAX_IOMMUS; i++) {
			if(iommus[i].base && iommus[i].pcie_seg == seg) {
				iommu_set_context_entry(&iommus[i], sid >> 8, sid & 0xff, ept_phys, 1);
			}
		}
	}
}

/* TODO: generalize */
void pcie_iommu_fault(uint16_t seg, uint16_t sid, uint64_t addr, bool handled);
void __iommu_fault_handler(int v __unused, struct interrupt_handler *h __unused)
{
	for(int i = 0; i < MAX_IOMMUS; i++) {
		struct iommu *im = &iommus[i];
		if(!im->init)
			continue;
		uint32_t fsr = iommu_read32(im, IOMMU_REG_FSR);
		iommu_write32(im, IOMMU_REG_FSR, fsr);
		if(fsr & IOMMU_FSR_PPF) {
			/* TODO: should use the FRI field */
			for(unsigned fr = 0; fr < IOMMU_CAP_NFR(im->cap); fr++) {
				uint64_t flo = iommu_read64(im, IOMMU_CAP_FRO(im->cap) + (fr * 16));
				uint64_t fhi = iommu_read64(im, IOMMU_CAP_FRO(im->cap) + (fr * 16) + 8);
				if(!(fhi & IOMMU_FRRH_FAULT))
					continue;
				iommu_write64(im, IOMMU_CAP_FRO(im->cap) + (fr * 16) + 8, IOMMU_FRRH_FAULT);
				uint16_t sid = IOMMU_FRRH_SID(fhi);
				printk(":: detected fault from device %x:%x:%x.%x access to %lx\n",
				  im->pcie_seg,
				  sid >> 8,
				  (sid >> 3) & 0xf,
				  (sid & 7),
				  flo);
				size_t slot = flo / mm_page_size(MAX_PGLEVEL);
				size_t idx = (flo % mm_page_size(MAX_PGLEVEL)) / mm_page_size(0);
				struct object *o = obj_lookup_slot(flo);
				if(o) {
					do_iommu_object_map_slot(o, 0);
					iommu_set_context_entry(im, sid >> 8, sid & 0xff, ept_phys, 1);
					struct objpage *p = obj_get_page(o, idx, true);

					printk("[iommu] mapping page %ld\n", p->idx);
					if(!(p->flags & OBJPAGE_MAPPED)) {
						arch_object_map_page(o, p->page, p->idx);
						p->flags |= OBJPAGE_MAPPED;
					}
					arch_object_map_flush(o, p->idx);

					obj_put(o);
				} else {
					printk("[iommu] fault to slot %ld; unknown object\n", slot);
				}
				pcie_iommu_fault(im->pcie_seg, sid, flo, !!o);
			}
		}
		if(fsr & IOMMU_FSR_PFO) {
			printk("[iommu] warning - primary fault overflow\n");
		}
	}
}

void __iommu_inv_handler(int v __unused, struct interrupt_handler *h __unused)
{
	printk("!!! IOMMU INV\n");
}

static struct interrupt_alloc_req _iommu_int_iaq[2] = {
	[0] = {
		.pri = IVP_NORMAL,
		.handler.fn = __iommu_fault_handler,
		.flags = INTERRUPT_ALLOC_REQ_VALID,
	},
	[1] = {
		.pri = IVP_NORMAL,
		.handler.fn = __iommu_inv_handler,
		.flags = INTERRUPT_ALLOC_REQ_VALID,
	}
};

static int iommu_init(struct iommu *im)
{
	uint32_t vs = iommu_read32(im, IOMMU_REG_VERS);
	uint64_t cap = iommu_read64(im, IOMMU_REG_CAP);
	uint64_t ecap = iommu_read64(im, IOMMU_REG_EXCAP);
	im->cap = cap;

	ept_phys = mm_physical_alloc(0x1000, PM_TYPE_DRAM, true);

	printk(":: %lx %lx %x\n", cap, ecap, vs);
	/*	printk("nfr=%lx, sllps=%lx, fro=%lx, nd=%ld\n",
	      IOMMU_CAP_NFR(cap),
	      IOMMU_CAP_SLLPS(cap),
	      IOMMU_CAP_FRO(cap),
	      IOMMU_CAP_ND(cap));
	      */
	if(IOMMU_CAP_ND(cap) < 16) {
		printk("[iommu] iommu %d does not support large enough domain ID\n", im->id);
		// return -1;
	}
	if(IOMMU_CAP_SLLPS(cap) != 3) {
		printk("[iommui] iommu %d does not support huge pages at sl translation\n", im->id);
		return -1;
	}

	/* first disable the hardware during init */
	iommu_write32(im, IOMMU_REG_GCMD, 0);
	/* if it was enabled, wait for it to disable */
	iommu_status_wait(im, IOMMU_GCMD_TE, false);

	/* set the root table */
	im->root_table = mm_physical_alloc(0x1000, PM_TYPE_DRAM, true);
	iommu_write64(im, IOMMU_REG_RTAR, im->root_table | IOMMU_RTAR_TTM_LEGACY);

	iommu_write32(im, IOMMU_REG_GCMD, IOMMU_GCMD_SRTP);
	iommu_status_wait(im, IOMMU_GCMD_SRTP, true);

	/* allocate interrupt vectors for the iommu itself. It need to inform us of faults and of
	 * invalidation completions. It uses message-signaled interrupts. */
	iommu_write32(im, IOMMU_REG_FED, _iommu_int_iaq[0].vec);
	iommu_write32(im, IOMMU_REG_FEA, x86_64_msi_addr(0, X86_64_MSI_DM_PHYSICAL));
	iommu_write32(im, IOMMU_REG_FEUA, 0);
	/* unmask the FECTL interrupt */
	iommu_write32(im, IOMMU_REG_FEC, 0);

	iommu_write32(im, IOMMU_REG_ICED, _iommu_int_iaq[1].vec);
	iommu_write32(im, IOMMU_REG_ICEA, x86_64_msi_addr(0, X86_64_MSI_DM_PHYSICAL));
	iommu_write32(im, IOMMU_REG_ICEUA, 0);

	im->init = true;
	uint32_t cmd = IOMMU_GCMD_TE;
	iommu_write32(im, IOMMU_REG_GCMD, cmd);
	iommu_status_wait(im, IOMMU_GCMD_TE, true);

	return 0;
}

static void dmar_late_init(void *__u __unused)
{
	interrupt_allocate_vectors(2, _iommu_int_iaq);
	printk("[iommu] allocated vectors (%d, %d) for iommu\n",
	  _iommu_int_iaq[0].vec,
	  _iommu_int_iaq[1].vec);
	for(size_t i = 0; i < MAX_IOMMUS; i++) {
		if(iommus[i].base) {
			iommu_init(&iommus[i]);
		}
	}
}
POST_INIT(dmar_late_init);

__orderedinitializer(__orderedafter(ACPI_INITIALIZER_ORDER)) static void dmar_init(void)
{
	if(!(dmar = acpi_find_table("DMAR"))) {
		return;
	}

	remap_entries = (dmar->header.length - sizeof(struct dmar_desc)) / sizeof(struct dmar_remap);
	printk("[iommu] found DMAR header with %ld remap entries (haw=%d; flags=%x)\n",
	  remap_entries,
	  dmar->host_addr_width,
	  dmar->flags);
	size_t ie = 0;
	struct dmar_remap *remap = &dmar->remaps[0];
	for(size_t i = 0; i < remap_entries; i++) {
		iommus[ie].id = ie;
		size_t scope_entries =
		  (remap->length - sizeof(struct dmar_remap)) / sizeof(struct device_scope);
		if(remap->type != 0)
			continue;
		printk(
		  "[iommu] %d remap: type=%d, length=%d, flags=%x, segnr=%d, base_addr=%lx, %ld device "
		  "scopes\n",
		  iommus[ie].id,
		  remap->type,
		  remap->length,
		  remap->flags,
		  remap->segnr,
		  remap->reg_base_addr,
		  scope_entries);
#if 0
		for(size_t j = 0; j < scope_entries; j++) {
			size_t path_len = (remap->scopes[j].length - sizeof(struct device_scope)) / 2;
			printk("[iommu]    dev_scope: type=%d, length=%d, enumer_id=%d, start_bus=%d, "
			       "path_len=%ld\n",
			  dmar->remaps[i].scopes[j].type,
			  dmar->remaps[i].scopes[j].length,
			  dmar->remaps[i].scopes[j].enumer_id,
			  dmar->remaps[i].scopes[j].start_bus_nr,
			  path_len);
			for(size_t k = 0; k < path_len; k++) {
				//		printk("[iommu]      path %ld: %x\n", k, dmar->remaps[i].scopes[j].path[k]);
			}
		}
#endif

		if(1 || remap->flags & 1 /* PCIe include all */) {
			iommus[ie].base = (uint64_t)mm_ptov(remap->reg_base_addr);
			iommus[ie].pcie_seg = remap->segnr;
			ie++;
		} else {
			printk("[iommu] warning - remap hardware without PCI-include-all bit unsupported\n");
		}
		remap = (void *)((char *)remap + remap->length);
	}
}
