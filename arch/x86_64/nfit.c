#include <arch/x86_64-acpi.h>
#include <debug.h>
#include <kalloc.h>
#include <lib/iter.h>
#include <lib/list.h>
#include <memory.h>
#include <nvdimm.h>
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

struct spa {
	struct list entry, regions;
	struct nfit_entry *nfit;
};

struct region {
	struct list entry, spa_entry;
	struct spa *spa;
	struct nfit_entry *nfit;
};

static DECLARE_LIST(spa_list);
static DECLARE_LIST(region_list);

__orderedinitializer(__orderedafter(ACPI_INITIALIZER_ORDER)) static void nfit_init(void)
{
	if(!(nfit = acpi_find_table("NFIT"))) {
		return;
	}

	char *start = (char *)&nfit->nfits[0];
	char *ptr = start;
	while(ptr < start + nfit->header.length) {
		struct nfit_entry *nfit_entry = (struct nfit_entry *)ptr;
		if(nfit_entry->type == NFIT_TYPE_SPA) {
			struct spa *s = kalloc(sizeof(*s));
			list_init(&s->regions);
			s->nfit = nfit_entry;
			list_insert(&spa_list, &s->entry);
		} else if(nfit_entry->type == NFIT_TYPE_REGION) {
			struct region *r = kalloc(sizeof(*r));
			r->spa = NULL;
			r->nfit = nfit_entry;
			foreach(e, list, &spa_list) {
				struct spa *s = list_entry(e, struct spa, entry);
				if(s->nfit->spa.idx == nfit_entry->region.spa_range_idx) {
					r->spa = s;
					list_insert(&s->regions, &r->spa_entry);
					break;
				}
			}
			if(r->spa == NULL) {
				printk("[nfit] warning - unable to locate SPA %d for region %d:%d:%d\n",
				  nfit_entry->region.spa_range_idx,
				  nfit_entry->region.dev_handle,
				  nfit_entry->region.phys_id,
				  nfit_entry->region.region_id);
			} else {
				list_insert(&region_list, &r->entry);
			}
		}
		ptr += nfit_entry->length;
	}

	foreach(e, list, &region_list) {
		struct region *r = list_entry(e, struct region, entry);
		struct nv_device *dev = nv_lookup_device(r->nfit->region.dev_handle);
		if(!dev) {
			nv_register_device(r->nfit->region.dev_handle, NULL);
			dev = nv_lookup_device(r->nfit->region.dev_handle);
		}

		nv_register_region(dev,
		  r->nfit->region.region_id,
		  r->nfit->region.offset + r->spa->nfit->spa.base,
		  r->nfit->region.length,
		  NULL,
		  0);
	}
}
