#pragma once

struct arch_secctx {
	uintptr_t ept_root;
};

struct secctx;
void x86_64_secctx_switch(struct secctx *s);
