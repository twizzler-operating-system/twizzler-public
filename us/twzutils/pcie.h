#pragma once

#include <twz/driver/pcie.h>

#define PCIE_FUNCTION_INIT 1
#define PCIE_FUNCTION_DRIVEN 2

struct pcie_function {
	volatile struct pcie_config_space *config;
	uint16_t segment;
	uint8_t bus, device, function;
	uint8_t flags;
	size_t bar_sizes[6];
	struct pcie_function *next;
	objid_t cid;
	struct object cobj;
};
