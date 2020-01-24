#include <arch/x86_64-acpi.h>
#include <debug.h>
#include <memory.h>
#include <stdint.h>
#include <string.h>
#include <system.h>
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
static void *sdt_vaddr = NULL;

static struct xsdt_desc *get_xsdt_addr(void)
{
	assert(rsdp != NULL);
	assert(rsdp->rsdp.rev >= 1);
	if(sdt_vaddr == NULL) {
		sdt_vaddr = pmap_allocate(rsdp->xsdt_addr, rsdp->length, 0);
	}
	return sdt_vaddr;
}

static struct rsdt_desc *get_rsdt_addr(void)
{
	assert(rsdp != NULL);
	assert(rsdp->rsdp.rev == 0);
	if(sdt_vaddr == NULL) {
		/* TODO: determine length? */
		sdt_vaddr = pmap_allocate(rsdp->rsdp.rsdt_addr, 0x1000, 0);
	}
	return sdt_vaddr;
}

void *acpi_find_table(const char *sig)
{
	if(rsdp->rsdp.rev == 0) {
		struct rsdt_desc *rsdt = get_rsdt_addr();
		int entries = (rsdt->header.length - sizeof(rsdt->header)) / 4;
		for(int i = 0; i < entries; i++) {
			struct sdt_header *head = mm_ptov(rsdt->sdts[i]);
			if(!strncmp(head->sig, sig, 4))
				return head;
		}
	} else {
		struct xsdt_desc *xsdt = get_xsdt_addr();
		int entries = (xsdt->header.length - sizeof(xsdt->header)) / 8;
		for(int i = 0; i < entries; i++) {
			struct sdt_header *head = mm_ptov(xsdt->sdts[i]);
			if(!strncmp(head->sig, sig, 4))
				return head;
		}
	}
	return NULL;
}

static bool check_rsdp(void *ptr)
{
	struct rsdp2_desc *desc = ptr;
	if(strncmp(desc->rsdp.sig, RSDP_SIG, 8))
		return false;
	if(desc->rsdp.rev) {
		uint8_t cs = 0;
		for(unsigned char *c = ptr; c < (unsigned char *)ptr + desc->length; c++)
			cs += *c;
		return cs == 0;
	} else {
		struct rsdp_desc *r = ptr;
		uint8_t cs = 0;
		for(unsigned char *c = ptr; c < (unsigned char *)ptr + sizeof(*r); c++)
			cs += *c;
		return cs == 0;
	}
}

static void found_rsdp(struct rsdp_desc *ptr)
{
	rsdp = (void *)ptr;
}

static char rsdp_copy[sizeof(struct rsdp2_desc)];

void acpi_set_rsdp(void *ptr, size_t sz)
{
	/* this happens really early on, when we're still identity mapped to 1GB, etc. So we can just
	 * copy this out for later (which we need to do anyway) */
	if(rsdp) {
		/* if we have a revision 0 (ACPI 1.0) pointer, then try this new one.
		 * We'd like a newer version, if possible. */
		if(rsdp->rsdp.rev != 0)
			return;
	}
	if(sz > sizeof(rsdp_copy)) {
		sz = sizeof(rsdp_copy);
	}
	if(!check_rsdp(ptr)) {
		printk("[acpi] platform/bootloader specified RSDP valid validation.\n");
		return;
	}
	memcpy(rsdp_copy, ptr, sz);
	found_rsdp((void *)rsdp_copy);
	printk("[acpi] found platform/bootloader specified RSDP.\n");
}

__orderedinitializer(ACPI_INITIALIZER_ORDER) static void acpi_init(void)
{
	/* did we get one from the bootloader or platform? */
	if(rsdp) {
		return;
	}
	panic("unsupported: bootloader did not pass us RDSP");
#if 0
	uintptr_t ebda = *(uint16_t *)mm_ptov(0x40E) << 4;
	for(uintptr_t search = ebda; search < (ebda + 1024); search += 16) {
		struct rsdp_desc *desc = mm_ptov(search);
		if(check_rsdp(desc)) {
			found_rsdp(desc);
			return;
		}
	}
	/* didn't find it there; we can also try between 0xE0000 -> 0xFFFFF */
	for(uintptr_t search = 0xE0000; search < 0x100000; search += 16) {
		struct rsdp_desc *desc = mm_ptov(search);
		if(check_rsdp(desc)) {
			found_rsdp(desc);
			return;
		}
	}
	printk("warning - couldn't find ACPI tables\n");
#endif
}
