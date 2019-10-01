#include <arch/x86_64-io.h>
#include <debug.h>
#include <device.h>
#include <instrument.h>
#include <interrupt.h>
#include <machine/isa.h>
#include <processor.h>
#define COM1_PORT 0x3f8 /* COM1 */
#define COM1_IRQ 0x24

#define UART_REG_DATA 0
#define UART_REG_IER 1
#define UART_REG_IID 2
#define UART_REG_FIFOCTL 2 /* same as above */
#define UART_REG_LCR 3
#define UART_REG_MCR 4
#define UART_REG_LSR 5
#define UART_REG_MSR 6
#define UART_REG_SCRATCH 7

#define UART_REG_DIV_LSB 0 /* with DLAB set to 1 */
#define UART_REG_DIV_MSB 1

#define UART_IER_RX_FULL (1 << 0)
#define UART_IER_TX_FULL (1 << 1)
#define UART_IER_LINESTATUS (1 << 2)
#define UART_IER_LINEDELTA (1 << 3)

#define UART_IID_FIFOEN1 (1 << 7)
#define UART_IID_FIFOEN2 (1 << 6)
#define UART_IID_IID2 (1 << 3)
#define UART_IID_IID1 (1 << 2)
#define UART_IID_IID0 (1 << 1)
#define UART_IID_PENDING (1 << 0)

#define UART_FCR_FIFO64 (1 << 5)
#define UART_FCR_DMASEL (1 << 3)
#define UART_FCR_TX_RESET (1 << 2)
#define UART_FCR_RX_RESET (1 << 1)
#define UART_FCR_ENABLE (1 << 0)
/* bits 6,7 are RX trigger */

#define UART_LCR_DLAB (1 << 7)
#define UART_LCR_SBR (1 << 6)
#define UART_LCR_STICKY_PAR (1 << 5)
#define UART_LCR_EVEN_PAR (1 << 4)
#define UART_LCR_ENABLE_PAR (1 << 3)
#define UART_LCR_STOP_BIT (1 << 2)
/* bits 0,1 are word length */

#define UART_MCR_LOOP_EANBLE (1 << 4)
#define UART_MCR_OUT2 (1 << 3)
#define UART_MCR_OUT1 (1 << 2)
#define UART_MCR_RTS (1 << 1)
#define UART_MCR_DTR (1 << 0)

#define UART_LSR_FIFO_ERR (1 << 7)
#define UART_LSR_TX_EMPTY (1 << 6)
#define UART_LSR_THR_EMPTY (1 << 5)
#define UART_LSR_BREAK (1 << 4)
#define UART_LSR_FRAME_ERR (1 << 3)
#define UART_LSR_PARITY_ERR (1 << 2)
#define UART_LSR_OVERRUN_ERR (1 << 1)
#define UART_LSR_RX_READY (1 << 0)

#define UART_MSR_DCD (1 << 7)
#define UART_MSR_RI (1 << 6)
#define UART_MSR_DSR (1 << 5)
#define UART_MSR_CTS (1 << 4)
#define UART_MSR_DDCD (1 << 3)
#define UART_MSR_TERI (1 << 2)
#define UART_MSR_DDSR (1 << 1)
#define UART_MSR_DCTS (1 << 0)

#define UART_PARITY_TYPE_NONE 0
#define UART_PARITY_TYPE_ODD 1
#define UART_PARITY_TYPE_EVEN 2
#define UART_PARITY_TYPE_MARK 3
#define UART_PARITY_TYPE_SPACE 4

struct uart {
	uint8_t lc;
	uint8_t mc;
	uint8_t ie;
	uint8_t fc;
	int divisor;
	int port;
	int max_baud;
	int fifo_sz;
	int irq;
};

__noinstrument static void uart_write(struct uart *u, int reg, uint8_t value)
{
	x86_64_outb(u->port + reg, value);
}

__noinstrument static uint8_t uart_read(struct uart *u, int reg)
{
	return x86_64_inb(u->port + reg);
}

static void uart_configure_line(struct uart *u, int parity, int stopbits, int wordlen)
{
	uint8_t lc = 0;
	switch(parity) {
		case UART_PARITY_TYPE_EVEN:
			lc |= UART_LCR_EVEN_PAR;
			break;
		case UART_PARITY_TYPE_SPACE:
			lc |= UART_LCR_EVEN_PAR;
			break;
		case UART_PARITY_TYPE_MARK:
			lc |= UART_LCR_STICKY_PAR | UART_LCR_EVEN_PAR;
			break;
	}
	if(parity != UART_PARITY_TYPE_NONE) {
		lc |= UART_LCR_ENABLE_PAR;
	}
	/* stop bits can be 1 or 2. */
	if(stopbits == 2) {
		lc |= UART_LCR_STOP_BIT;
	}
	lc |= (wordlen - 5) & 3;

	u->lc = lc;
}

static void uart_set_speed(struct uart *u, int speed)
{
	/* Unfortunately, some chips are not as good as others. We'll detect them
	 * and set a max baud to deal with buggy chips. */
	if(speed > u->max_baud) {
		speed = u->max_baud;
	}
	u->divisor = 115200 / speed;
}

static void uart_set_fifo(struct uart *u, int fifosz)
{
	u->fifo_sz = fifosz;
	u->fc = 0;
	if(u->fifo_sz > 1) {
		u->fc = UART_FCR_ENABLE | UART_FCR_TX_RESET | UART_FCR_RX_RESET;
		switch(u->fifo_sz) {
			case 4:
				u->fc |= (1 << 6);
				break;
			case 8:
				u->fc |= (1 << 7);
				break;
			default:
				u->fc |= (1 << 6) | (1 << 7);
				break;
				/* TODO: some chips support 64-byte FIFOs. We don't, yet. */
		}
	}
}

static void uart_reset(struct uart *u)
{
	/* reset by reading all the registers. It's less of a reset and more
	 * of a "make sure we're in a reasonable state". To fully reset, the UART
	 * should be fully re-programmed before use (or when updating configuration)
	 */
	uart_read(u, UART_REG_IER);
	uart_read(u, UART_REG_IID);
	uart_read(u, UART_REG_LCR);
	uart_read(u, UART_REG_MCR);
	uart_read(u, UART_REG_LSR);
	uart_read(u, UART_REG_MSR);
	uart_read(u, UART_REG_SCRATCH);
	uart_read(u, UART_REG_DATA);
}

static void uart_program(struct uart *u, bool interrupts)
{
	/* We don't know how the BIOS left the UARTs. Read all the registers to ack
	 * any pending info and interrupts */
	uart_reset(u);

	/* Take previously set register values and actually program the UART with
	 * them. First, set the speed. */
	uart_write(u, UART_REG_LCR, UART_LCR_DLAB | u->lc);
	uart_write(u, UART_REG_DIV_LSB, u->divisor & 0xFF);
	uart_write(u, UART_REG_DIV_MSB, (u->divisor >> 8) & 0xFF);

	/* Switch off DLAB and program the line config. */
	uart_write(u, UART_REG_LCR, u->lc);

	uart_write(u, UART_REG_FIFOCTL, u->fc);

	if(interrupts) {
#if CONFIG_INSTRUMENT
		u->ie = UART_IER_RX_FULL;
//			| UART_IER_LINESTATUS | UART_IER_LINEDELTA;
#else
		// u->ie = UART_IER_RX_FULL | UART_IER_TX_FULL | UART_IER_LINESTATUS | UART_IER_LINEDELTA;
		u->ie = UART_IER_RX_FULL | UART_IER_LINESTATUS | UART_IER_LINEDELTA;
#endif
	} else {
		u->ie = 0;
	}
	uart_write(u, UART_REG_IER, u->ie);

	/* The way is open. Mark DTR and RTS to send and recv data */
	u->mc = UART_MCR_DTR | UART_MCR_RTS;
	uart_write(u, UART_REG_MCR, u->mc);

	/* Ack everything again, because funky hardware */
	uart_reset(u);

	if(interrupts) {
		/* If we want interrupts, set OUT1 (often doesn't matter, but sometimes
		 * is used to enable/disable a port). Also set OUT2, which transfers
		 * interrupts for us to see. */
		u->mc |= UART_MCR_OUT1 | UART_MCR_OUT2;
		uart_write(u, UART_REG_MCR, u->mc);
	}
}

static void uart_identify(struct uart *u)
{
	/* Reset, allowing us to ID the chip */
	uart_reset(u);

	uint8_t fc = 0;
	fc |= UART_FCR_ENABLE;
	fc |= UART_FCR_TX_RESET;
	fc |= UART_FCR_RX_RESET;
	fc |= (3 << 6);
	fc |= UART_FCR_FIFO64;
	uart_write(u, UART_REG_FIFOCTL, fc);

	fc = uart_read(u, UART_REG_IID);
	int trigger = (fc >> 6) & 3;
	int max_baud, fifo_size;
	if(trigger == 3 && (fc & UART_FCR_FIFO64)) {
		/* We can write to the 64-byte setting. It's a 16750. */
		fifo_size = 64;
		max_baud = 115200;
	} else if(trigger == 3 && !(fc & UART_FCR_FIFO64)) {
		/* We cannot write to the 64-byte setting, but both trigger bits
		 * were writable. It's a 16550A. */
		fifo_size = 14;
		max_baud = 115200;
	} else if(trigger != 3) {
		/* Detected the buggy 16550. Fifo cannot be used */
		fifo_size = 1;
		max_baud = 57600;
	} else {
		/* The 16450 preserves the scratch register */
		uart_write(u, UART_REG_SCRATCH, 0xAA);
		if(uart_read(u, UART_REG_SCRATCH) == 0xAA) {
			max_baud = 38400;
		} else {
			max_baud = 19200;
		}
		fifo_size = 1;
	}

	u->max_baud = max_baud;
	uart_set_fifo(u, fifo_size);
}

static void uart_configure(struct uart *u,
  bool interrupts,
  int parity,
  int stop,
  int word,
  int speed)
{
	uart_set_speed(u, speed);
	uart_configure_line(u, parity, stop, word);
	uart_program(u, interrupts);
}

static void uart_init(struct uart *u, bool interrupts, int parity, int stop, int word, int speed)
{
	uart_identify(u);
	uart_configure(u, interrupts, parity, stop, word, speed);
}

static struct uart com1 = {
	.port = COM1_PORT,
	.irq = COM1_IRQ,
};

static struct object *ser_obj;

#include <init.h>
#include <limits.h>
#include <object.h>
#include <syscall.h>
#include <twz/driver/device.h>
__noinstrument static void _serial_interrupt(int i, struct interrupt_handler *h __unused)
{
	struct uart *u = &com1;
	(void)i;
	uint8_t id, ls;
	while(((id = uart_read(u, UART_REG_IID)) & 1) == 0) {
		switch((id >> 1) & 7) {
			unsigned char c;
			case 0:
				uart_read(u, UART_REG_MSR);
				break;
			case 1:
				break;
			case 2:
			case 6:
				c = uart_read(u, UART_REG_DATA);

				long tmp = c;
				obj_write_data_atomic64(ser_obj, offsetof(struct device_repr, syncs[0]), tmp);
				thread_wake_object(
				  ser_obj, offsetof(struct device_repr, syncs[0]) + OBJ_NULLPAGE_SIZE, INT_MAX);
				break;
			case 3:
				ls = uart_read(u, UART_REG_LSR);
				printk("[serial]: got line status interrupt: %x\n", ls);
				break;
		}
	}
}

static void __late_init_serial(void *a __unused)
{
	ser_obj = device_register(DEVICE_BT_ISA, DEVICE_ID_SERIAL);
	kso_setname(ser_obj, "UART0");
	kso_attach(pc_get_isa_bus(), ser_obj, DEVICE_ID_SERIAL);
}
POST_INIT(__late_init_serial, NULL);

static struct interrupt_handler _serial_handler = {
	.fn = _serial_interrupt,
};

void serial_init(void)
{
	uart_init(&com1,
	  false,
	  UART_PARITY_TYPE_NONE,
	  CONFIG_SERIAL_DEBUG_STOPBITS,
	  CONFIG_SERIAL_DEBUG_WORDSZ,
	  CONFIG_SERIAL_DEBUG_BAUD);
	printk("Initialized serial debugging (max_baud=%d, fifo_sz=%d, div=%d)\n",
	  com1.max_baud,
	  com1.fifo_sz,
	  com1.divisor);
}

__noinstrument void serial_putc(char c)
{
	while((uart_read(&com1, UART_REG_LSR) & UART_LSR_THR_EMPTY) == 0)
		;
	uart_write(&com1, UART_REG_DATA, c);
}

char serial_getc()
{
	while((uart_read(&com1, UART_REG_LSR) & UART_LSR_RX_READY) == 0)
		;
	return uart_read(&com1, UART_REG_DATA);
}

__initializer static void __serial_init(void)
{
	interrupt_register_handler(com1.irq, &_serial_handler);
	arch_interrupt_unmask(com1.irq);
	uart_init(&com1,
	  true,
	  UART_PARITY_TYPE_NONE,
	  CONFIG_SERIAL_DEBUG_STOPBITS,
	  CONFIG_SERIAL_DEBUG_WORDSZ,
	  CONFIG_SERIAL_DEBUG_BAUD);
}

#include <spinlock.h>
static struct spinlock _lock = SPINLOCK_INIT;
__noinstrument void debug_puts(char *s)
{
#if CONFIG_INSTRUMENT
	instrument_disable();
#endif

	bool fl = __spinlock_acquire(&_lock, NULL, 0);
	while(*s) {
		serial_putc(*s);
		if(*s == '\n')
			serial_putc('\r');
		s++;
	}
	__spinlock_release(&_lock, fl, NULL, 0);

#if CONFIG_INSTRUMENT
	instrument_enable();
#endif
}
