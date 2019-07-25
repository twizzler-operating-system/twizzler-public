struct __asan_global_source_location {
	const char *filename;
	int line_no;
	int column_no;
};

// This structure describes an instrumented global variable.
struct __asan_global {
	uintptr_t beg;               // The address of the global.
	uintptr_t size;              // The original size of the global.
	uintptr_t size_with_redzone; // The size with the redzone.
	const char *name;            // Name as a C string.
	const char *module_name;     // Module name as a C string. This pointer is a
	// unique identifier of a module.
	uintptr_t has_dynamic_init; // Non-zero if the global has dynamic initializer.
	struct __asan_global_source_location *location; // Source location of a global,
	// or NULL if it is unknown.
	uintptr_t odr_indicator; // The address of the ODR indicator symbol.
};

void __asan_storeN_noabort(uintptr_t addr, size_t sz);
void __asan_loadN_noabort(uintptr_t addr, size_t sz);
void __asan_report_storeN(uintptr_t addr, size_t sz, bool abort);
void __asan_report_loadN(uintptr_t addr, size_t sz, bool abort);

#define __asan_report_template(type, sz)                                                           \
	__attribute__((no_sanitize_address)) void __asan_##type##sz##_noabort(uintptr_t addr)          \
	{                                                                                              \
		__asan_report_##type##N(addr, sz, true);                                                   \
	}                                                                                              \
	__attribute__((no_sanitize_address)) void __asan_report_##type##sz(uintptr_t addr)             \
	{                                                                                              \
		__asan_report_##type##N(addr, sz, false);                                                  \
	}

__asan_report_template(store, 1);
__asan_report_template(store, 2);
__asan_report_template(store, 4);
__asan_report_template(store, 8);
__asan_report_template(store, 16);
__asan_report_template(load, 1);
__asan_report_template(load, 2);
__asan_report_template(load, 4);
__asan_report_template(load, 8);
__asan_report_template(load, 16);

__attribute__((no_sanitize_address)) void __asan_report_storeN(uintptr_t addr,
  size_t sz,
  bool abort)
{
}

__attribute__((no_sanitize_address)) void __asan_report_loadN(uintptr_t addr, size_t sz, bool abort)
{
}

__attribute__((no_sanitize_address)) void __asan_report_store_n(uintptr_t addr, size_t sz)
{
	__asan_report_storeN(addr, sz, true);
}

__attribute__((no_sanitize_address)) void __asan_report_load_n(uintptr_t addr, size_t sz)
{
	__asan_report_loadN(addr, sz, true);
}

__attribute__((no_sanitize_address)) void __asan_storeN_noabort(uintptr_t addr, size_t sz)
{
	__asan_report_storeN(addr, sz, false);
}

__attribute__((no_sanitize_address)) void __asan_loadN_noabort(uintptr_t addr, size_t sz)
{
	__asan_report_loadN(addr, sz, false);
}

__attribute__((no_sanitize_address)) void __asan_handle_no_return(void)
{
}

__attribute__((no_sanitize_address)) void __asan_register_globals(struct __asan_global *globals,
  size_t size)
{
}

__attribute__((no_sanitize_address)) void __asan_unregister_globals(struct __asan_global *globals,
  size_t size)
{
}

__attribute__((no_sanitize_address)) void __asan_alloca_poison(uintptr_t addr, size_t size)
{
}

__attribute__((no_sanitize_address)) void __asan_allocas_unpoison(const void *stack_top,
  const void *stack_bottom)
{
}
