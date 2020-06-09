#define _GNU_SOURCE

#include <assert.h>
#include <dirent.h>
#include <err.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <twz/_obj.h>

#include "common.h"

#define MAGIC "TWIZZLER DEVICE"
struct sb {
	char magic[16];
	objid_t nameroot;
	uint64_t hashlen;
};

struct bucket {
	objid_t id;
	uint64_t pgnum;
	uint64_t next;
	uint64_t chainpage;
	uint64_t datapage;
};

static void *out_base;
static size_t outmm_sz = 0x2000;
static size_t nextpg = 0x1000;
static size_t cur_chain_page = 0;
static size_t cur_chain_idx = 0;
static int mmfd = 0;
static size_t htlen;

static size_t nr_chains = 0;
static size_t nr_buckets = 0;
static size_t data_copied = 0;
static size_t nr_chain_pages = 0;

static void *alloc_page(void)
{
	size_t next = nextpg;
	nextpg += 0x1000;
	if(nextpg > outmm_sz) {
		size_t oldsz = outmm_sz;
		outmm_sz += 0x10000;
		munmap(out_base, oldsz);
		ftruncate(mmfd, outmm_sz);
		out_base = mmap(NULL, outmm_sz, PROT_READ | PROT_WRITE, MAP_SHARED, mmfd, 0);
	}
	return (char *)out_base + next;
}

static size_t get_bucket_num(objid_t id, size_t pg)
{
	return (id ^ ((objid_t)pg << (pg % 31))) % htlen;
}

static struct bucket *get_bucket(uint64_t chainpage, size_t bn)
{
	return (struct bucket *)((char *)out_base + chainpage * 0x1000 + bn * sizeof(struct bucket));
}

static uint64_t addr_to_pagenr(void *p)
{
	return ((uint64_t)p - (uint64_t)out_base) / 0x1000;
}

static size_t max_chain_len = 0;
static float avg_chain_len = 0;
static size_t nr_pages = 0;
void add_object_page(objid_t id, size_t pg, void *data, size_t len)
{
	struct bucket *last = NULL;
	struct bucket *b = get_bucket(1, get_bucket_num(id, pg));
	// printf("    lookup bucket %ld\n", get_bucket_num(id, pg));
	size_t last_chain, last_idx;
	if(!b->id)
		nr_buckets++;
	else
		nr_chains++;

	size_t cl = 0;
	while(b->id) {
		cl++;
		last = b;
		if(b->chainpage) {
			b = get_bucket(b->chainpage, b->next);
			continue;
		}

		if(cur_chain_page == 0 || cur_chain_idx >= (0x1000 / sizeof(struct bucket))) {
			cur_chain_page = addr_to_pagenr(alloc_page());
			cur_chain_idx = 0;
			nr_chain_pages++;
		}

		last = b;
		b = get_bucket(cur_chain_page, cur_chain_idx);
		// printf("      follow chain -> %ld %ld\n", cur_chain_page, cur_chain_idx);
		last_chain = cur_chain_page;
		last_idx = cur_chain_idx;
		cur_chain_idx++;
	}
	if(cl > max_chain_len)
		max_chain_len = cl;
	nr_pages++;
	if(nr_pages > 1) {
		avg_chain_len = (avg_chain_len * (nr_pages - 1) + cl) / (float)nr_pages;
	}
	if(last) {
		last->chainpage = last_chain;
		last->next = last_idx;
	}

	b->id = id;
	b->pgnum = pg;
	b->chainpage = 0;
	b->next = 0;
	if(data) {
		void *datapage = alloc_page();
		assert(len <= 0x1000);
		memcpy(datapage, data, len);
		memset((char *)datapage + len, 0, 0x1000 - len);
		data_copied += len;

		b->datapage = addr_to_pagenr(datapage);
	} else {
		b->datapage = 0;
	}
	// printf("    added object page %p " IDFMT " :: %lx :: %ld\n", b, IDPR(id), pg, b->datapage);
}

struct ustar_header {
	char name[100];
	char mode[8];
	char uid[8];
	char gid[8];
	char size[12];
	char mtime[12];
	char checksum[8];
	char typeflag[1];
	char linkname[100];
	char magic[6];
	char version[2];
	char uname[32];
	char gname[32];
	char devmajor[8];
	char devminor[8];
	char prefix[155];
	char pad[12];
};

static uint64_t octal_to_int(char *in)
{
	uint64_t res = 0;
	for(; *in; in++) {
		res <<= 3;
		res += *in - '0';
	}
	return res;
}

static void copy_data(objid_t id, FILE *file, size_t len, int meta)
{
	size_t pg = 1;
	if(meta) {
		pg = (OBJ_MAXSIZE - len) / 0x1000;
	}
	size_t nrpages = ((len + (0x1000 - 1)) & ~(0x1000 - 1)) / 0x1000;

	for(size_t i = pg; i < pg + nrpages; i++) {
		// printf("  copying page %ld (%ld bytes this page)\n", i, len > 0x1000 ? 0x1000 : len);
		char buffer[0x1000];
		size_t rl = 0;
		ssize_t rc = 0;
		do {
			rl += rc;
		} while((rc = fread(buffer + rl, 1, (len > 0x1000 ? 0x1000 : len) - rl, file)) > 0);
		//	size_t rl = fread(buffer, 1, 0x1000, file);
		add_object_page(id, i, buffer, rl);
		len -= rl;
	}
}

static void process_object(const char *filename, objid_t id)
{
	FILE *file = fopen(filename, "r");
	if(!file)
		err(1, "fopen");

	struct ustar_header hdr;
	_Static_assert(sizeof(hdr) == 512, "");
	fread(&hdr, sizeof(hdr), 1, file);

	if(strcmp("ustar", hdr.magic)) {
		errx(1, "invalid object");
	}

	if(strcmp("data", hdr.name)) {
		errx(1, "invalid object");
	}

	// printf("%s :: %s %s\n", filename, hdr.name, hdr.magic);
	size_t sz = octal_to_int(hdr.size);
	// printf("  datasz = %ld\n", sz);

	copy_data(id, file, sz, 0);

	fseek(file, ((512 + sz) + 511) & ~511, SEEK_SET);
	fread(&hdr, sizeof(hdr), 1, file);

	if(strcmp("ustar", hdr.magic)) {
		errx(1, "invalid object");
	}

	if(strcmp("meta", hdr.name)) {
		errx(1, "invalid object");
	}

	sz = octal_to_int(hdr.size);
	copy_data(id, file, sz, 1);
	// printf("  metasz = %ld\n", sz);

	fclose(file);
	add_object_page(id, 0, NULL, 0);
}

int main(int argc, char **argv)
{
	int c;
	char *outf = NULL;
	char *nameroot_str = NULL;
	while((c = getopt(argc, argv, "o:n:")) != EOF) {
		switch(c) {
			case 'o':
				outf = optarg;
				break;
			case 'n':
				nameroot_str = optarg;
				break;
			default:
				errx(1, "invalid option: %c", c);
		}
	}

	objid_t nameroot;
	if(!nameroot_str)
		errx(1, "provide nameroot ID (-n <id>)");

	nameroot = str_to_objid(nameroot_str);

	if(optind >= argc) {
		errx(1, "provide directory to use");
	}

	DIR *d = opendir(argv[optind]);
	if(!d) {
		err(1, "opendir");
	}

	if(!outf)
		errx(1, "provide '-o outfile'");

	mmfd = open(outf, O_RDWR | O_CREAT | O_TRUNC, 0644);
	if(mmfd == -1)
		err(1, "open");

	struct dirent *ent;
	size_t totalsz = 0;
	while((ent = readdir(d))) {
		objid_t id;
		if(str_to_objid_try(ent->d_name, &id)) {
			continue;
		}
		//	printf(":: %s :: %lx:%lx\n", ent->d_name, (uint64_t)(id >> 64), (uint64_t)id);
		char *filename;
		asprintf(&filename, "%s/%s", argv[optind], ent->d_name);
		struct stat st;
		if(stat(filename, &st)) {
			err(1, "stat");
		}
		totalsz += st.st_size;
		free(filename);
	}

	htlen = (totalsz / 0x1000) * 2;
	// printf(
	//"estimated htsz = %ld entries (%ld KB)\n", htlen, (htlen * sizeof(struct bucket)) / 1024);

	outmm_sz =
	  0x1000 /* sb */ + (((htlen * sizeof(struct bucket)) + (0x1000 - 1)) & ~(0x1000 - 1)) + 0x1000;
	nextpg = outmm_sz - 0x1000;
	ftruncate(mmfd, outmm_sz);
	out_base = mmap(NULL, outmm_sz, PROT_READ | PROT_WRITE, MAP_SHARED, mmfd, 0);

	rewinddir(d);
	while((ent = readdir(d))) {
		objid_t id;
		if(str_to_objid_try(ent->d_name, &id)) {
			continue;
		}
		//	printf(":: %s :: %lx:%lx\n", ent->d_name, (uint64_t)(id >> 64), (uint64_t)id);
		char *filename;
		asprintf(&filename, "%s/%s", argv[optind], ent->d_name);
		process_object(filename, id);
		free(filename);
	}

	struct sb *sb = out_base;
	strcpy(sb->magic, MAGIC);
	sb->nameroot = nameroot;
	sb->hashlen = htlen;
	msync(out_base, outmm_sz, MS_SYNC);

	printf("IMAGE CREATED. %ld KB data (%ld pages), %ld KB total (%f %%). %ld buckets (out of %ld "
	       "allocated), %ld chains. "
	       "Max chain len = %ld, avg chain len = %f. num chain pages = %ld (%ld KB)\n",
	  data_copied / 1024,
	  nr_pages,
	  outmm_sz / 1024,
	  100.f * (float)data_copied / (float)outmm_sz,
	  nr_buckets,
	  htlen,
	  nr_chains,
	  max_chain_len,
	  avg_chain_len,
	  nr_chain_pages,
	  nr_chain_pages / 4);
	return 0;
}
