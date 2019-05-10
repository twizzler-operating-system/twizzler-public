#include <elf.h>
#include <err.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../us/include/twz/_obj.h"

typedef Elf64_Half Elf64_Hashelt;

size_t fotentry = 0;
#define DYN_CNT 32

struct elf {
	FILE *file;
	Elf64_Ehdr hdr;
	Elf64_Dyn *dynamic[DYN_CNT];
	Elf64_Dyn *needed;
	size_t needcnt;
	struct elf *libs;
	Elf64_Addr dynaddr;
	Elf64_Phdr *phdrs;
	bool has_dyn;
	char *dstrtab;
	char *dsymtab;
	char *drelo;
	char **vbase;
	size_t symsz, relsz, relcnt, phdrcnt, loadptcnt;
	unsigned fote;
};

#define GET_HDR(b, s, i) ({ (void *)((b) + (i) * (s)); })

Elf64_Phdr *get_phdr(struct elf *e, size_t i)
{
	return &e->phdrs[i];
}
Elf64_Sym *get_sym(struct elf *e, size_t i)
{
	return GET_HDR(e->dsymtab, e->symsz, i);
}
Elf64_Rela *get_relo(struct elf *e, size_t i)
{
	return GET_HDR(e->drelo, e->relsz, i);
}

void *vaddr(struct elf *e, Elf64_Addr a)
{
	for(unsigned i = 0; i < e->phdrcnt; i++) {
		if(e->vbase[i]) {
			if(a >= e->phdrs[i].p_vaddr && a < e->phdrs[i].p_vaddr + e->phdrs[i].p_memsz) {
				return e->vbase[i] + (a - e->phdrs[i].p_vaddr);
			}
		}
	}
	errx(1, "trying to translate invalid address %lx", a);
	return NULL;
}

static unsigned long elf_Hash(const unsigned char *name)
{
	unsigned long h = 0, g = 0;

	while(*name) {
		h = (h << 4) + *name++;
		if((g = (h & 0xf0000000)))
			h ^= g >> 24;
		h &= ~g;
	}
	return h;
}

Elf64_Sym *elf_get_sym(struct elf *e, unsigned i)
{
	if(!e->has_dyn || !e->dsymtab || !e->dstrtab) {
		errx(1, "cannot lookup symbol in this ELF file");
		return NULL;
	}
	unsigned long hash = 0;
	Elf64_Hashelt *table = vaddr(e, e->dynamic[DT_HASH]->d_un.d_ptr);
	Elf64_Hashelt *buckets = table + 2;
	Elf64_Hashelt *chain = buckets + table[0];
	Elf64_Sym *s = NULL;
	unsigned idx = STN_UNDEF;
	for(idx = buckets[hash % table[0]]; hash <= table[0];
	    idx = idx == STN_UNDEF ? buckets[++hash % table[0]] : chain[idx]) {
		if(idx != STN_UNDEF) {
			s = get_sym(e, idx);
			if(i-- == 0) {
				return s;
			}
		}
	}
	return NULL;
}

Elf64_Sym *elf_lookup_sym(struct elf *e, const char *name)
{
	if(!e->has_dyn || !e->dsymtab || !e->dstrtab) {
		err(1, "cannot lookup symbol in this ELF file");
		return NULL;
	}
	unsigned long hash = elf_Hash((unsigned char *)name);
	Elf64_Hashelt *table = vaddr(e, e->dynamic[DT_HASH]->d_un.d_ptr);
	Elf64_Hashelt *buckets = table + 2;
	Elf64_Hashelt *chain = buckets + table[0];
	for(unsigned idx = buckets[hash++ % table[0]]; idx != STN_UNDEF; idx = chain[idx]) {
		Elf64_Sym *s = get_sym(e, idx);
		if(s && !strcmp(e->dstrtab + s->st_name, name)) {
			if(ELF64_ST_BIND(s->st_info) == STB_GLOBAL && s->st_shndx) {
				return s;
			}
		}
	}
	return NULL;
}

void elf_open(struct elf *e, FILE *f)
{
	memset(e, 0, sizeof(*e));
	e->file = f;

	fread(&e->hdr, sizeof(e->hdr), 1, f);

	e->phdrcnt = e->hdr.e_phnum;
	char tmp[e->hdr.e_phentsize * e->phdrcnt];
	fseek(f, e->hdr.e_phoff, SEEK_SET);
	fread(tmp, 1, sizeof(tmp), f);

	e->phdrs = calloc(e->phdrcnt, sizeof(Elf64_Phdr));
	e->vbase = calloc(e->phdrcnt, sizeof(char *));
	Elf64_Addr dyn = 0;
	e->loadptcnt = 0;
	for(unsigned i = 0; i < e->phdrcnt; i++) {
		memcpy(&e->phdrs[i], tmp + i * e->hdr.e_phentsize, sizeof(Elf64_Phdr));
		Elf64_Phdr *p = get_phdr(e, i);

		if(p->p_type == PT_LOAD && p->p_memsz > 0) {
			e->loadptcnt++;
			e->vbase[i] =
			  mmap(NULL, p->p_memsz, PROT_READ | PROT_WRITE, MAP_PRIVATE, fileno(f), p->p_offset);
			if(e->vbase[i] == MAP_FAILED) {
				err(
				  1, "failed to map LOAD region (%lx %lx %d)", p->p_memsz, p->p_offset, fileno(f));
			}
		}
		if(p->p_type == PT_DYNAMIC) {
			dyn = p->p_vaddr;
		}
	}
	if(dyn != 0) {
		e->has_dyn = true;
		e->needcnt = 0;
		e->needed = NULL;
		e->dynaddr = dyn;
		for(Elf64_Dyn *d = vaddr(e, dyn); d->d_tag != DT_NULL; d++) {
			if(d->d_tag < DYN_CNT) {
				e->dynamic[d->d_tag] = d;
			}
			if(d->d_tag == DT_NEEDED) {
				e->needcnt++;
				e->needed = realloc(e->needed, e->needcnt * sizeof(Elf64_Dyn));
				memcpy(&e->needed[e->needcnt - 1], d, sizeof(*d));
			}
		}

		if(e->dynamic[DT_SYMTAB]) {
			e->dsymtab = vaddr(e, e->dynamic[DT_SYMTAB]->d_un.d_ptr);
			e->symsz = e->dynamic[DT_SYMENT]->d_un.d_val;
		}

		if(e->dynamic[DT_STRTAB]) {
			e->dstrtab = vaddr(e, e->dynamic[DT_STRTAB]->d_un.d_ptr);
		}

		if(e->dynamic[DT_RELA]) {
			e->drelo = vaddr(e, e->dynamic[DT_RELA]->d_un.d_ptr);
			e->relsz = e->dynamic[DT_RELAENT]->d_un.d_val;
			if(e->dynamic[DT_JMPREL]) {
				if(e->dynamic[DT_JMPREL]->d_un.d_ptr != e->dynamic[DT_RELA]->d_un.d_ptr) {
					errx(1, "NOTSUPP: JMPREL differs from RELA");
				}
				e->relcnt = e->dynamic[DT_PLTRELSZ]->d_un.d_val / e->relsz;
			} else {
				e->relcnt = e->dynamic[DT_RELASZ]->d_un.d_val / e->relsz;
			}
		}

		e->libs = calloc(e->needcnt, sizeof(struct elf));
		for(unsigned i = 0; i < e->needcnt; i++) {
			char *name = e->needed[i].d_un.d_val + e->dstrtab;
			printf("Loading lib: %s\n", name);
			char path[strlen(name) + 32];
			sprintf(path, "/twz/usr/lib/%s", name);
			FILE *l = fopen(path, "r+");
			elf_open(&e->libs[i], l);
		}
	}
}

static void usage()
{
	printf("usage: postelf [-h] [-s <start>] <infile>\n");
	printf("Pre-link an ELF file and generate an FOT for use in twizzler.\n");
	printf("<infile> must be a valid ELF file. Postelf will generate one file"
	       " per PT_LOAD program header for use as twizzler objects.\n");
	printf("Options:\n");
	printf("  -s <start> : First free FOT entry (default 1).\n");
}

/*
unsigned attach_dynamic_lib(struct twzobj *tw, char *name)
{
    printf("Adding %s splits to FOT\n", name);
    struct fotentry f = {
        .nresolver = 0,
        .flags = FE_NAME,
    };
    char str[256];
    sprintf(str, "%s.0", name);
    char *b = basename(str);
    if(strlen(b) > 14) {
        printf("WARNING - need to implement longer names!\n");
        exit(1);
    }
    char str_m[256];
    sprintf(str_m, "%s.0.meta", name);
    sprintf(f.data, "%s", b);
    f.flags = FE_NAME | FE_READ | FE_EXEC;
    unsigned start = fotentry;
    to_fot_set(tw, fotentry++, &f);

    FILE *dyl = fopen(str, "r");
    FILE *dyl_m = fopen(str_m, "r");
    if(!dyl) {
        sprintf(str, "/twz/usr/lib/%s.0", name);
        sprintf(str_m, "/twz/usr/lib/%s.0.meta", name);
        dyl = fopen(str, "r");
        dyl_m = fopen(str_m, "r");
        if(!dyl) {
            perror("fopen lib");
            exit(1);
        }
    }
    if(!dyl_m) {
        perror("fopen lib meta");
        exit(1);
    }
    struct twzobj *dlto = to_new(1024 * 1024 * 1024);
    to_load(dlto, dyl, 0, dyl_m);
    for(unsigned i = 0; i < dlto->fotlen; i++) {
        to_fot_set(tw, fotentry++, &dlto->fot[i]);
    }

    fclose(dyl);
    fclose(dyl_m);
    return start;
}
*/
int main(int argc, char **argv)
{
	int c;
	size_t startfot = 0;
	while((c = getopt(argc, argv, "h")) != -1) {
		switch(c) {
			case 'h':
				usage();
				exit(0);
			default:
				usage();
				exit(1);
		}
	}

	if(argc <= optind) {
		fprintf(stderr, "Provide input ELF to operate on.\n");
		exit(1);
	}
	char *filename = argv[optind];
	FILE *f = fopen(filename, "r+");
	if(!f) {
		perror("fopen");
		exit(1);
	}

	struct elf target;
	elf_open(&target, f);

	fotentry = target.loadptcnt - 1;
	if(startfot != 0) {
		if(startfot < fotentry) {
			fprintf(stderr, "start fot index must be greater than number of splits.\n");
			exit(1);
		}
		if(startfot > fotentry) {
			fotentry = startfot;
		}
	}

	if(target.dynamic[DT_JMPREL]) {
		if(vaddr(&target, target.dynamic[DT_JMPREL]->d_un.d_ptr) != target.drelo) {
			errx(1, "NOTSUPP: different JMPREL than RELA");
		}
		printf("Fixing up PLT\n");
		uint64_t *pltgot = vaddr(&target, target.dynamic[DT_PLTGOT]->d_un.d_ptr);
		pltgot[1] = target.dynaddr;
		pltgot[2] = 0x3ffec0001000ul;
		for(unsigned i = 0; i < target.relcnt; i++) {
			Elf64_Rela *r = get_relo(&target, i);
			if(ELF64_R_TYPE(r->r_info) == R_X86_64_JUMP_SLOT) {
				Elf64_Sym *sym = get_sym(&target, ELF64_R_SYM(r->r_info));
				// printf("  %s\n", sym->st_name + target.dstrtab);
				// printf("  %lx\n", sym->st_value);
				uint64_t *a = vaddr(&target, r->r_offset);
				// printf("  %p (%lx): %lx\n\n", a, r->r_offset, *a);
				*a = sym->st_value;
			}
		}
	}

#if 0
	if(target.dynamic[DT_RELA]) {
		if(vaddr(&target, target.dynamic[DT_RELA]->d_un.d_ptr) != target.drelo) {
			errx(1, "NOTSUPP: different JMPREL than RELA");
		}
		printf("Fixing up RELA\n");
		for(unsigned i=0;i<target.relcnt;i++) {
			Elf64_Rela *r = get_relo(&target, i);
			if(ELF64_R_TYPE(r->r_info) == R_X86_64_GLOB_DAT) {
				Elf64_Sym *sym = get_sym(&target, ELF64_R_SYM(r->r_info));
				uint64_t *a = vaddr(&target, r->r_offset);
				*a = sym->st_value + r->r_addend;
				printf("Fixing symbol: %s: %p <- %lx + %ld\n", sym->st_name + target.dstrtab, a, sym->st_value, r->r_addend);
			}
		}
	}
#endif

	unsigned loadseg = 0;
	for(unsigned int i = 0; i < target.phdrcnt; i++) {
		Elf64_Phdr *p = get_phdr(&target, i);
		if(p->p_memsz == 0)
			continue;
		switch(p->p_type) {
			case PT_LOAD:
				if(loadseg > 1) {
					fprintf(stderr, "UNSUP: more than 2 LOAD PHDRS\n");
					exit(1);
				}
				//	printf("Creating ELFsplit %d\n", loadseg);
				char outstr[256];
				sprintf(outstr, "%s.%s", filename, loadseg == 0 ? "text" : "data");
				FILE *o = fopen(outstr, "w+");
				truncate(outstr, 0);
				if(!o) {
					perror("fopen out");
					exit(1);
				}

				// char *bname = basename(outstr);

				fwrite(target.vbase[i], 1, p->p_filesz, o);
				fclose(o);

				loadseg++;
				break;
		}
	}

#if 0
	Elf64_Sym *s = NULL;
	for(size_t i=0;target.has_dyn && (s = elf_get_sym(&target, i++));) {
		char *name = s->st_name + target.dstrtab;
		if(s->st_shndx != 0 && ELF64_ST_BIND(s->st_info) == STB_GLOBAL) {
			char data[strlen(name) + 4 + 1];
			sprintf(data, "sym:%s", name);
			to_mdvar_add(twztarget, data, (char *)&s->st_value,
					strlen(name) + 4, sizeof(s->st_value));
		}
	}

	for(int i = 0; i < exports_count; i++) {
		Elf64_Sym *s = elf_lookup_sym(&target, exports[i]);
		printf("-> %s\n", exports[i]);
		if(s) {
			char *name = s->st_name + target.dstrtab;
			printf("Exporting %s\n", name);
			char data[strlen(name) + 4 + 1];
			sprintf(data, "sym:%s", name);
			to_mdvar_add(
			  twztarget, data, (char *)&s->st_value, strlen(name) + 4, sizeof(s->st_value));
		}
	}
#endif

	/*
	if(target.needcnt) {
	    for(unsigned i = 0; i < target.needcnt; i++) {
	        char *name = target.dstrtab + target.needed[i].d_un.d_val;
	        target.libs->fote = attach_dynamic_lib(twztarget, name);
	    }
	}*/

	return 0;
}
