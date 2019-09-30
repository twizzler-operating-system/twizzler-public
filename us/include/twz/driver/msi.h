#pragma once

#define device_msi_addr(did) ({ (0xfeeul << 20) | ((did) << 12); })

#define MSI_EDGE 0ul
#define MSI_LEVEL (1ul << 15)
#define device_msi_data(vec, fl) ({ (vec) | (fl); })
//#define X86_64_MSI_LEVEL_ASSERT (1ul << 14)
//#define X86_64_MSI_LEVEL_DEASSERT (0)
//#define X86_64_MSI_DELIVERY_FIXED 0
//#define X86_64_MSI_DELIVERY_LOW (1ul << 8)

#ifndef __KERNEL__

#include <twz/driver/pcie.h>
#include <twz/obj.h>

static void msix_configure(struct object *co, struct pcie_msix_capability *m, int nrvecs)
{
	struct pcie_function_header *hdr = twz_device_getds(co);
	struct device_repr *repr = twz_device_getrepr(co);
	uint8_t bir = m->table_offset_bir & 0x7;
	uint32_t off = m->table_offset_bir & ~0x7;
	volatile struct pcie_msix_table_entry *tbl =
	  twz_ptr_lea(co, (void *)((long)hdr->bars[bir] + off));
	for(int i = 0; i < nrvecs; i++, tbl++) {
		tbl->data = device_msi_data(repr->interrupts[i].vec, MSI_LEVEL);
		tbl->addr = device_msi_addr(0);
		tbl->ctl = 0;
	}
	m->fn_mask = 0;
	m->msix_enable = 1;
}

#endif
