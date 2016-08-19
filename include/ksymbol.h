#pragma once
struct ksymbol {
	uintptr_t value;
	size_t size;
	const char *name;
};

extern const struct ksymbol kernel_symbol_table[];
extern const size_t kernel_symbol_table_length;

const struct ksymbol *ksymbol_find_by_value(uintptr_t val, bool range);
