#pragma once

#define device_msi_addr(did) ({ (0xfeeul << 20) | ((did) << 12); })

#define MSI_EDGE 0ul
#define MSI_LEVEL (1ul << 15)
#define device_msi_data(vec, fl) ({ (vec) | (fl); })
//#define X86_64_MSI_LEVEL_ASSERT (1ul << 14)
//#define X86_64_MSI_LEVEL_DEASSERT (0)
//#define X86_64_MSI_DELIVERY_FIXED 0
//#define X86_64_MSI_DELIVERY_LOW (1ul << 8)
