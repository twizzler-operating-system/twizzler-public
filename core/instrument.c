#include <ksymbol.h>
#include <debug.h>
#include <lib/inthash.h>
#include <string.h>
#include <processor.h>
#include <lib/lib.h>
#include <slab.h>
#include <debug.h>
#define INSTRUMENT_TABLE_LEN 1024

struct tracepoint {
	void *fn;
	void *caller;
	unsigned long long start;
};

static struct tracepoint function_trace_stack[1024] = {NULL};
static int fts_ind = 0;

/* TODO: percpu */
static int disable_trace = 0;
static bool disable_trace_output = true;

__noinstrument
void instrument_disable(void)
{
	disable_trace++;
}

__noinstrument
void instrument_enable(void)
{
	disable_trace--;
}

__noinstrument
void __cyg_profile_func_enter(void *func, void *caller)
{
	if(disable_trace) return;
	instrument_disable();
	if(unlikely(kernel_symbol_table_length >= INSTRUMENT_TABLE_LEN)) {
		panic("--> increase INSTRUMENT_TABLE_LEN");
	}
	if(fts_ind == 1024) {
		panic("function_trace_stack exceeded");
	}
	if(function_trace_stack[fts_ind].fn == func) {
		panic("direct recusive call detected");
	}
#if 0
	for(int i=0;i<fts_ind-1;i++) {
		if(function_trace_stack[i].fn == func) {
			instrument_enable();
			return;
		}
	}
#endif
	function_trace_stack[fts_ind].fn = func;
	function_trace_stack[fts_ind].caller = caller;
	instrument_enable();
	function_trace_stack[fts_ind++].start = arch_processor_timestamp();
}

__noinstrument
void __cyg_profile_func_exit(void *func, void *caller __unused)
{
	unsigned long long ts = arch_processor_timestamp();
	if(disable_trace) return;
	instrument_disable();
	if(fts_ind == 0) {
		panic("function_trace_stack underflow");
	}
	if(function_trace_stack[fts_ind-1].fn == func) {
		const struct ksymbol *ks = ksymbol_find_by_value((uintptr_t)func, true);
		if(ks && !disable_trace_output) {
			printk("{instrtrace %d %s %lld %lld}", fts_ind, ks->name,
					function_trace_stack[fts_ind-1].start, ts);
		}
		function_trace_stack[--fts_ind].fn = NULL;
	}
	instrument_enable();
}

static void __calibrate_instrumentation(void);
__attribute__((noinline))
static void __calibration_amount(void)
{
	__cyg_profile_func_enter(__calibration_amount, __calibrate_instrumentation);
	__cyg_profile_func_exit(__calibration_amount, __calibrate_instrumentation);
}

__initializer
__noinstrument
static void __calibrate_instrumentation(void)
{
	printk("calibrating instrumentation system\n");
	for(int i=0;i<100000;i++) __calibration_amount();
	disable_trace_output = false;
}

