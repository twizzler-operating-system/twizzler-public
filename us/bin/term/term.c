#include <ctype.h>
#include <libgen.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <twz/bstream.h>
#include <twz/debug.h>
#include <twz/driver/device.h>
#include <twz/driver/misc.h>
#include <twz/driver/pcie.h>
#include <twz/io.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/pty.h>
#include <twz/thread.h>
#include <unistd.h>

#include "ssfn.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define ES_NORM 0
#define ES_ESC 1
#define ES_CSI 2
#define ES_DONE 3

struct fb {
	twzobj obj;
	twzobj font;
	size_t gl_w, gl_h;
	ssfn_glyph_t *glyph_cache[256];
	ssfn_glyph_t *bold_glyph_cache[256];
	unsigned char *back_buffer, *front_buffer, *background_buffer;
	unsigned char *char_buffer;
	size_t x, y, max_x, max_y;
	size_t fbw, fbh, pitch, bpp;
	int spacing;
	ssfn_t ctx;
	ssfn_t bold_ctx;
	int init, flip;

	unsigned esc_args[16];
	int esc_argc;
	int esc_state;
	/* attrs */
	bool bold;
	uint32_t fg, bg;
};

struct fb fb = {};

void fb_putc(struct fb *fb, int c);

/* values are rgb */
static uint32_t color_map[8] = {
	[0] = 0, /* black */
	[1] = 0x00e74c3c,
	/* red */ //#e74c3c
	[2] = 0x002ecc71,
	/* green */ //#2ecc71
	[3] = 0x00f1c40f,
	/* yellow */ //#f1c40f
	[4] = 0x002980b9,
	/* blue */ //#2980b9
	[5] = 0x009b59b6,
	/* magenta */ //#9b59b6
	[6] = 0x001abc9c,
	/* cyan */ //#1abc9c
	[7] = 0x00bdc3c7,
	/* white */ //#ecf0f1
};

static twzobj ptyobj, kbobj;
static struct termios *termios;

void process_keyboard(twzobj *, char *, size_t);

void *kbmain(void *a)
{
	(void)a;
	kso_set_name(NULL, "[instance] term.input");

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
		ssize_t r = twzio_read(&kbobj, buf, 128, 0, 0);
		if(r < 0) {
			fprintf(stderr, "ERR: %ld\n", r);
			twz_thread_exit(r);
		}
		process_keyboard(&ptyobj, buf, r);
	}
}

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

#define OPTIMIZE __attribute__((target("sse", "sse2", "avx"), optimize("Ofast")))

OPTIMIZE
static inline uint32_t clamp(uint32_t x, uint32_t c)
{
	return x > c ? c : x;
}

OPTIMIZE
static inline uint32_t ARGB_TO_BGR(uint32_t x)
{
	float aa = (((x >> 24) & 0xff) + 1) / (float)0x100;
	float ar = (((x >> 16) & 0xff) + 1) / (float)0x100;
	float ag = (((x >> 8) & 0xff) + 1) / (float)0x100;
	float ab = ((x & 0xff) + 1) / (float)0x100;

	float rr = ar * aa;
	float rb = ab * aa;
	float rg = ag * aa;

	uint32_t r = 0xffu << 24 | clamp((uint32_t)(rr * 0x100), 0xff) << 16
	             | clamp((uint32_t)(rg * 0x100), 0xff) << 8 | clamp((uint32_t)(rb * 0x100), 0xff);

	return r;
}
OPTIMIZE
static inline uint32_t ablend(uint32_t a, uint32_t b)
{
	float aa = (((a >> 24) & 0xff) + 1) / (float)0x100;
	float ar = (((a >> 16) & 0xff) + 1) / (float)0x100;
	float ag = (((a >> 8) & 0xff) + 1) / (float)0x100;
	float ab = ((a & 0xff) + 1) / (float)0x100;

	float ba = (((b >> 24) & 0xff) + 1) / (float)0x100;
	float br = (((b >> 16) & 0xff) + 1) / (float)0x100;
	float bg = (((b >> 8) & 0xff) + 1) / (float)0x100;
	float bb = ((b & 0xff) + 1) / (float)0x100;

	float rr = (ar * aa + br * ba * (1 - aa)) / (aa + ba * (1 - aa));
	float rg = (ag * aa + bg * ba * (1 - aa)) / (aa + ba * (1 - aa));
	float rb = (ab * aa + bb * ba * (1 - aa)) / (aa + ba * (1 - aa));

	float ra = aa + (ba * (1 - aa));

	uint32_t r = clamp((uint32_t)(ra * 0x100), 0xff) << 24
	             | clamp((uint32_t)(rr * 0x100), 0xff) << 16
	             | clamp((uint32_t)(rg * 0x100), 0xff) << 8 | clamp((uint32_t)(rb * 0x100), 0xff);
	return r;
}

static __inline__ unsigned long long rdtsc(void)
{
	unsigned hi, lo;
	__asm__ __volatile__("rdtscp" : "=a"(lo), "=d"(hi));
	return ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
}

#include <assert.h>
OPTIMIZE
static void draw_glyph(const struct fb *restrict fb,
  ssfn_glyph_t *restrict glyph,
  uint32_t fgcolor,
  int c)
{
	unsigned int x, y, i, m;
	int32_t pen_x = fb->x * (fb->gl_w + fb->spacing);
	int32_t pen_y = fb->y * fb->gl_h;
	/* align glyph properly, we may have received a vertical letter */

	if(glyph->adv_y)
		pen_x -= (int8_t)glyph->baseline;
	else
		pen_y -= ((int8_t)glyph->baseline - (fb->gl_h / 2 + 2));
	if(pen_y < 0)
		pen_y = 0;

	uint32_t max_gy = fb->max_y * fb->gl_h - pen_y;
	if(max_gy > (fb->y + 1) * fb->gl_h - pen_y)
		max_gy = (fb->y + 1) * fb->gl_h - pen_y;
	uint32_t *buffer = (void *)fb->back_buffer;

	uint32_t bpmul = fb->pitch / 4;
	uint32_t set = 0xff | fgcolor;
	// uint64_t start = rdtsc();

	switch(glyph->mode) {
		case SSFN_MODE_BITMAP:
			for(y = 0; y < glyph->h && y < max_gy; y++) {
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
			for(y = 0; y < glyph->h && y < max_gy; y++) {
				uint32_t ys = (pen_y + y) * bpmul;
				uint32_t ygs = y * glyph->pitch;
				for(x = 0; x < glyph->w; x++) {
					assert(ys + pen_x + x < (fb->pitch * fb->fbh / 4));
					buffer[ys + pen_x + x] =
					  ablend(((uint32_t)glyph->data[ygs + x] << 24) | fgcolor,
					    fb->bg ? (fb->bg | 0xff000000) : fb->bg);
				}
			}
			break;

		case SSFN_MODE_CMAP:
			for(y = 0; y < glyph->h && y < max_gy; y++) {
				uint32_t ys = (pen_y + y) * bpmul;
				uint32_t ygs = y * glyph->pitch;
				for(x = 0; x < glyph->w; x++) {
					buffer[ys + pen_x + x] =
					  SSFN_CMAP_TO_ARGB(glyph->data[ygs + x], glyph->cmap, fgcolor);
				}
			}
			break;
		default:
			fprintf(stderr, "Unsupported mode for %x: %d\n", c, glyph->mode);
	}
	// uint64_t end = rdtsc();
	// fprintf(stderr, "TSC: %ld\n", end - start);
}
void fb_render_cursor(struct fb *fb)
{
	uint32_t *px = (uint32_t *)fb->front_buffer;
	uint32_t *pxb = (uint32_t *)fb->back_buffer;
	size_t pen_x = fb->x * (fb->gl_w + fb->spacing);
	size_t yinc = fb->pitch / 4;
	size_t pen_y = fb->y * (fb->gl_h) * yinc;
	for(size_t y = pen_y; y < (pen_y + (fb->gl_h) * yinc); y += yinc) {
		for(size_t x = 0; x < fb->gl_w; x++) {
			px[y + pen_x + x + 1] = ~pxb[y + pen_x + x + 1] | 0xff000000;
		}
	}
}

OPTIMIZE
static void fb_flip(struct fb *fb)
{
	// uint64_t s = rdtsc();
	if(fb->flip) {
		uint32_t *src1 = (uint32_t *)fb->back_buffer;
		uint32_t *src2 = (uint32_t *)fb->background_buffer;
		uint32_t *dest = (uint32_t *)fb->front_buffer;
		for(size_t i = 0; i < fb->fbh * fb->pitch / 4; i++) {
			dest[i] = ARGB_TO_BGR(ablend(src1[i], src2[i]));
			// fb->front_buffer[i] = ARGB_TO_BGR(fb->back_buffer[i]);
		}
		// fastMemcpy(fb->front_buffer, fb->back_buffer, fb->fbh * fb->pitch);
		// memcpy(fb->front_buffer, fb->back_buffer, fb->fbh * fb->pitch);
		// fb->front_buffer[i] = 0; // fb->back_buffer[i];
		// x += fb->back_buffer[i];
	}
	fb_render_cursor(fb);

	// uint64_t e = rdtsc();
	// fprintf(stderr, "flip: %ld\n", e - s);
}

#if 0
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
#endif

void fb_scroll(struct fb *fb, int nlines)
{
	// memmove(&fb->char_buffer[0], &fb->char_buffer[fb->cw], fb->cw * (fb->ch - 1));
	// memset(&fb->char_buffer[fb->cw * (fb->ch - 1)], 0, fb->cw);
	memmove(fb->back_buffer, fb->back_buffer + fb->pitch * nlines, fb->pitch * (fb->fbh - nlines));
	memset(fb->back_buffer + fb->pitch * (fb->fbh - nlines), 0, fb->pitch * nlines);
	fb->flip = 1;
}

void fb_clear_cell(struct fb *fb, int ax, int ay, uint32_t bg)
{
	uint32_t *px = (uint32_t *)fb->back_buffer;
	size_t pen_x = ax * (fb->gl_w + fb->spacing);
	size_t yinc = fb->pitch / 4;
	size_t pen_y = ay * (fb->gl_h) * yinc;
	for(size_t y = pen_y; y < (pen_y + fb->gl_h * yinc); y += yinc) {
		for(size_t x = 0; x < fb->gl_w + fb->spacing; x++) {
			px[y + pen_x + x] = bg;
		}
	}
}

void fb_clear_line(struct fb *fb, int line, int start, int end)
{
	uint32_t *px = (uint32_t *)fb->back_buffer;
	size_t pen_x = start * (fb->gl_w + fb->spacing);
	size_t yinc = fb->pitch / 4;
	size_t pen_y = line * (fb->gl_h) * yinc;
	unsigned int wid = (end - start) * (fb->gl_w + fb->spacing);
	for(size_t y = pen_y; y < (pen_y + fb->gl_h * yinc); y += yinc) {
		for(size_t x = 0; x < wid; x++) {
			px[y + pen_x + x] = 0;
		}
	}
}

#include <string.h>
void fb_del_chars(struct fb *fb, int count)
{
	if(fb->x + count > fb->max_x)
		count = fb->max_x - fb->x;
	uint32_t *px = (uint32_t *)fb->back_buffer;
	size_t pen_x = fb->x * (fb->gl_w + fb->spacing);
	size_t pen2_x = (fb->x + count) * (fb->gl_w + fb->spacing);
	size_t yinc = fb->pitch / 4;
	size_t pen_y = fb->y * (fb->gl_h) * yinc;
	for(size_t y = pen_y; y < (pen_y + fb->gl_h * yinc); y += yinc) {
		memmove(&px[y + pen_x], &px[y + pen2_x], fb->pitch - (fb->fbw - pen2_x) * fb->bpp);
	}
	fb_clear_line(fb, fb->y, fb->max_x - count, fb->max_x);
}

void fb_ins_chars(struct fb *fb, int count)
{
	if(fb->x + count > fb->max_x)
		count = fb->max_x - fb->x;
	uint32_t *px = (uint32_t *)fb->back_buffer;
	size_t pen2_x = fb->x * (fb->gl_w + fb->spacing);
	size_t pen_x = (fb->x + count) * (fb->gl_w + fb->spacing);
	size_t yinc = fb->pitch / 4;
	size_t pen_y = fb->y * (fb->gl_h) * yinc;
	for(size_t y = pen_y; y < (pen_y + fb->gl_h * yinc); y += yinc) {
		memmove(&px[y + pen_x], &px[y + pen2_x], fb->pitch - (fb->fbw - pen2_x) * fb->bpp);
	}
	for(int i = 0; i < count; i++) {
		fb_clear_cell(fb, fb->x + i, fb->y, fb->bg);
	}
}

void fb_clear_screen(struct fb *fb, int sl, int el)
{
	for(int i = sl; i < el; i++) {
		fb_clear_line(fb, i, 0, fb->max_x);
	}
}

void fb_render(struct fb *fb, int c)
{
	if(c < 8)
		return;
	fb->flip = 1;
	switch(c) {
		ssfn_glyph_t *glyph;
		case '\n':
			fb->y++;
			if(fb->y >= fb->max_y) {
				fb_scroll(fb, fb->gl_h);
				fb->y--;
			}

			if(!(termios->c_oflag & ONLRET))
				break;
			/* fall-through */
		case '\r':
			fb->x = 0;
			break;
		case '\t':
			fb->x = (fb->x + 8) & ~7;
			if(fb->x >= fb->max_x) {
				fb->x = fb->max_x - 1;
			}
			break;
		case '\b':
			if(fb->x > 0) {
				fb->x--;
				/* TODO: check if we should erase or not */
				if((termios->c_lflag & ICANON) && (termios->c_lflag & ECHOE)) {
					fb_clear_cell(fb, fb->x, fb->y, 0);
				}
			}
			break;
		default:
			if(!isprint(c))
				break;
			glyph = fb->bold ? fb->bold_glyph_cache[c] : fb->glyph_cache[c];
			if(!glyph) {
				glyph = ssfn_render(fb->bold ? &fb->bold_ctx : &fb->ctx, c);
				if(fb->bold)
					fb->bold_glyph_cache[c] = glyph;
				else
					fb->glyph_cache[c] = glyph;
			}

			if(!glyph) {
				if(ssfn_lasterr(&fb->ctx)) {
					fprintf(stderr, "render %x: %d\n", c, ssfn_lasterr(&fb->ctx));
				}
				return;
			}

			if(fb->x >= fb->max_x) {
				fb->x = 0;
				fb->y++;
			}

			if(fb->y >= fb->max_y) {
				fb_scroll(fb, fb->gl_h);
				fb->y--;
			}

			/* display the bitmap on your screen */
			fb_clear_cell(fb, fb->x, fb->y, fb->bg);
			draw_glyph(fb, glyph, fb->fg, c);
			fb->x++;
			fb->flip = 1;
			break;
	}
}

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

void setup_pty(int cw, int ch)
{
	struct winsize w = { .ws_row = ch, .ws_col = cw };
	int r;
	if((r = twzio_ioctl(&ptyobj, TCSETS, &def_term)) < 0) {
		fprintf(stderr, "failed to set pty termios: %d\n", r);
	}
	if((r = twzio_ioctl(&ptyobj, TIOCSWINSZ, &w)) < 0) {
		fprintf(stderr, "failed to set pty winsize: %d\n", r);
	}
	struct pty_hdr *hdr = twz_object_base(&ptyobj);
	termios = &hdr->termios;
}

void init_fb(struct fb *fb)
{
	if(fb->init == 1) {
#if 0
		struct pcie_function_header *hdr = twz_device_getds(&fb->obj);
		fb->front_buffer = twz_object_lea(&fb->obj, (void *)hdr->bars[0]);
		volatile struct bga_regs *regs = twz_object_lea(&fb->obj, (void *)hdr->bars[2] + 0x500);
		regs->index = 0xb0c5;
		regs->enable = 0;
		regs->xres = 1024;
		regs->yres = 768;
		regs->bpp = 0x20;
		regs->enable = 1 | 0x40;
#endif

		struct misc_framebuffer *mfb = twz_device_getds(&fb->obj);
		fb->front_buffer = twz_object_lea(&fb->obj, (void *)mfb->offset);

		fb->fbw = mfb->width;
		fb->fbh = mfb->height;
		fb->bpp = mfb->bpp / 8;
		fb->pitch = mfb->pitch;
		/* TODO: check mfb type */
#if 0
		fb->fbw = 1024;
		fb->fbh = 768;
		fb->bpp = 4;
		fb->pitch = fb->fbw * fb->bpp;
#endif
		fb->gl_w = 8;
		fb->gl_h = 16;
		fb->x = 0;
		fb->y = 0;
		fb->spacing = 1;
		fb->fg = 0x00ffffff;
		fb->bg = 0;
		fb->max_x = fb->fbw / (fb->gl_w + fb->spacing);
		fb->max_y = fb->fbh / fb->gl_h;
		for(int i = 0; i < 256; i++) {
			fb->glyph_cache[i] = NULL;
			fb->bold_glyph_cache[i] = NULL;
		}

		fb->back_buffer = malloc(fb->pitch * fb->fbh);
		fb->background_buffer = malloc(fb->pitch * fb->fbh);

		memset(fb->back_buffer, 0, fb->pitch * fb->fbh);
		fb->char_buffer = calloc(sizeof(fb->char_buffer[0]), fb->max_x * fb->max_y);

		twzobj font;
		int r;
		if((r = twz_object_init_name(&font, "/usr/share/inconsolata.sfn", FE_READ))) {
			printf("ERR opening font: %d\n", r);
			fb->init = 0;
			return;
		}
		memset(&fb->ctx, 0, sizeof(fb->ctx));
		memset(&fb->bold_ctx, 0, sizeof(fb->ctx));
		ssfn_font_t *_font_start = twz_object_base(&font);

		if((r = ssfn_load(&fb->ctx, _font_start))) {
			fprintf(stderr, "load:%d\n", r);
			fb->init = 0;
			return;
		}
		if((r = ssfn_load(&fb->bold_ctx, _font_start))) {
			fprintf(stderr, "load:%d\n", r);
			fb->init = 0;
			return;
		}

		if((r = ssfn_select(&fb->ctx,
		      SSFN_FAMILY_ANY,
		      NULL, /* family */
		      SSFN_STYLE_REGULAR,
		      fb->gl_h,       /* style and size */
		      SSFN_MODE_ALPHA /* rendering mode */
		      ))) {
			fprintf(stderr, "select: %d\n", r);
			fb->init = 0;
			return;
		}

		if((r = ssfn_select(&fb->bold_ctx,
		      SSFN_FAMILY_ANY,
		      NULL, /* family */
		      SSFN_STYLE_BOLD,
		      fb->gl_h,       /* style and size */
		      SSFN_MODE_ALPHA /* rendering mode */
		      ))) {
			fprintf(stderr, "select: %d\n", r);
			fb->init = 0;
			return;
		}

		fb->init = 2;
		setup_pty(fb->max_x, fb->max_y);

		int x, y, n;
		unsigned char *data = stbi_load("/usr/share/mountains.jpeg", &x, &y, &n, 4);
		if(data) {
			n = 4;
			for(int i = 0; i < y * 2; i++) {
				for(int j = 0; j < x * 2; j++) {
					int dpos = (i * fb->pitch) + j * fb->bpp;
					int spos = ((i / 2) * x * n) + (j / 2) * n;
					((char *)fb->background_buffer)[dpos + 0] = data[spos + 2];
					((char *)fb->background_buffer)[dpos + 1] = data[spos + 1];
					((char *)fb->background_buffer)[dpos + 2] = data[spos + 0];
					((char *)fb->background_buffer)[dpos + 3] = 0x55;
				}
			}
		}
	}
}

void process_esc(struct fb *fb, int c)
{
	(void)fb;
	(void)c;
	fprintf(stderr, " -- unhandled ESC %c\n", c);
}

void sendescstr(twzobj *out_obj, char *str);
#include <ctype.h>
void process_csi(struct fb *fb, int c)
{
	if(isdigit(c)) {
		fb->esc_args[fb->esc_argc] *= 10;
		fb->esc_args[fb->esc_argc] += c - '0';
	} else if(c == ';') {
		fb->esc_argc++;
	} else {
		fb->esc_argc++;
		// fprintf(stderr, "CSI seq %c (%d %d)\n", c, fb->esc_args[0], fb->esc_args[1]);
		switch(c) {
			case 'l':
			case 'h':
			case 'c':
				//			sendescstr(&ptyobj, "\e[?6c");
				break;
			case 'P':
				fb_del_chars(fb, fb->esc_args[0]);
				break;
			case '@':
				fb_ins_chars(fb, fb->esc_args[0]);
				break;
			case 'r':
				fprintf(stderr, "CSI r: %d %d\n", fb->esc_args[0], fb->esc_args[1]);
				break;
			case 'C':
				if(fb->esc_args[0] == 0)
					fb->esc_args[0]++;
				fb->x += fb->esc_args[0];
				if(fb->x > fb->max_x)
					fb->x = fb->max_x;
				break;
			case 'K':
				switch(fb->esc_args[0]) {
					case 0:
						fb_clear_line(fb, fb->y, fb->x, fb->max_x);
						break;
					case 1:
						fb_clear_line(fb, fb->y, 0, fb->x + 1);
						break;
					case 2:
						fb_clear_line(fb, fb->y, 0, fb->max_x);
						break;
				}
				break;
			case 'J':
				/* TODO: does this clear the entire line the cursor is on, or no? */
				switch(fb->esc_args[0]) {
					case 0:
						fb_clear_screen(fb, fb->y, fb->max_y);
						break;
					case 1:
						fb_clear_screen(fb, 0, fb->y + 1);
						break;
					case 2:
					case 3:
						fb_clear_screen(fb, 0, fb->max_y);
						break;
					default:
						fprintf(stderr, "bad arg to CSI J: %d\n", fb->esc_args[0]);
				}
				break;
			case 'H':
				/* args are 1-based, but an ommitted arg is a zero, so just switch everything to
				 * zero-based, which is better anyway */
				if(fb->esc_args[0])
					fb->esc_args[0]--;
				if(fb->esc_args[1])
					fb->esc_args[1]--;

				if(fb->esc_args[0] > fb->max_y)
					fb->esc_args[0] = fb->max_y;
				if(fb->esc_args[1] > fb->max_x)
					fb->esc_args[1] = fb->max_x;

				fb->x = fb->esc_args[1];
				fb->y = fb->esc_args[0];
				// fprintf(stderr, " got H CSI: %d %d\n", fb->x, fb->y);

				break;
			case 'm':
				for(int i = 0; i < fb->esc_argc; i++) {
					int a = fb->esc_args[i];
					if(a >= 30 && a <= 37) {
						fb->fg = color_map[a - 30];
					} else if(a >= 40 && a <= 47) {
						fb->bg = color_map[a - 30];
					} else {
						switch(a) {
							uint32_t tmp;
							case 0:
								/* reset */
								fb->fg = 0x00ffffff;
								fb->bg = 0;
								fb->bold = false;
								break;
							case 1:
								fb->bold = true;
								break;
							case 7:
								tmp = fb->bg;
								fb->bg = fb->fg;
								fb->fg = tmp;
								break;
						}
					}
				}
				break;
			default:
				fprintf(stderr,
				  " -- unhandled CSI %c (%d %d %d)\n",
				  c,
				  fb->esc_args[0],
				  fb->esc_args[1],
				  fb->esc_args[2]);
		}
		fb->esc_state = ES_DONE;
		fb->flip = 1;
	}
}

void process_incoming(struct fb *fb, int c)
{
	switch(fb->esc_state) {
		case ES_NORM:
			if(c == 27)
				fb->esc_state = ES_ESC;
			break;
		case ES_ESC:
			fb->esc_state = ES_DONE;
			if(c == '[') {
				memset(fb->esc_args, 0, sizeof(fb->esc_args));
				fb->esc_argc = 0;
				fb->esc_state = ES_CSI;
			} else
				process_esc(fb, c);
			break;
		case ES_CSI:
			if(c == '?') {
				return;
			}
			process_csi(fb, c);
			break;
	}

	if(fb->esc_state == ES_NORM) {
		// if(c != '\n' && c != '\r')
		//	fb->char_buffer[fb->y * fb->max_x + fb->x] = c;
		fb_render(fb, c);
	} else if(fb->esc_state == ES_DONE) {
		fb->esc_state = ES_NORM;
	}
}

void fb_putc(struct fb *fb, int c)
{
	if(fb->init == 0)
		return;
	init_fb(fb);
	fb->flip = 0;
	if(c > 0xff || !c) {
		return;
	}

	process_incoming(fb, c);
}

void curfb_putc(int c)
{
	fb_putc(&fb, c);
}

int main(int argc, char **argv)
{
	int r;

	char *input = NULL, *output = NULL, *pty = NULL;

	int c;
	while((c = getopt(argc, argv, "i:o:p:")) != -1) {
		switch(c) {
			case 'i':
				input = optarg;
				break;
			case 'o':
				output = optarg;
				break;
			case 'p':
				pty = optarg;
				break;
			default:
				fprintf(stderr, "unrecognized option: %c\n", c);
				exit(1);
		}
	}

	if(!input || !output || !pty) {
		fprintf(stderr, "term: usage: term -i <input> -o <output> -p <pty>\n");
		exit(1);
	}

	if(twz_object_init_name(&kbobj, input, FE_READ | FE_WRITE)) {
		fprintf(stderr, "failed to load object %s\n", input);
		return 1;
	}
	if(twz_object_init_name(&ptyobj, pty, FE_READ | FE_WRITE)) {
		fprintf(stderr, "failed to load object %s\n", pty);
		return 1;
	}

	fb.init = 1;
	if((r = twz_object_init_name(&fb.obj, output, FE_READ | FE_WRITE))) {
		fprintf(stderr, "term: failed to open framebuffer: %d\n", r);
		fb.init = 0;
	}

	fb_putc(&fb, 0);

	pthread_t thrd;
	pthread_create(&thrd, NULL, kbmain, NULL);

#if 0
	struct thread kthr;
	if((r = twz_thread_spawn(
	      &kthr, &(struct thrd_spawn_args){ .start_func = kbmain, .arg = NULL }))) {
		fprintf(stderr, "failed to spawn kb thread");
		abort();
	}

	if(twz_thread_wait(1, (struct thread *[]){ &kthr }, (int[]){ THRD_SYNC_READY }, NULL, NULL)
	   < 0) {
		fprintf(stderr, "failed to wait for thread\n");
		abort();
	}
#endif
	if((r = twz_thread_ready(NULL, THRD_SYNC_READY, 0))) {
		fprintf(stderr, "failed to mark ready");
		abort();
	}

	for(;;) {
		char buf[128];
		memset(buf, 0, sizeof(buf));
		ssize_t r = twzio_read(&ptyobj, buf, 127, 0, 0);
		if(r > 0) {
			for(ssize_t i = 0; i < r; i++) {
				if(fb.init) {
					if(buf[i] == '\n')
						fb_putc(&fb, '\r');
					fb_putc(&fb, buf[i]);
				}
			}
			fb_flip(&fb);
		}
	}
}
