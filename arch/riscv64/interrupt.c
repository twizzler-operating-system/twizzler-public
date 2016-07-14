
struct exception_frame {
	uint64_t sstatus;
	uint64_t x1;
	uint64_t _unused; //skip x2
	uint64_t x3;
	uint64_t x4;
	uint64_t x5;
	uint64_t x6;
	uint64_t x7;
	uint64_t x8;
	uint64_t x9;
	uint64_t x10;
	uint64_t x11;
	uint64_t x12;
	uint64_t x13;
	uint64_t x14;
	uint64_t x15;
	uint64_t x16;
	uint64_t x17;
	uint64_t x18;
	uint64_t x19;
	uint64_t x20;
	uint64_t x21;
	uint64_t x22;
	uint64_t x23;
	uint64_t x24;
	uint64_t x25;
	uint64_t x26;
	uint64_t x27;
	uint64_t x28;
	uint64_t x29;
	uint64_t x30;
	uint64_t x31;
	uint64_t scause;
	uint64_t sepc;
	uint64_t sbadaddr;
	uint64_t sip;
};

void debug_puts(char *s);
void arch_interrupt_entry(struct exception_frame *frame, uint64_t magic)
{
	printk("Int %lx %lx %lx %lx %lx\n", frame->sstatus, frame->scause, frame->sepc, frame->sip, frame->sbadaddr);
	for(;;);
	asm(
			"li t0, 1 << 1;"
			"csrc sip, t0;"
			::: "t0");
}

