#include <system.h>
#include <debug.h>
#include <memory.h>
#include <arch/x86_64-acpi.h>

struct __packed hpet_desc {
	struct sdt_header header;
	uint8_t hw_rev_id;
	uint8_t comp_count:5;
	uint8_t counter_size:1;
	uint8_t _res:1;
	uint8_t leg_repl:1;
	uint16_t ven_id;
	uint32_t pci_addr_data;
	uint64_t address;
	uint8_t hpet_number;
	uint16_t min_tick;
	uint8_t page_prot;
};

#define HPET_CAP 0
#define HPET_CONFIG 0x10
#define HPET_INTSTATUS 0x20
#define HPET_COUNTER 0xF0
#define HPET_ENABLE_CNF 1
#define HPET_COUNT_SIZE_64 (1 << 13)

#define HPET_TIMERN_CONFIG(n) (0x100 + 0x20 * (n)) 
#define HPET_TN_CONFIG_ENABLE (1 << 2)

static struct hpet_desc *hpet;
static uint32_t countperiod;

__noinstrument static inline uint64_t hpet_read64(int offset)
{
	return *(volatile uint64_t *)(mm_ptov(hpet->address + offset));
}

__noinstrument static inline void hpet_write64(int offset, uint64_t data)
{
	*(volatile uint64_t *)(mm_ptov(hpet->address + offset)) = data;
}

__orderedinitializer(__orderedafter(ACPI_INITIALIZER_ORDER))
static void hpet_init(void)
{
	if(!(hpet = acpi_find_table("HPET"))) {
		panic("HPET not found");
	}

	uint64_t tmp = hpet_read64(HPET_CAP);
	countperiod = tmp >> 32;
	if(!(tmp & HPET_COUNT_SIZE_64)) {
		panic("HPET is a 32-bit counter");
	}
	int count = (tmp >> 8) & 0xf;
	/* disable */
	tmp = hpet_read64(HPET_CONFIG);
	tmp &= ~HPET_ENABLE_CNF;
	hpet_write64(HPET_CONFIG, tmp);

	printk("hpet: found %d counters\n", count);
	for(int i=0;i<count;i++) {
		tmp = hpet_read64(HPET_TIMERN_CONFIG(i));
		tmp &= ~(HPET_TN_CONFIG_ENABLE);
		hpet_write64(HPET_TIMERN_CONFIG(i), tmp);
	}

	hpet_write64(HPET_COUNTER, 0);

	/* enable */
	tmp = hpet_read64(HPET_CONFIG);
	tmp |= HPET_ENABLE_CNF;
	hpet_write64(HPET_CONFIG, tmp);
	printk("HPET: %lx\n", hpet->address);
}

uint64_t arch_processor_get_nanoseconds(void)
{
	//return *(volatile uint64_t *)(0xFFFFFF80FED00000 + HPET_COUNTER);
	//printk("--> %ld\n", hpet_read64(HPET_COUNTER));
	//printk(":: %p\n", mm_ptov(hpet->address));
	return (hpet_read64(HPET_COUNTER) * countperiod) / 1000000;
}

