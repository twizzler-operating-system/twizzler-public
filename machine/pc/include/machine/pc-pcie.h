#pragma once

#include <driver/pcie.h>

#include <lib/list.h>
#include <spinlock.h>
struct pcie_function {
	struct pcie_config_space *config;
	uint16_t segment;
	uint8_t bus, device, function;
	uint8_t flags;
	struct spinlock lock;
	struct list entry;

	size_t bar_sizes[6];
};
