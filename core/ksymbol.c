#include <ksymbol.h>

/* TODO: insert these into a hash table */
__noinstrument
const struct ksymbol *ksymbol_find_by_value(uintptr_t val, bool range)
{
	for(size_t i=0;i<kernel_symbol_table_length;i++) {
		if(kernel_symbol_table[i].value == val ||
				(range && kernel_symbol_table[i].value <= val && kernel_symbol_table[i].value + kernel_symbol_table[i].size > val)) {
			return &kernel_symbol_table[i];
		}
	}
	return NULL;
}

