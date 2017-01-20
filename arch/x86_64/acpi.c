#include <system.h>
#include <arch/x86_64-acpi.h>
#include <string.h>
#include <stdint.h>
#include <memory.h>
#include <debug.h>
#define RSDP_SIG "RSD PTR " /* yes, there is an extra space here */

struct __attribute__((packed)) rsdp_desc {
	char sig[8];
	uint8_t checksum;
	char oemid[6];
	uint8_t rev;
	uint32_t rsdt_addr;
};

struct __attribute__((packed)) rsdp2_desc {
	struct rsdp_desc rsdp;
	uint32_t length;
	uint64_t xsdt_addr;
	uint8_t extchecksum;
	uint8_t __res[3];
};

struct __attribute__((packed)) xsdt_desc {
	struct sdt_header header;
	uint64_t sdts[];
};

struct __attribute__((packed)) rsdt_desc {
	struct sdt_header header;
	uint32_t sdts[];
};

static struct rsdp2_desc *rsdp;

static struct xsdt_desc *get_xsdt_addr(void)
{
	assert(rsdp != NULL);
	assert(rsdp->rsdp.rev >= 1);
	return (void *)(rsdp->xsdt_addr + PHYSICAL_MAP_START);
}

static struct rsdt_desc *get_rsdt_addr(void)
{
	assert(rsdp != NULL);
	assert(rsdp->rsdp.rev == 0);
	return (void *)(rsdp->rsdp.rsdt_addr + PHYSICAL_MAP_START);
}

void *acpi_find_table(const char *sig)
{
	if(rsdp->rsdp.rev == 0) {
		struct rsdt_desc *rsdt = get_rsdt_addr();
		int entries = (rsdt->header.length - sizeof(rsdt->header)) / 4;
		for(int i=0;i<entries;i++) {
			struct sdt_header *head = (void *)(rsdt->sdts[i] + PHYSICAL_MAP_START);
			if(!strncmp(head->sig, sig, 4))
				return head;
		}
	} else {
		struct xsdt_desc *xsdt = get_xsdt_addr();
		int entries = (xsdt->header.length - sizeof(xsdt->header)) / 8;
		for(int i=0;i<entries;i++) {
			struct sdt_header *head = (void *)(xsdt->sdts[i] + PHYSICAL_MAP_START);
			if(!strncmp(head->sig, sig, 4))
				return head;
		}
	}
	return NULL;
}

static void found_rsdp(struct rsdp_desc *ptr)
{
	rsdp = (void *)ptr;
}

__orderedinitializer(ACPI_INITIALIZER_ORDER) static void acpi_init(void)
{
	uintptr_t ebda = *(uint16_t *)(PHYSICAL_MAP_START + 0x40E) << 4;
	for(uintptr_t search = ebda; search < (ebda + 1024); search += 16) {
		struct rsdp_desc *desc = (void *)(search + PHYSICAL_MAP_START);
		if(!strncmp(desc->sig, RSDP_SIG, 8)) {
			found_rsdp(desc);
			return;
		}
	}
	/* didn't find it there; we can also try between 0xE0000 -> 0xFFFFF */
	for(uintptr_t search = 0xE0000; search < 0x100000; search += 16) {
		struct rsdp_desc *desc = (void *)(search + PHYSICAL_MAP_START);
		if(!strncmp(desc->sig, RSDP_SIG, 8)) {
			found_rsdp(desc);
			return;
		}
	}
	printk("warning - couldn't find ACPI tables\n");
}

