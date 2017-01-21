#include <arch/x86_64-acpi.h>
#include <stdint.h>
#include <system.h>
#include <debug.h>
#include <arch/x86_64-madt.h>
#include <processor.h>

struct __attribute__((packed)) madt_desc {
	struct sdt_header header;
	uint32_t lca;
	uint32_t flags;
};

#define LAPIC_ENTRY 0
#define IOAPIC_ENTRY 1

struct __attribute__((packed)) lapic_entry {
	struct madt_record rec;
	uint8_t acpiprocid;
	uint8_t apicid;
	uint32_t flags;
};

static struct madt_desc *madt;

__orderedinitializer(MADT_INITIALIZER_ORDER + __orderedafter(PROCESSOR_INITIALIZER_ORDER))
static void madt_init(void)
{
	madt = acpi_find_table("APIC");
	assert(madt != NULL);

	uintptr_t table = (uintptr_t)(madt + 1);
	int entries = madt->header.length - sizeof(*madt);
	for(int i=0;i<entries;i++) {
		struct madt_record *rec = (void *)table;
		if(rec->reclen == 0)
			break;
		switch(rec->type) {
			struct lapic_entry *lapic;
			case LAPIC_ENTRY:
				lapic = (void *)rec;
				if(lapic->flags & 1) {
					processor_register(lapic->apicid == 0, lapic->acpiprocid);
				}
				break;
			case IOAPIC_ENTRY:
				ioapic_init((struct ioapic_entry *)rec);
				break;
			default:
				break;
		}
		table += rec->reclen;
	}
}

