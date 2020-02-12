#pragma once

struct arch_object {
	uintptr_t pt_root;
	uint64_t *pd;
	uint64_t **pts;
};

struct arch_object_space {
	uintptr_t ept_phys;
	uint64_t *ept;
	uint64_t **pdpts;
	size_t *counts;
};
