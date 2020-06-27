#pragma once
#ifndef __packed
#define __packed __attribute__((packed))
#endif

#include <stddef.h>
#include <stdint.h>

#include <twz/_kso.h>

#define PCIE_BUS_HEADER_MAGIC 0x88582323

#define KACTION_CMD_PCIE_INIT_DEVICE 1

struct pcie_function_header {
	struct kso_hdr hdr;
	uint16_t deviceid;
	uint16_t vendorid;
	uint16_t classid;
	uint16_t subclassid;
	uint16_t progif;
	uint16_t flags;
	uint16_t bus;
	uint16_t device;
	uint16_t function;
	uint16_t segment;
	uint32_t resv;
	uint32_t prefetch[6];
	volatile void *bars[6];
	size_t barsz[6];
	struct pcie_config_space *space;
};

struct pcie_config_space;
struct pcie_bus_header {
	uint32_t magic;
	uint32_t start_bus;
	uint32_t end_bus;
	uint32_t segnr;
	uint64_t flags;
	struct pcie_config_space *spaces;
};

/* PCIe extends the PCI configuration space in a backwards compatible way:
 *   - The first 256 bytes of the configuration space is identical to the PCI config space.
 *     - Note that some fields have new restrictions and requirements.
 *   - The configuration space _must_ be memory-mapped. This is really nice for us, because we
 *     require this.
 *
 * The configuration space is broken into two pieces: the header, which is common to all config
 * spaces, and the rest, which can be either a device function (header type 0) or a PCI-PCI bridge
 * (header type 1). The fields are defined in the PCI base specification, retrieved from version
 * 3.0.
 */

struct __packed pcie_config_space_header {
	/* 0x00 */
	uint16_t vendor_id;
	uint16_t device_id;
	/* 0x04 */
	uint16_t command;
	uint16_t status;
	/* 0x08 */
	uint8_t revision;
	uint8_t progif;
	uint8_t subclass;
	uint8_t class_code;
	/* 0x0C */
	uint8_t cache_line_size;
	uint8_t latency_timer;
	uint8_t header_type;
	uint8_t bist;
};

struct __packed pcie_config_space {
	struct pcie_config_space_header header;
	union {
		struct __packed {
			/* 0x10 */
			uint32_t bar[6];
			/* 0x28 */
			uint32_t cardbus_cis_pointer;
			/* 0x2C */
			uint16_t subsystem_vendor_id;
			uint16_t subsystem_id;
			/* 0x30 */
			uint32_t expansion_rom_base_address;
			/* 0x34 */
			uint32_t cap_ptr : 8;
			uint32_t reserved0 : 24;
			/* 0x38 */
			uint32_t reserved1;
			/* 0x3C */
			uint8_t interrupt_line;
			uint8_t interrupt_pin;
			uint8_t min_grant;
			uint8_t max_latency;
		} device;
		struct __packed {
			uint32_t bar[2];
			uint8_t primary_bus_nr;
			uint8_t secondary_bus_nr;
			uint8_t subordinate_bus_nr;
			uint8_t secondary_latency_timer;
			uint8_t io_base;
			uint8_t io_limit;
			uint8_t secondary_status;
			uint16_t memory_base;
			uint16_t memory_limit;
			uint16_t pref_memory_base;
			uint16_t pref_memory_limit;
			/* 28 */
			uint32_t pref_base_upper;
			uint32_t pref_limit_upper;
			uint16_t io_base_upper;
			uint16_t io_limit_upper;
			uint32_t cap_ptr : 8;
			uint32_t reserved0 : 24;
			uint32_t exp_rom_base;
			uint8_t interrupt_line;
			uint8_t interrupt_pin;
			uint16_t bridge_control;
		} bridge;
	};
};

/* The cap_ptr field is an offset into the configuration space starting a linked list of additional
 * capabilities the device has. Each capability starts with a header that contains an ID identifying
 * it and a next pointer (also an offset into the config space) indicating the next capability.
 * next=0 indicates the last link. */

struct __packed pcie_capability_header {
	uint8_t capid;
	uint8_t next;
};

/* power management support: this is detailed in the PCI power management specification */

#define PCIE_POWER_CAPABILITY_ID 1

struct __packed pcie_power_capability {
	struct pcie_capability_header header;
	/* PMC register */
	uint16_t version : 3;
	uint16_t pme_clock : 1;
	uint16_t resv0 : 1;
	uint16_t dsi : 1;
	uint16_t aux_current : 3;
	uint16_t d1_support : 1;
	uint16_t d2_support : 1;
	uint16_t pme_support : 5;

	/* PM control */
	uint16_t powerstate : 2;
	uint16_t xxx : 1;
	uint16_t no_soft_reset : 1;
	uint16_t resv1 : 4;
	uint16_t pme_enable : 1;
	uint16_t data_select : 4;
	uint16_t data_scale : 2;
	uint16_t pme_status : 1;

	/* PMCSR_BSE */
	uint8_t resv2 : 6;
	uint8_t b2_b3 : 1;
	uint8_t bpcc_enable : 1;
	uint8_t data;
};

/* Message Signaled Interrupt support. Detailed in the PCI 3.0 base specification */

#define PCIE_MSI_CAPABILITY_ID 5

struct __packed pcie_msi_capability {
	struct pcie_capability_header header;
	/* message control */
	uint16_t msi_enable : 1;
	uint16_t mmc : 3;
	uint16_t mme : 3;
	uint16_t support_64bit : 1;
	uint16_t per_vec_mask : 1;
	uint16_t resv0 : 7;

	uint32_t msg_addr;
	uint32_t msg_addr_upper;

	uint16_t msg_data;
	uint32_t mask_bits;
	uint32_t pending_bits;
};

/* Message Signaled Interrupt-X support. Detailed in the PCI 3.0 base specification */
#define PCIE_MSIX_CAPABILITY_ID 0x11

struct __packed pcie_msix_capability {
	struct pcie_capability_header header;
	/* message control */
	uint16_t table_size : 11;
	uint16_t resv0 : 3;
	uint16_t fn_mask : 1;
	uint16_t msix_enable : 1;
	uint32_t table_offset_bir;
	uint32_t pba_offset_bir;
};

union pcie_capability_ptr {
	struct pcie_capability_header *header;
	struct pcie_power_capability *power;
	struct pcie_msi_capability *msi;
	struct pcie_msix_capability *msix;
};

struct __packed pcie_msix_table_entry {
	uint64_t addr;
	uint32_t data;
	uint32_t ctl;
};

#define HEADER_TYPE_DEVICE 0
#define HEADER_TYPE_BRIDGE 1
#define HEADER_TYPE(x) (x & 0x7f)

#define HEADER_MULTIFUNC (1 << 7)

#define BIST_CAPABLE (1 << 7)
#define BIST_START (1 << 6)
#define BIST_COMPLETION_CODE (0xf)

#define COMMAND_IOSPACE (1 << 0)
#define COMMAND_MEMORYSPACE (1 << 1)
#define COMMAND_BUSMASTER (1 << 2)
#define COMMAND_PARITYERR (1 << 6)
#define COMMAND_SERRENABLE (1 << 8)
#define COMMAND_INTDISABLE (1 << 10)

#define BCTRL_PARITYERR (1 << 0)
#define BCTRL_SERRENABLE (1 << 1)
#define BCTRL_RESET (1 << 6)

#define STATUS_INTERRUPT (1 << 3)
#define STATUS_PARITYERR (1 << 8)
#define STATUS_TARGETABRT (1 << 11)
#define STATUS_RECV_TARGETABRT (1 << 12)
#define STATUS_RECV_MASTER_TARGETABRT (1 << 12)
#define STATUS_SYSTEMERR (1 << 14)
#define STATUS_DET_PARITYERR (1 << 15)

#define MEMORY_BAR_ADDRESS(m) ((m)&0xfffffffffffffff0)
#define MEMORY_BAR_TYPE(m) (((m)&6) >> 1)
#define MEMORY_BAR_PREFETCH(m) (m & (1 << 3))

#ifndef __KERNEL__

#ifdef __cplusplus
extern "C" {
#endif
bool pcief_capability_get(struct pcie_function_header *pf, int id, union pcie_capability_ptr *cap);
#ifdef __cplusplus
}
#endif

#endif
