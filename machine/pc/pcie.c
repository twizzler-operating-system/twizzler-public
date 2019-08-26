#include <arch/x86_64-acpi.h>
#include <debug.h>
#include <machine/pc-pcie.h>
#include <memory.h>
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

__orderedinitializer(__orderedafter(ACPI_INITIALIZER_ORDER)) static void hpet_init(void)
{
	if(!(mcfg = acpi_find_table("MCFG"))) {
		printk("[pcie] WARNING - no PCI Extended Configuration Space found!\n");
		return;
	}

	mcfg_entries =
	  (mcfg->header.length - sizeof(struct mcfg_desc)) / sizeof(struct mcfg_desc_entry);

	printk("[pcie] found MCFG table with %ld entries\n", mcfg_entries);
}
