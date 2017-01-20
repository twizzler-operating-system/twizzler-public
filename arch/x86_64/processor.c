#include <processor.h>

void arch_processor_enumerate()
{
	processor_register(true, 0);
}

void arch_processor_boot(struct processor *proc)
{

}

