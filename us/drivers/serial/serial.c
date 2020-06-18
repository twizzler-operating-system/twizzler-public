#include <pthread.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <twz/bstream.h>
#include <twz/debug.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/thread.h>

static inline uint8_t inb(uint16_t port)
{
	uint8_t ret;
	asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}

static inline void outb(uint16_t port, uint8_t val)
{
	asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

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

static void uart_write(struct uart *u, int reg, uint8_t value)
{
	outb(u->port + reg, value);
}

static uint8_t uart_read(struct uart *u, int reg)
{
	return inb(u->port + reg);
}
static struct uart com1 = {
	.port = COM1_PORT,
	.irq = COM1_IRQ,
};

void serial_putc(char c)
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

char serial_ready()
{
	return (uart_read(&com1, UART_REG_LSR) & UART_LSR_RX_READY) != 0;
}

#include <twz/driver/device.h>

struct termios def_term = {
	.c_iflag = BRKINT | ICRNL,
	.c_oflag = ONLCR | OPOST,
	.c_lflag = ICANON | ECHO | ISIG | ECHOE,
	.c_cc[VEOF] = 4,
	.c_cc[VEOL] = 0,
	.c_cc[VERASE] = '\b',
	.c_cc[VINTR] = 3,
	.c_cc[VMIN] = 1,
	.c_cc[VSUSP] = 26,
};

static twzobj ks_obj;
static twzobj p_obj;

static struct device_repr *dr;

void setup_pty(int cw, int ch)
{
	struct winsize w = { .ws_row = ch, .ws_col = cw };
	int r;
	if((r = twzio_ioctl(&p_obj, TCSETS, &def_term)) < 0) {
		fprintf(stderr, "failed to set pty termios: %d\n", r);
	}
	if((r = twzio_ioctl(&p_obj, TIOCSWINSZ, &w)) < 0) {
		fprintf(stderr, "failed to set pty winsize: %d\n", r);
	}
	// struct pty_hdr *hdr = twz_object_base(&ptyobj);
	// termios = &hdr->termios;
}

ssize_t get_input(char *buf, size_t len)
{
	size_t c = 0;
	while(c < len) {
		long x = atomic_exchange(&dr->syncs[0], 0);
		if(!x) {
			if(c) {
				return c;
			}
			struct sys_thread_sync_args args = {
				.op = THREAD_SYNC_SLEEP,
				.arg = 0,
				.addr = (uint64_t *)&dr->syncs[0],
			};

			int r = sys_thread_sync(1, &args, NULL);
			if(r < 0) {
				return r;
			}
		} else {
			// if(x == '\r')
			//	x = '\n';
			buf[c++] = x;
			// serial_putc(x);
			// if(x == '\n')
			//	serial_putc('\r');
		}
	}
	return c;
}

void *somain(void *a)
{
	(void)a;
	snprintf(twz_thread_repr_base()->hdr.name, KSO_NAME_MAXLEN, "[instance] serial.output");

	if(sys_thrd_ctl(THRD_CTL_SET_IOPL, 3)) {
		fprintf(stderr, "failed to set IOPL to 3\n");
		abort();
	}
	int r;
	if((r = twz_thread_ready(NULL, THRD_SYNC_READY, 0))) {
		fprintf(stderr, "failed to mark ready");
		abort();
	}

	for(;;) {
		char buf[128];
		ssize_t r = twzio_read(&p_obj, buf, 128, 0, 0);
		if(r < 0) {
			fprintf(stderr, "ERR: %ld\n", r);
			twz_thread_exit(r);
		}
		for(size_t i = 0; i < (size_t)r; i++) {
			// if(buf[i] == '\r')
			//	serial_putc('\n');
			// if(buf[i] == '\n')
			//	serial_putc('\r');
			serial_putc(buf[i]);
		}
	}
	return NULL;
}

int main(int argc, char **argv)
{
	int r;
	if(argc < 2)
		abort();
	char *kernel_side = argv[1];
	char *primary = argv[2];

	if(!kernel_side || !primary)
		abort();

	if(twz_object_init_name(&ks_obj, kernel_side, FE_READ | FE_WRITE)) {
		fprintf(stderr, "failed to load object %s\n", kernel_side);
		return 1;
	}
	if(twz_object_init_name(&p_obj, primary, FE_READ | FE_WRITE)) {
		fprintf(stderr, "failed to load object %s\n", primary);
		return 1;
	}

	setup_pty(80, 25);
	dr = twz_object_base(&ks_obj);

	pthread_t thrd;
	pthread_create(&thrd, NULL, somain, NULL);

	if((r = twz_thread_ready(NULL, THRD_SYNC_READY, 0))) {
		fprintf(stderr, "failed to mark ready");
		abort();
	}

	for(;;) {
		char buf[128];
		ssize_t r = get_input(buf, 127);
		if(r < 0) {
			fprintf(stderr, "ERR!: %d\n", (int)r);
			return 1;
		}
		size_t count = 0;
		ssize_t w = 0;
		do {
			w = twzio_write(&p_obj, buf + count, r - count, 0, 0);
			if(w < 0)
				break;
			count += w;
		} while(count < (size_t)r);

		if(w < 0) {
			fprintf(stderr, "ERR!: %d\n", (int)w);
			return 1;
		}
	}
	return 0;
}
