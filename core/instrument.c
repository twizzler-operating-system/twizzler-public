#include <ksymbol.h>
#include <debug.h>
#include <lib/inthash.h>
#include <string.h>
#include <processor.h>
#include <lib/lib.h>
#include <slab.h>
#include <debug.h>
#include <init.h>
#define INSTRUMENT_TABLE_LEN 1024

struct tracepoint {
	void *fn;
	void *caller;
	unsigned long long start;
};

_Atomic int disable_trace = 100;
static DECLARE_PER_CPU(struct tracepoint, function_trace_stack)[1024] = {NULL};
static DECLARE_PER_CPU(_Atomic int, disable_trace_local) = 100;
static DECLARE_PER_CPU(int, fts_ind) = 0;

__noinstrument
void instrument_disable(void)
{
	if(disable_trace)
		return;
	_Atomic int *disable_trace_local = per_cpu_get(disable_trace_local);
	(*disable_trace_local)++;
}

__noinstrument
void instrument_enable(void)
{
	if(disable_trace)
		return;
	_Atomic int *disable_trace_local = per_cpu_get(disable_trace_local);
	(*disable_trace_local)--;
}

__noinstrument
bool instrument_disabled(void)
{
	if(disable_trace)
		return true;
	_Atomic int *disable_trace_local = per_cpu_get(disable_trace_local);
	return *disable_trace_local != 0;
}

__noinstrument
void __cyg_profile_func_enter(void *func, void *caller)
{
	if(instrument_disabled()) return;
	instrument_disable();
	if(unlikely(kernel_symbol_table_length >= INSTRUMENT_TABLE_LEN)) {
		panic("--> increase INSTRUMENT_TABLE_LEN");
	}
	int *fi = per_cpu_get(fts_ind);
	if(*fi == 1024) {
		panic("function_trace_stack exceeded");
	}
	struct tracepoint *function_trace_stack = *per_cpu_get(function_trace_stack);
	if(function_trace_stack[*fi].fn == func) {
		panic("direct recusive call detected");
	}
	function_trace_stack[*fi].fn = func;
	function_trace_stack[*fi].caller = caller;
	instrument_enable();
	function_trace_stack[(*fi)++].start = arch_processor_timestamp();
}

__noinstrument
void __cyg_profile_func_exit(void *func, void *caller __unused)
{
	unsigned long long ts = arch_processor_timestamp();
	if(instrument_disabled()) return;
	instrument_disable();
	int *fi = per_cpu_get(fts_ind);
	if(*fi == 0) {
		instrument_enable();
		return;
	}
	struct tracepoint *function_trace_stack = *per_cpu_get(function_trace_stack);
	if(function_trace_stack[*fi-1].fn == func) {
		const struct ksymbol *ks = ksymbol_find_by_value((uintptr_t)func, true);
		if(ks) {
	//		printk("{instrtrace %d %d %s %lld %lld}", current_processor->id, *fi, ks->name,
	//				function_trace_stack[*fi-1].start, ts);
		}
		function_trace_stack[--(*fi)].fn = NULL;
	}
	instrument_enable();
}

static void __start_instrumentation(void *_a __unused)
{
	disable_trace = 0;
	_Atomic int *disable_trace_local = per_cpu_get(disable_trace_local);
	(*disable_trace_local) = 0;
}
POST_INIT_ALLCPUS(__start_instrumentation);

