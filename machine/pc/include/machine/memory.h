#pragma once
#define KERNEL_LOAD_OFFSET 0x400000
#define KERNEL_PHYSICAL_BASE 0x0
#define KERNEL_VIRTUAL_BASE 0xFFFFFFFF80000000
#define PHYSICAL_MAP_START 0xFFFFFF8000000000
#define PHYSICAL_MAP_END                                                                           \
	KERNEL_VIRTUAL_BASE /* TODO (minor): figure out better systems for this                        \
	                     */
