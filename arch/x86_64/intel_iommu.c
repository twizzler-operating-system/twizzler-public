#include <arch/x86_64-acpi.h>
#include <debug.h>
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
	for(size_t i = 0; i < remap_entries; i++) {
		size_t scope_entries =
		  (dmar->remaps[i].length - sizeof(struct dmar_remap)) / sizeof(struct device_scope);
		printk("[iommu]  remap: type=%d, length=%d, flags=%x, segnr=%d, base_addr=%lx, %ld device "
		       "scopes\n",
		  dmar->remaps[i].type,
		  dmar->remaps[i].length,
		  dmar->remaps[i].flags,
		  dmar->remaps[i].segnr,
		  dmar->remaps[i].reg_base_addr,
		  scope_entries);
		for(size_t j = 0; j < scope_entries; j++) {
			size_t path_len = (dmar->remaps[i].scopes[j].length - sizeof(struct device_scope)) / 2;
			printk("[iommu]    dev_scope: type=%d, length=%d, enumer_id=%d, start_bus=%d, "
			       "path_len=%ld\n",
			  dmar->remaps[i].scopes[j].type,
			  dmar->remaps[i].scopes[j].length,
			  dmar->remaps[i].scopes[j].enumer_id,
			  dmar->remaps[i].scopes[j].start_bus_nr,
			  path_len);
			for(size_t k = 0; k < path_len; k++) {
				printk("[iommu]      path %ld: %x\n", k, dmar->remaps[i].scopes[j].path[k]);
			}
		}
	}
}
