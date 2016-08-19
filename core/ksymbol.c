#include <ksymbol.h>
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

#pragma weak kernel_symbol_table
#pragma weak kernel_symbol_table_length
__attribute__((section(".ksyms"))) __attribute__((weak,used)) const size_t kernel_symbol_table_length = 0;
__attribute__((section(".ksyms"))) const struct ksymbol kernel_symbol_table[] __attribute__ ((weak,used)) = {{0,0,0}};

