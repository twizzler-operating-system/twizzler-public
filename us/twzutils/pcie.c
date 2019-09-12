#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <twz/debug.h>
#include <twz/sys.h>
#include <twz/thread.h>

#include "pcie.h"

//#define printf debug_printf

struct object pcie_cs_obj;
objid_t pcie_cs_oid;
static struct object pids;

static struct pcie_function *pcie_list = NULL;

static bool pcief_capability_get(struct pcie_function *pf, int id, union pcie_capability_ptr *cap)
{
	if(pf->config->device.cap_ptr) {
		size_t offset = pf->config->device.cap_ptr;
		do {
			cap->header = (struct pcie_capability_header *)((char *)pf->config + offset);

			if(cap->header->capid == id)
				return true;
			offset = cap->header->next;
		} while(offset != 0);
	}
	/* TODO: pcie extended caps? */
	return false;
}

void pcie_print_function(struct pcie_function *pf, bool nums)
{
	static bool _init = false;
	if(!_init && !nums) {
		if(twz_object_open_name(&pids, "pcieids", FE_READ)) {
			fprintf(stderr, "failed to open pcieids\n");
			return;
		}
		_init = true;
	}

	char *vname = NULL, *dname = NULL;
	uint16_t vendor = pf->config->header.vendor_id;
	uint16_t device = pf->config->header.device_id;
	uint16_t class = pf->config->header.class_code;
	uint16_t subclass = pf->config->header.subclass;
	uint16_t progif = pf->config->header.progif;
	const char *str = twz_obj_base(&pids);
	const char *line;
	for(line = str; line && *line; line = strchr(line, '\n')) {
		while(*line == '\n')
			line++;
		if(*line == '#')
			continue;

		if(*line != '\t') {
			char *endptr = NULL;
			if(strtol((char *)line, &endptr, 16) == vendor) {
				char *next = strchr(line, '\n');
				while(*endptr == ' ')
					endptr++;
				vname = strndup(endptr, next - endptr);

				line = next + 1;
				for(; line && *line; line = strchr(line, '\n')) {
					while(*line == '\n')
						line++;
					if(*line == '#')
						continue;
					if(*line != '\t')
						break;
					if(strtol((char *)line + 1, &endptr, 16) == device) {
						char *next = strchr(line, '\n');
						while(*endptr == ' ')
							endptr++;
						dname = strndup(endptr, next - endptr);
					}
				}
				break;
			}
		}
	}

	char *cname = NULL, *sname = NULL, *pname = NULL;

	char *cstart = strstr(str, "\nC ");

	for(line = cstart ? cstart + 1 : NULL; line && *line; line = strchr(line, '\n')) {
		while(*line == '\n')
			line++;
		if(*line == '#')
			continue;

		if(*line == 'C') {
			char *endptr = NULL;
			if(strtol((char *)line + 2, &endptr, 16) == class) {
				char *next = strchr(line, '\n');
				while(*endptr == ' ')
					endptr++;
				cname = strndup(endptr, next - endptr);

				line = next + 1;
				for(; line && *line; line = strchr(line, '\n')) {
					while(*line == '\n')
						line++;
					if(*line == '#')
						continue;
					if(*line != '\t')
						break;
					if(*(line + 1) == '\t')
						continue;
					if(strtol((char *)line + 1, &endptr, 16) == subclass) {
						char *next = strchr(line, '\n');
						while(*endptr == ' ')
							endptr++;
						sname = strndup(endptr, next - endptr);

						line = next + 1;

						for(; line && *line; line = strchr(line, '\n')) {
							while(*line == '\n')
								line++;
							if(*line == '#')
								continue;
							if(*line != '\t')
								break;
							if(*(line + 1) != '\t')
								break;
							if(strtol((char *)line + 2, &endptr, 16) == progif) {
								char *next = strchr(line, '\n');
								while(*endptr == ' ')
									endptr++;
								pname = strndup(endptr, next - endptr);

								line = next + 1;
							}
						}
					}
				}
				break;
			}
		}
	}

	// printf("%x %x %x : %x %x\n", class, subclass, progif, vendor, device);
	printf("[pcie]  %s ", sname ? sname : cname);
	if(pname)
		printf("[%s] ", pname);
	printf(":: %s %s\n", vname, dname);
}

int pcie_init_function(struct pcie_function *pf)
{
	uint64_t wc = 0;
	if(pf->config->header.vendor_id == 0x1234 && pf->config->header.device_id == 0x1111) {
		/* TODO: generalize */
		wc = 1;
	}
	struct sys_kaction_args args = {
		.id = pcie_cs_oid,
		.cmd = KACTION_CMD_PCIE_INIT_DEVICE,
		.arg = pf->segment << 16 | pf->bus << 8 | pf->device << 3 | pf->function | (wc << 32),
		.flags = KACTION_VALID,
	};
	int r;
	if((r = sys_kaction(1, &args)) < 0) {
		fprintf(stderr, "kaction: %d\n", r);
		return r;
	}
	if(args.result) {
		fprintf(stderr, "kaction-result: %d\n", args.result);
		return r;
	}

	struct pcie_bus_header *hdr = twz_obj_base(&pcie_cs_obj);
	uint32_t fid = pf->bus << 8 | pf->device << 3 | pf->function;
	if((pf->cid = hdr->functions[fid])) {
		twz_object_open(&pf->cobj, pf->cid, FE_READ | FE_WRITE);
		struct pcie_function_header *fh = twz_obj_base(&pf->cobj);
		fh->deviceid = pf->config->header.device_id;
		fh->vendorid = pf->config->header.vendor_id;
		fh->classid = pf->config->header.class_code;
		fh->subclassid = pf->config->header.subclass;
		fh->progif = pf->config->header.progif;
	}
	return pf->cid ? 0 : -1;
}

static struct pcie_function *pcie_register_device(struct pcie_bus_header *space,
  volatile struct pcie_config_space *config,
  unsigned int bus,
  unsigned int device,
  unsigned int function)
{
	struct pcie_function *pf = malloc(sizeof(*pf));
	pf->config = config;
	pf->segment = space->segnr;
	pf->bus = bus;
	pf->device = device;
	pf->function = function;
	return pf;
}

static void pcie_init_space(struct pcie_bus_header *space)
{
	if(space->magic != PCIE_BUS_HEADER_MAGIC) {
		printf("[pcie]: invalid PCIe bus header\n");
		return;
	}
	printf("[pcie] initializing PCIe configuration space covering %.4d:%.2x-%.2x\n",
	  space->segnr,
	  space->start_bus,
	  space->end_bus);

	/* brute-force scan. We _could_ detect areas to look in based on bridges and stuff, but this
	 * doesn't take much longer and is still fast. */
	for(unsigned bus = space->start_bus; bus < space->end_bus; bus++) {
		for(unsigned device = 0; device < 32; device++) {
			uintptr_t addr = ((bus - space->start_bus) << 20 | device << 15);
			volatile struct pcie_config_space *config =
			  (void *)twz_ptr_lea(&pcie_cs_obj, (char *)space->spaces + addr);
			/* if a device isn't plugged in, the lines go high */
			if(config->header.vendor_id == 0xffff) {
				continue;
			}
			if(config->header.header_type & HEADER_MULTIFUNC) {
				/* for a multi-function device, brute-force scan all functions. We check for
				 * this
				 * before brute-force scanning all functions to reduce time by a factor of 8 */
				for(unsigned function = 0; function < 8; function++) {
					addr = ((bus - space->start_bus) << 20 | device << 15 | function << 12);
					config = (void *)twz_ptr_lea(&pcie_cs_obj, (char *)space->spaces + addr);
					if(config->header.vendor_id != 0xffff) {
						struct pcie_function *f =
						  pcie_register_device(space, config, bus, device, function);
						f->next = pcie_list;
						pcie_list = f;
					}
				}
			} else {
				/* not multi-function (function 0 is the only valid function) */
				struct pcie_function *f = pcie_register_device(space, config, bus, device, 0);
				f->next = pcie_list;
				pcie_list = f;
			}
		}
	}
}

#define VBE_DISPI_INDEX_ID (0)
#define VBE_DISPI_INDEX_XRES (1)
#define VBE_DISPI_INDEX_YRES (2)
#define VBE_DISPI_INDEX_BPP (3)
#define VBE_DISPI_INDEX_ENABLE (4)
#define VBE_DISPI_INDEX_BANK (5)
#define VBE_DISPI_INDEX_VIRT_WIDTH (6)
#define VBE_DISPI_INDEX_VIRT_HEIGHT (7)
#define VBE_DISPI_INDEX_X_OFFSET (8)
#define VBE_DISPI_INDEX_Y_OFFSET (9)

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

#include "ssfn.h"

void wpix(unsigned char *fb, int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
	long off = (y * 1024 + x) * 4;
	fb[off] = b;
	fb[off + 1] = g;
	fb[off + 2] = r;
}

size_t pitch;

#define SDL_PIXEL ((uint32_t *)(fb))[(pen_y + y) * pitch / 4 + (pen_x + x)]

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

void my_draw_glyph(void *fb, ssfn_glyph_t *glyph, int pen_x, int pen_y, uint32_t fgcolor)
{
	int x, y, i, m;
	/* align glyph properly, we may have received a vertical letter */

	if(glyph->adv_y)
		pen_x -= glyph->baseline;
	else
		pen_y -= glyph->baseline;

	switch(glyph->mode) {
		case SSFN_MODE_BITMAP:
			for(y = 0; y < glyph->h; y++) {
				for(x = 0, i = 0, m = 1; x < glyph->w; x++, m <<= 1) {
					if(m > 0x80) {
						m = 1;
						i++;
					}
#if 0
					printf(":: %d %d -> %d %d : %d ==> %x :: %x\n",
					  x,
					  y,
					  (pen_y + y) * pitch / 4,
					  pen_x + x,
					  (pen_y + y) * pitch / 4 + (pen_x + x),
					  glyph->data[y * glyph->pitch + i] & m,
					  (glyph->data[y * glyph->pitch + i] & m) ? 0xFF000000 | fgcolor : 0);
#endif
					SDL_PIXEL = (glyph->data[y * glyph->pitch + i] & m) ? 0xFF000000 | fgcolor : 0;
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
			for(y = 0; y < glyph->h; y++)
				for(x = 0; x < glyph->w; x++) {
					SDL_PIXEL = ARGB_TO_BGR(
					  SSFN_CMAP_TO_ARGB(glyph->data[y * glyph->pitch + x], glyph->cmap, fgcolor));
				}
			break;
		default:
			printf("Unsupported mode\n");
	}
}

void pcie_load_driver(struct pcie_function *pf)
{
	struct pcie_function_header *hdr = twz_obj_base(&pf->cobj);
	if(hdr->vendorid == 0x1234 && hdr->deviceid == 0x1111) {
		twz_name_assign(pf->cid, "dev:output:framebuffer");
		return;
		printf("VGA!\n");
		printf("%lx\n", hdr->bars[0]);
		volatile struct bga_regs *regs = twz_ptr_lea(&pf->cobj, (void *)hdr->bars[2] + 0x500);
		regs->index = 0xb0c5;
		regs->enable = 0;
		regs->xres = 1024;
		regs->yres = 768;
		regs->bpp = 0x20;
		regs->enable = 1 | 0x40;
		pitch = 1024 * 4;
		unsigned char *fb = twz_ptr_lea(&pf->cobj, (void *)hdr->bars[0]);

		struct object font;
		int r;
		if((r = twz_object_open_name(&font, "inconsolata.sfn", FE_READ))) {
			printf("ERR opening font: %d\n", r);
			return;
		}

		ssfn_t ctx; /* the renderer context */
		memset(&ctx, 0, sizeof(ctx));

		ssfn_glyph_t *glyph; /* the returned rasterized bitmap */

		ssfn_font_t *_font_start = twz_obj_base(&font);

		if((r = ssfn_load(&ctx, _font_start))) {
			printf("load:%d\n", r);
			return;
		}

		/* select the typeface to use */

		if((r = ssfn_select(&ctx,
		      SSFN_FAMILY_ANY,
		      NULL, /* family */
		      SSFN_STYLE_REGULAR,
		      20,            /* style and size */
		      SSFN_MODE_CMAP /* rendering mode */
		      ))) {
			printf("select: %d\n", r);
			return;
		}

		glyph = ssfn_render(&ctx, 0x41);
		if(!glyph) {
			printf("glyph\n");
			return;
		}

		for(int i = 0; i < glyph->pitch * glyph->h; i++) {
			//		printf("%x\n", glyph->data[i]);
		}

		/* display the bitmap on your screen */

		unsigned pen_x = 0, pen_y = glyph->baseline + 2;
		my_draw_glyph(fb, glyph, pen_x, pen_y, 0x00ffffff);
		pen_x += glyph->adv_x;
		pen_y += glyph->adv_y;
		my_draw_glyph(fb, glyph, pen_x, pen_y, 0x00ffffff);
		pen_y += glyph->h;
		pen_x = 0;
		my_draw_glyph(fb, glyph, pen_x, pen_y, 0x00ffffff);

		printf("DONE\n");
#if 0
		my_draw_glyph(pen_x,
		  pen_y - glyph->baseline, /* coordinates to draw to */
		  glyph->w,
		  glyph->h,
		  glyph->pitch, /* bitmap dimensions */
		  &glyph->data  /* bitmap data */
		);
#endif
	}
}

int main(int argc, char **argv)
{
	printf("[pcie] loading PCIe bus %s\n", argv[1]);

	if(!objid_parse(argv[1], strlen(argv[1]), &pcie_cs_oid)) {
		printf("invalid object ID: %s\n", argv[1]);
		exit(1);
	}

	twz_object_open(&pcie_cs_obj, pcie_cs_oid, FE_READ | FE_WRITE);

	pcie_init_space(twz_obj_base(&pcie_cs_obj));

	for(struct pcie_function *pf = pcie_list; pf; pf = pf->next) {
		pcie_print_function(pf, false);
		if(pcie_init_function(pf) == 0)
			pcie_load_driver(pf);
	}

	int r;
	if((r = twz_thread_ready(NULL, THRD_SYNC_READY, 0))) {
		printf("failed to mark ready");
		exit(1);
	}
}
