#pragma once
#include <stdint.h>
#define ACPI_INITIALIZER_ORDER 0
void *acpi_find_table(const char *sig);
void acpi_set_rsdp(void *ptr, size_t sz);

struct sdt_header {
	char sig[4];
	uint32_t length;
	uint8_t rev;
	uint8_t checksum;
	char oemid[6];
	char oemtableid[8];
	uint32_t oemrev;
	uint32_t creatorid;
	uint32_t creatorrev;
} __attribute__((packed));
