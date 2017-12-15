#include <ksymbol.h>
#include <debug.h>

#define INSTRUMENT_TABLE_LEN 1024

struct tracepoint {
	void *fn;
	void *caller;
	long long start;
};

struct tracefn {
	const struct ksymbol *ks;
	unsigned long count;
};

static struct tracefn[INSTRUMENT_TABLE_LEN];

/* TODO: percpu */
static void *function_trace_stack[1024]= {NULL};
static int fts_ind = 0;

static int disable_trace = 0;

__noinstrument
void __cyg_profile_func_enter(void *func, void *caller __unused)
{
	if(unlikely(kernel_symbol_table_length >= INSTRUMENT_TABLE_LEN)) {
		panic("--> increase INSTRUMENT_TABLE_LEN");
	}
	if(disable_trace) return;
	__unused const struct ksymbol * volatile ks = ksymbol_find_by_value((uintptr_t)func, true);
	if(fts_ind == 1024) {
		panic("function_trace_stack exceeded");
	}
	if(function_trace_stack[fts_ind] == func) {
		panic("direct recusive call detected");
	}
	for(int i=0;i<fts_ind;i++) {
		if(function_trace_stack[i] == func) {
			return;
		}
	}
	function_trace_stack[fts_ind++] = func;
}

__noinstrument
void __cyg_profile_func_exit(void *func, void *caller __unused)
{
	if(disable_trace) return;
	if(fts_ind == 0) {
		panic("function_trace_stack underflow");
	}
	if(function_trace_stack[fts_ind-1] == func) {
		function_trace_stack[--fts_ind] = NULL;
	}
}

