#pragma once

struct arch_sctx {
	uintptr_t ept_root;
};

struct sctx;
void x86_64_secctx_switch(struct sctx *s);
