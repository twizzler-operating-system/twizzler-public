#if CONFIG_UBSAN
#include <debug.h>
#include <lib/ubsan.h>
#include <printk.h>
#include <spinlock.h>
#include <stdint.h>
#include <stdnoreturn.h>
#if UINT32_MAX == UINTPTR_MAX
#define STACK_CHK_GUARD 0xe2dee396
#else
#define STACK_CHK_GUARD 0x595e9fbd94fda766
#endif

uintptr_t __stack_chk_guard = STACK_CHK_GUARD;
__attribute__((no_sanitize_undefined)) _Noreturn void __stack_chk_fail(void)
{
	panic("Stack smashing detected");
	for(;;)
		;
}

static struct spinlock lock = SPINLOCK_INIT;

const char *type_check_kinds[] = { "load of",
	"store to",
	"reference binding to",
	"member access within",
	"member call on",
	"constructor call on",
	"downcast of",
	"downcast of" };

#define REPORTED_BIT 31

#if(__SIZEOF_LONG__ == 8) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define COLUMN_MASK (~(1U << REPORTED_BIT))
#define LINE_MASK (~0U)
#else
#define COLUMN_MASK (~0U)
#define LINE_MASK (~(1U << REPORTED_BIT))
#endif

__attribute__((no_sanitize_undefined)) static void print_source_location(const char *prefix,
  struct source_location *loc)
{
	printk(
	  "%s %s:%d:%d\n", prefix, loc->file_name, loc->line & LINE_MASK, loc->column & COLUMN_MASK);
}

__attribute__((no_sanitize_undefined)) static void ubsan_prologue(struct source_location *location)
{
	__spinlock_acquire(&lock, NULL, 0);
	panic_continue("Undefined Behavior Detected");
	print_source_location("Undefined behaviour in", location);
}

__attribute__((no_sanitize_undefined)) noreturn static void ubsan_epilogue(void)
{
	printk("----------------------\n");
	spinlock_release(&lock, 0);
	for(;;)
		;
}

__attribute__((no_sanitize_undefined)) noreturn void __ubsan_handle_builtin_unreachable(
  struct unreachable_data *data)
{
	ubsan_prologue(&data->location);
	printk("calling __builtin_unreachable()\n");
	ubsan_epilogue();
}

__attribute__((no_sanitize_undefined)) static void handle_overflow(struct overflow_data *data,
  unsigned long lhs,
  unsigned long rhs,
  char symbol)
{
	ubsan_prologue(&data->location);
	printk("signed overflow %ld %c %ld\n", lhs, symbol, rhs);
	ubsan_epilogue();
}

__attribute__((no_sanitize_undefined)) void __ubsan_handle_add_overflow(struct overflow_data *data,
  unsigned long lhs,
  unsigned long rhs)
{
	handle_overflow(data, lhs, rhs, '+');
}

__attribute__((no_sanitize_undefined)) void __ubsan_handle_sub_overflow(struct overflow_data *data,
  unsigned long lhs,
  unsigned long rhs)
{
	handle_overflow(data, lhs, rhs, '-');
}

__attribute__((no_sanitize_undefined)) void __ubsan_handle_mul_overflow(struct overflow_data *data,
  unsigned long lhs,
  unsigned long rhs)
{
	handle_overflow(data, lhs, rhs, '*');
}

__attribute__((no_sanitize_undefined)) void __ubsan_handle_divrem_overflow(
  struct overflow_data *data,
  unsigned long lhs,
  unsigned long rhs)
{
	handle_overflow(data, lhs, rhs, '/');
}

__attribute__((no_sanitize_undefined)) void __ubsan_handle_negate_overflow(
  struct overflow_data *data,
  unsigned long value)
{
	ubsan_prologue(&data->location);
	printk("signed overflow ~%ld\n", value);
	ubsan_epilogue();
}

__attribute__((no_sanitize_undefined)) static void handle_null_ptr_deref(
  struct type_mismatch_data *data)
{
	ubsan_prologue(&data->location);
	printk("%s null pointer of type %s\n",
	  type_check_kinds[data->type_check_kind],
	  data->type->type_name);
	ubsan_epilogue();
}

__attribute__((no_sanitize_undefined)) static void handle_missaligned_access(
  struct type_mismatch_data *data,
  unsigned long ptr)
{
	ubsan_prologue(&data->location);
	printk("%s misaligned address %p for type %s\n",
	  type_check_kinds[data->type_check_kind],
	  (void *)ptr,
	  data->type->type_name);
	printk("which requires %ld byte alignment\n", data->alignment);
	ubsan_epilogue();
}

__attribute__((no_sanitize_undefined)) static void handle_object_size_mismatch(
  struct type_mismatch_data *data,
  unsigned long ptr)
{
	ubsan_prologue(&data->location);
	printk("%s address %pk with insufficient space\n",
	  type_check_kinds[data->type_check_kind],
	  (void *)ptr);
	printk("for an object of type %s\n", data->type->type_name);
	ubsan_epilogue();
}
#define IS_ALIGNED(x, a) (((x) & ((typeof(x))(a)-1)) == 0)
__attribute__((no_sanitize_undefined)) void __ubsan_handle_type_mismatch(
  struct type_mismatch_data *data,
  unsigned long ptr)
{
	if(!ptr)
		handle_null_ptr_deref(data);
	else if(data->alignment && !IS_ALIGNED(ptr, data->alignment))
		handle_missaligned_access(data, ptr);
	else
		handle_object_size_mismatch(data, ptr);
}

__attribute__((no_sanitize_undefined)) static bool location_is_valid(struct source_location *loc)
{
	return loc->file_name != NULL;
}

__attribute__((no_sanitize_undefined)) void __ubsan_handle_nonnull_return(
  struct nonnull_return_data *data)
{
	ubsan_prologue(&data->location);
	printk("null pointer returned from function declared to never return null\n");
	if(location_is_valid(&data->attr_location))
		print_source_location("returns_nonnull attribute specified in", &data->attr_location);
	ubsan_epilogue();
}

__attribute__((no_sanitize_undefined)) static bool type_is_signed(struct type_descriptor *type)
{
	return type->type_info & 1;
}

__attribute__((no_sanitize_undefined)) static unsigned type_bit_width(struct type_descriptor *type)
{
	return 1 << (type->type_info >> 1);
}

__attribute__((no_sanitize_undefined)) static bool is_inline_int(struct type_descriptor *type)
{
	unsigned inline_bits = sizeof(unsigned long) * 8;
	unsigned bits = type_bit_width(type);

	return bits <= inline_bits;
}

__attribute__((no_sanitize_undefined)) static intmax_t get_signed_val(struct type_descriptor *type,
  unsigned long val)
{
	if(is_inline_int(type)) {
		unsigned extra_bits = sizeof(intmax_t) * 8 - type_bit_width(type);
		return ((intmax_t)val) << extra_bits >> extra_bits;
	}

	if(type_bit_width(type) == 64)
		return *(int64_t *)val;

	return *(intmax_t *)val;
}

__attribute__((no_sanitize_undefined)) static uintmax_t get_unsigned_val(
  struct type_descriptor *type,
  unsigned long val)
{
	if(is_inline_int(type))
		return val;

	if(type_bit_width(type) == 64)
		return *(uint64_t *)val;

	return *(uintmax_t *)val;
}

__attribute__((no_sanitize_undefined)) static bool val_is_negative(struct type_descriptor *type,
  unsigned long val)
{
	return type_is_signed(type) && get_signed_val(type, val) < 0;
}

__attribute__((no_sanitize_undefined)) void __ubsan_handle_out_of_bounds(
  struct out_of_bounds_data *data,
  unsigned long index)
{
	ubsan_prologue(&data->location);

	printk("index %ld is out of range for type %s\n", index, data->array_type->type_name);
	ubsan_epilogue();
}

__attribute__((no_sanitize_undefined)) void __ubsan_handle_shift_out_of_bounds(
  struct shift_out_of_bounds_data *data,
  unsigned long lhs,
  unsigned long rhs)
{
	struct type_descriptor *rhs_type = data->rhs_type;
	struct type_descriptor *lhs_type = data->lhs_type;

	ubsan_prologue(&data->location);

	if(val_is_negative(rhs_type, rhs))
		printk("shift exponent %ld is negative\n", rhs);

	else if(get_unsigned_val(rhs_type, rhs) >= type_bit_width(lhs_type))
		printk("shift exponent %ld is too large for %u-bit type %s\n",
		  rhs,
		  type_bit_width(lhs_type),
		  lhs_type->type_name);
	else if(val_is_negative(lhs_type, lhs))
		printk("left shift of negative value %ld\n", lhs);
	else
		printk("left shift of %ld by %ld places cannot be"
		       " represented in type %s\n",
		  lhs,
		  rhs,
		  lhs_type->type_name);

	ubsan_epilogue();
}

__attribute__((no_sanitize_undefined)) void __ubsan_handle_load_invalid_value(
  struct invalid_value_data *data,
  unsigned long val)
{
	ubsan_prologue(&data->location);
	printk("load of value %lx is not a valid value for type %s\n", val, data->type->type_name);
	ubsan_epilogue();
}

__attribute__((no_sanitize_undefined)) void __ubsan_handle_vla_bound_not_positive(
  struct vla_bound_data *data,
  unsigned long bound)
{
	ubsan_prologue(&data->location);
	printk("variable length array bound value %ld <= 0\n", bound);
	ubsan_epilogue();
}

__attribute__((no_sanitize_undefined)) void __ubsan_handle_nonnull_arg(
  struct non_null_arg_data *data)
{
	ubsan_prologue(&data->location);
	printk("non-null arg detected (arg %d)", data->arg_idx);
	print_source_location("attribute location", &data->attr_loc);
	ubsan_epilogue();
}

__attribute__((no_sanitize_undefined)) void __ubsan_handle_pointer_overflow(
  struct pointer_overflow_data *data,
  unsigned long base,
  unsigned long result)
{
	ubsan_prologue(&data->location);
	printk("pointer overflow detected (base=%lx, result=%lx)", base, result);
	ubsan_epilogue();
}

__attribute__((no_sanitize_undefined)) void __ubsan_handle_type_mismatch_v1(
  struct type_mismatch_data_v1 *data,
  unsigned long ptr)
{
	struct type_mismatch_data tmdata = {
		.location = data->location,
		.type = data->type,
		.alignment = 1ul << data->log_alignment,
		.type_check_kind = data->type_check_kind,
	};
	__ubsan_handle_type_mismatch(&tmdata, ptr);
}

__attribute__((no_sanitize_undefined)) void __ubsan_handle_invalid_builtin(
  struct invalid_builtin_data *data)
{
	ubsan_prologue(&data->location);
	printk("invalid builtin detected (kind=%d)", data->kind);
	ubsan_epilogue();
}

#endif
