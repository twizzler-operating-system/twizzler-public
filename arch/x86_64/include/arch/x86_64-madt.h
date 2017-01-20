#pragma once

#include <arch/x86_64-acpi.h>
#include <system.h>

#define MADT_INITIALIZER_ORDER __orderedafter(ACPI_INITIALIZER_ORDER)

struct __attribute__((packed)) madt_record {
	uint8_t type;
	uint8_t reclen;
};

struct __attribute__((packed)) ioapic_entry {
	struct madt_record rec;
	uint8_t apicid;
	uint8_t __res0;
	uint32_t ioapicaddr;
	uint32_t gsib;
};

void ioapic_init(struct ioapic_entry *entry);

