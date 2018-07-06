TWIX_SRC=$(addprefix us/twix/,syscall.c)
TWIX_OBJ=$(addprefix $(BUILDDIR)/,$(TWIX_SRC:.c=.o))

$(BUILDDIR)/us/twix/libtwix.a: $(TWIX_OBJ)
	ar rcs $(BUILDDIR)/us/twix/libtwix.a $(TWIX_OBJ)

$(BUILDDIR)/us/twix/%.o: us/twix/%.c $(MUSL_READY)
	@mkdir -p $(BUILDDIR)/us/twix
	$(TOOLCHAIN_PREFIX)gcc -Og -g -Wall -Wextra -std=gnu11 -I us/include -ffreestanding $(MUSL_INCL) -c -o $@ $< -MD

-include $(TWIX_OBJ:.o=.d)

