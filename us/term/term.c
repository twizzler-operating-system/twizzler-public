#include <stdlib.h>
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

int pckbd_getc(void)
{
	while(!((inb(0x64) & 1)))
		;
	return inb(0x60);
}

char serial_ready()
{
	return (uart_read(&com1, UART_REG_LSR) & UART_LSR_RX_READY) != 0;
}

int pckbd_ready(void)
{
	return ((inb(0x64) & 1));
}

#include "../pcie/ssfn.h"
struct fb {
	struct object obj;
	struct object font;
	size_t fsz;
	ssfn_glyph_t *glyph_cache[256];
	unsigned char *back_buffer, *front_buffer;
	unsigned char *char_buffer;
	size_t cw, ch, cx, cy, fbx, fby;
	size_t fbw, fbh, pitch, bpp;
	ssfn_t ctx;
	int init, flip;
};
struct fb fb = { 0 };

void fb_putc(struct fb *fb, int c);
objid_t kid, sid, siid, soid;
struct object kobj, sobj, siobj, soobj;

#include "kbd.h"

void read_keyboard(int c);
void kbmain(void *a)
{
	snprintf(twz_thread_repr_base()->hdr.name, KSO_NAME_MAXLEN, "[instance] term.input-serial");

	sys_thrd_ctl(THRD_CTL_SET_IOPL, 3);
	int r;
	if((r = twz_thread_ready(NULL, THRD_SYNC_READY, 0))) {
		debug_printf("failed to mark ready");
		abort();
	}

	// bstream_write(&sobj, twz_obj_base(&sobj), "Hello, World", 12, 0);
	for(;;) {
		int x;
		if(serial_ready()) {
			int x = serial_getc();
			switch(x) {
				case 0x4:
					//	bstream_mark_eof(&ko);
					break;
				case '\r':
					x = '\n';
					serial_putc('\r'); /* fall through */
				default:
					serial_putc(x);
			}
			bstream_write(&siobj, &x, 1, 0);
		}
		if(pckbd_ready()) {
			int c = pckbd_getc();
			if(c == 0xe1) {
				__syscall6(0, 0x1234, 0, 0, 0, 0, 0);
			}
			read_keyboard(c);
		}
	}
}

#include <driver/pcie.h>

struct __packed bga_regs {
	uint16_t index;
	uint16_t xres;
	uint16_t yres;
	uint16_t bpp;
	uint16_t enable;
	uint16_t bank;
	uint16_t vwidth;
	uint16_t vheight;
	uint16_t xoff;
	uint16_t yoff;
};
#define SDL_PIXEL ((uint32_t *)(fb->back_buffer))[(pen_y + y) * fb->pitch / 4 + (pen_x + x)]

static inline uint32_t ARGB_TO_BGR(uint32_t x)
{
	uint32_t a = (x >> 24) & 0xff;
	uint32_t r = (x >> 16) & 0xff;
	uint32_t g = (x >> 8) & 0xff;
	uint32_t b = x & 0xff;
	r *= a;
	r /= 0xff;
	g *= a;
	g /= 0xff;
	b *= a;
	b /= 0xff;
	return (r << 16 | g << 8 | b);
}

static __inline__ unsigned long long rdtsc(void)
{
	unsigned hi, lo;
	__asm__ __volatile__("rdtscp" : "=a"(lo), "=d"(hi));
	return ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
}

static void draw_glyph(const struct fb *restrict fb,
  ssfn_glyph_t *restrict glyph,
  uint32_t fgcolor,
  int c)
{
	unsigned int x, y, i, m;
	uint32_t pen_x = fb->fbx;
	uint32_t pen_y = fb->fby;
	/* align glyph properly, we may have received a vertical letter */

	if(glyph->adv_y)
		pen_x -= (int8_t)glyph->baseline;
	else
		pen_y -= (int8_t)glyph->baseline;

	uint32_t *buffer = (void *)fb->back_buffer;

	uint32_t bpmul = fb->pitch / 4;
	uint32_t set = 0xff | fgcolor;
	uint64_t start = rdtsc();

	switch(glyph->mode) {
		case SSFN_MODE_BITMAP:
			for(y = 0; y < glyph->h; y++) {
				uint32_t ys = (pen_y + y) * bpmul;
				uint32_t ygs = y * glyph->pitch;
				for(x = 0, i = 0, m = 1; x < glyph->w; x++, m <<= 1) {
					if(m > 0x80) {
						m = 1;
						i++;
					}
					buffer[ys + pen_x + x] = (glyph->data[ygs + i] & m) ? set : 0;
				}
			}
			break;

		case SSFN_MODE_ALPHA:
			for(y = 0; y < glyph->h; y++)
				for(x = 0; x < glyph->w; x++) {
					uint32_t pix = (fgcolor & 0xff);
					pix *= glyph->data[y * glyph->pitch + x];
					pix /= 0xff;
					pix |= pix << 8;
					pix |= pix << 8;
					SDL_PIXEL = pix;
					// SDL_PIXEL = (uint32_t)((glyph->data[y * glyph->pitch + x] << 24) | fgcolor);
				}
			break;

		case SSFN_MODE_CMAP:
			for(y = 0; y < glyph->h; y++) {
				uint32_t ys = (pen_y + y) * bpmul;
				uint32_t ygs = y * glyph->pitch;
				for(x = 0; x < glyph->w; x++) {
					buffer[ys + pen_x + x] =
					  ARGB_TO_BGR(SSFN_CMAP_TO_ARGB(glyph->data[ygs + x], glyph->cmap, fgcolor));
				}
			}
			break;
		default:
			debug_printf("Unsupported mode for %x: %d\n", c, glyph->mode);
	}
	uint64_t end = rdtsc();
	// debug_printf("TSC: %ld\n", end - start);
}

#include <immintrin.h>
/* ... */
void fastMemcpy(void *pvDest, void *pvSrc, size_t nBytes)
{
	const __m256i *pSrc = (const __m256i *)(pvSrc);
	__m256i *pDest = (__m256i *)(pvDest);
	int64_t nVects = nBytes / sizeof(*pSrc);
	for(; nVects > 0; nVects--, pSrc++, pDest++) {
		const __m256i loaded = _mm256_stream_load_si256(pSrc);
		_mm256_stream_si256(pDest, loaded);
	}
	_mm_sfence();
}

void fb_scroll(struct fb *fb, int nlines)
{
	// memmove(&fb->char_buffer[0], &fb->char_buffer[fb->cw], fb->cw * (fb->ch - 1));
	// memset(&fb->char_buffer[fb->cw * (fb->ch - 1)], 0, fb->cw);
	fastMemcpy(
	  fb->back_buffer, fb->back_buffer + fb->pitch * nlines, fb->pitch * (fb->fbh - nlines));
	memset(fb->back_buffer + fb->pitch * (fb->fbh - nlines), 0, fb->pitch * nlines);
	fb->flip = 1;
}

void fb_render(struct fb *fb, int c)
{
	switch(c) {
		ssfn_glyph_t *glyph;
		case '\n':
			fb->fby += fb->fsz;
			break;
		case '\r':
			fb->fbx = 0;
			break;
		default:
			glyph = fb->glyph_cache[c];
			if(!glyph) {
				glyph = fb->glyph_cache[c] = ssfn_render(&fb->ctx, c);
			}

			if(!glyph) {
				if(ssfn_lasterr(&fb->ctx)) {
					debug_printf("render %x: %d\n", c, ssfn_lasterr(&fb->ctx));
				}
				return;
			}

			if(fb->fbx + glyph->adv_x >= fb->fbw) {
				fb->fbx = 0;
				fb->fby += fb->fsz;
			}

			long lines = fb->fbh;
			lines -= (fb->fby + fb->fsz);
			if(lines < 0) {
				fb_scroll(fb, -lines);
				fb->fby += lines;
			}

			/* display the bitmap on your screen */
			draw_glyph(fb, glyph, 0x00ffffff, c);
			fb->fbx += glyph->adv_x;
			fb->flip = 1;
			break;
	}
}

void fb_putc(struct fb *fb, int c)
{
	if(fb->init == 0)
		return;
	struct pcie_function_header *hdr = twz_obj_base(&fb->obj);
	if(fb->init == 1) {
		fb->front_buffer = twz_ptr_lea(&fb->obj, (void *)hdr->bars[0]);
		volatile struct bga_regs *regs = twz_ptr_lea(&fb->obj, (void *)hdr->bars[2] + 0x500);
		regs->index = 0xb0c5;
		regs->enable = 0;
		regs->xres = 1024;
		regs->yres = 768;
		regs->bpp = 0x20;
		regs->enable = 1 | 0x40;

		fb->fbw = 1024;
		fb->fbh = 768;
		fb->bpp = 4;
		fb->pitch = fb->fbw * fb->bpp;
		fb->fsz = 16;
		fb->cw = fb->fbw / (fb->fsz / 4);
		fb->ch = fb->fbh / fb->fsz * 32;
		fb->fbx = 0;
		fb->fby = fb->fsz / 2;
		for(int i = 0; i < 256; i++)
			fb->glyph_cache[i] = NULL;

		fb->char_buffer = calloc(sizeof(fb->char_buffer[0]), fb->cw * fb->ch);
		fb->back_buffer = malloc(fb->pitch * fb->fbh);
		memset(fb->back_buffer, 0, fb->pitch * fb->fbh);

		struct object font;
		int r;
		if((r = twz_object_open_name(&font, "inconsolata.sfn", FE_READ))) {
			printf("ERR opening font: %d\n", r);
			fb->init = 0;
			return;
		}
		memset(&fb->ctx, 0, sizeof(fb->ctx));
		ssfn_font_t *_font_start = twz_obj_base(&font);

		if((r = ssfn_load(&fb->ctx, _font_start))) {
			debug_printf("load:%d\n", r);
			fb->init == 0;
			return;
		}

		if((r = ssfn_select(&fb->ctx,
		      SSFN_FAMILY_ANY,
		      NULL, /* family */
		      SSFN_STYLE_REGULAR,
		      fb->fsz,       /* style and size */
		      SSFN_MODE_CMAP /* rendering mode */
		      ))) {
			debug_printf("select: %d\n", r);
			fb->init = 0;
			return;
		}
		fb->init = 2;
	}

	fb->flip = 0;
	if(c > 0xff) {
		return;
	}

	if(c != '\n' && c != '\r')
		fb->char_buffer[fb->cw * fb->cy + fb->cx] = c;
	fb_render(fb, c);

	uint64_t s = rdtsc();
	if(fb->flip) {
		fastMemcpy(fb->front_buffer, fb->back_buffer, fb->fbh * fb->pitch);
	}
	uint64_t e = rdtsc();
	// debug_printf("flip: %ld\n", e - s);
}

void smain(void *a)
{
	debug_printf(":::: SMAIN\n");
	snprintf(twz_thread_repr_base()->hdr.name, KSO_NAME_MAXLEN, "[instance] term.framebuffer");
	int r;
	if((r = twz_thread_ready(NULL, THRD_SYNC_READY, 0))) {
		debug_printf("failed to mark ready");
		abort();
	}

	for(;;) {
		char buf[128];
		memset(buf, 0, sizeof(buf));
		ssize_t r = bstream_read(&sobj, buf, 127, 0);
		if(r > 0) {
			for(ssize_t i = 0; i < r; i++) {
				if(fb.init) {
					if(buf[i] == '\n')
						fb_putc(&fb, '\r');
					fb_putc(&fb, buf[i]);
				}
			}
		}
	}
}

struct object bs;
int main(int argc, char **argv)
{
	int r;
	if((r = twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE, 0, 0, &kid))) {
		debug_printf("failed to create keyboard object");
		abort();
	}
	if((r = twz_object_open(&kobj, kid, FE_READ | FE_WRITE))) {
		debug_printf("failed to open keyboard object");
		abort();
	}

	if((r = twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE, 0, 0, &siid))) {
		debug_printf("failed to create serial in object");
		abort();
	}
	if((r = twz_object_open(&siobj, siid, FE_READ | FE_WRITE))) {
		debug_printf("failed to open serial in object");
		abort();
	}

	if((r = twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE, 0, 0, &soid))) {
		debug_printf("failed to create serial out object");
		abort();
	}
	if((r = twz_object_open(&soobj, soid, FE_READ | FE_WRITE))) {
		debug_printf("failed to open serial out object");
		abort();
	}

	if((r = twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE, 0, 0, &sid))) {
		debug_printf("failed to create screen object");
		abort();
	}
	if((r = twz_object_open(&sobj, sid, FE_READ | FE_WRITE))) {
		debug_printf("failed to open screen object");
		abort();
	}

	if((r = bstream_obj_init(&kobj, twz_obj_base(&kobj), 8))) {
		debug_printf("failed to init keyboard bstream");
		abort();
	}

	if((r = bstream_obj_init(&sobj, twz_obj_base(&sobj), 16))) {
		debug_printf("failed to init screen bstream");
		abort();
	}

	if((r = bstream_obj_init(&soobj, twz_obj_base(&soobj), 16))) {
		debug_printf("failed to init serial out bstream");
		abort();
	}
	if((r = bstream_obj_init(&siobj, twz_obj_base(&siobj), 16))) {
		debug_printf("failed to init serial in bstream");
		abort();
	}

	if((r = twz_name_assign(kid, "dev:dfl:keyboard"))) {
		debug_printf("failed to assign keyboard object name");
		abort();
	}
	if((r = twz_name_assign(sid, "dev:dfl:screen"))) {
		debug_printf("failed to assign screen object name");
		abort();
	}

	if((r = twz_name_assign(siid, "dev:dfl:serial-input"))) {
		debug_printf("failed to assign keyboard object name");
		abort();
	}
	if((r = twz_name_assign(soid, "dev:dfl:serial-output"))) {
		debug_printf("failed to assign screen object name");
		abort();
	}

	fb.init = 1;
	if((r = twz_object_open_name(&fb.obj, "dev:framebuffer", FE_READ | FE_WRITE))) {
		debug_printf("term: failed to open framebuffer: %d\n", r);
		fb.init = 0;
	}

	struct thread kthr;
	if((r = twz_thread_spawn(
	      &kthr, &(struct thrd_spawn_args){ .start_func = kbmain, .arg = &bs }))) {
		debug_printf("failed to spawn kb thread");
		abort();
	}

	twz_thread_wait(1, (struct thread *[]){ &kthr }, (int[]){ THRD_SYNC_READY }, NULL, NULL);

	struct thread sthr;
	if((r =
	       twz_thread_spawn(&sthr, &(struct thrd_spawn_args){ .start_func = smain, .arg = &bs }))) {
		debug_printf("failed to spawn framebuffer thread");
		abort();
	}

	twz_thread_wait(1, (struct thread *[]){ &sthr }, (int[]){ THRD_SYNC_READY }, NULL, NULL);

	if((r = twz_thread_ready(NULL, THRD_SYNC_READY, 0))) {
		debug_printf("failed to mark ready");
		abort();
	}

	for(;;) {
		char buf[128];
		memset(buf, 0, sizeof(buf));
		ssize_t r = bstream_read(&soobj, buf, 127, 0);
		if(r > 0) {
			for(ssize_t i = 0; i < r; i++) {
				if(buf[i] == '\n')
					serial_putc('\r');
				serial_putc(buf[i]);
			}
		}
	}
}
