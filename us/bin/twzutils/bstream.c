#include <twz/bstream.h>
#include <twz/gate.h>

#if 0
__asm__(".section .gates, \"ax\", @progbits\n"
        ".global __twz_gate_bstream_read \n"
        ".type __twz_gate_bstream_read STT_FUNC\n"
        ".org 1*32, 0x90\n"
        "__twz_gate_bstream_read:\n"
        "movabs $bstream_read, %rax\n"
        "leaq -__twz_gate_bstream_read+1*32(%rip), %r10\n"
        "addq %r10, %rax\n"
        "jmp *%rax\n"
        ".balign 32, 0x90\n"
        ".previous");

__asm__(".section .gates, \"ax\", @progbits\n"
        ".global __twz_gate_bstream_write \n"
        ".type __twz_gate_bstream_write STT_FUNC\n"
        ".org 2*32, 0x90\n"
        "__twz_gate_bstream_write:\n"
        "movabs $bstream_write, %rax\n"
        "leaq -__twz_gate_bstream_write+2*32(%rip), %r10\n"
        "addq %r10, %rax\n"
        "jmp *%rax\n"
        ".balign 32, 0x90\n"
        ".previous");
#endif
TWZ_GATE(bstream_read, BSTREAM_GATE_READ);
TWZ_GATE(bstream_write, BSTREAM_GATE_WRITE);
TWZ_GATE(bstream_poll, BSTREAM_GATE_POLL);

int main()
{
	__builtin_unreachable();
}
