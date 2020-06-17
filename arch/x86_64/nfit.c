#include <arch/x86_64-acpi.h>
#include <debug.h>
#include <memory.h>
#include <pmap.h>
#include <system.h>

#define NFIT_TYPE_SPA 0
#define NFIT_TYPE_REGION 1
#define NFIT_TYPE_CONTROL 4

struct __packed nfit_entry {
	uint16_t type;
	uint16_t length;

	union {
		struct __packed {
			uint16_t idx;
			uint16_t flags;
			uint32_t resv;
			uint32_t prox_domain;
			struct __packed {
				uint32_t a;
				uint16_t b;
				uint16_t c;
				uint8_t d[8];
			} art_guid;
			uint64_t base;
			uint64_t length;
			uint64_t attrs;
		} spa;
		struct __packed {
			uint32_t dev_handle;
			uint16_t phys_id;
			uint16_t region_id;
			uint16_t spa_range_idx;
			uint16_t control_region_idx;
			uint64_t length;
			uint64_t offset;
			uint64_t phys_base;
			uint16_t interleave_idx;
			uint16_t interleave_ways;
			uint16_t state;
			uint16_t resv;
		} region;
		struct __packed {
			uint16_t idx;
			uint16_t vendor;
			uint16_t device;
			uint16_t revision;
			uint16_t subsys_vendor;
			uint16_t subsys_device;
			uint16_t subsys_revision;
			uint8_t valid;
			uint8_t manufacture;
			uint16_t date;
			uint16_t resv;
			uint32_t serial;
			uint16_t region_fmt;
			uint16_t nr_bcw;
			uint64_t sz_bcw;
			uint64_t cmd_reg_bcw;
			uint64_t sz_cmd_reg_bcw;
			uint64_t stat_reg_bcw;
			uint64_t sz_stat_reg_bcw;
			uint16_t ctrl_reg_flag;
			uint8_t resv1[6];
		} control;
	};
};

struct __packed nfit_desc {
	struct sdt_header header;
	uint32_t resv;
	struct nfit_entry nfits[];
};

static struct nfit_desc *nfit;

__orderedinitializer(__orderedafter(ACPI_INITIALIZER_ORDER)) static void nfit_init(void)
{
	if(!(nfit = acpi_find_table("NFIT"))) {
		return;
	}

	char *start = (char *)&nfit->nfits[0];
	char *ptr = start;
	while(ptr < start + nfit->header.length) {
		struct nfit_entry *nfit_entry = (struct nfit_entry *)ptr;
		printk("nfit: %d\n", nfit_entry->type);
		if(nfit_entry->type == NFIT_TYPE_SPA) {
			printk(":: nfit spa: base = %lx, length = %lx; flags = %x, attrs = %lx, index = %d\n",
			  nfit_entry->spa.base,
			  nfit_entry->spa.length,
			  nfit_entry->spa.flags,
			  nfit_entry->spa.attrs,
			  nfit_entry->spa.idx);
			printk("           : guid: %x %x %x ",
			  nfit_entry->spa.art_guid.a,
			  nfit_entry->spa.art_guid.b,
			  nfit_entry->spa.art_guid.c);
			for(int i = 0; i < 8; i++) {
				printk("%x ", nfit_entry->spa.art_guid.d[i]);
			}
			printk("\n");
		} else if(nfit_entry->type == NFIT_TYPE_REGION) {
			printk(
			  ":: nfit reg: base = %lx, spa_idx = %d, offset = %lx, length = %lx, state = %x\n",
			  nfit_entry->region.phys_base,
			  nfit_entry->region.spa_range_idx,
			  nfit_entry->region.offset,
			  nfit_entry->region.length,
			  nfit_entry->region.state);
		}
		ptr += nfit_entry->length;
	}
}
