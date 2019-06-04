__asm__(".text \n"
        ".global " START " \n" START ": \n"
        "	xor %rbp,%rbp \n"
        "   mov %rsi,%fs:0 \n"
        "   mov %rdx,%fs:8 \n"
        /*"	mov %rsp,%rdi \n"*/
        ".weak _DYNAMIC \n"
        ".hidden _DYNAMIC \n"
        "	lea _DYNAMIC(%rip),%rsi \n"
        "	andq $-16,%rsp \n"
        "	call " START "_c \n");
