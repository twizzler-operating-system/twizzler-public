TWIX_SRC=$(addprefix us/twix/,syscall.c)
TWIX_OBJ=$(addprefix $(BUILDDIR)/,$(TWIX_SRC:.c=.o))

$(BUILDDIR)/us/twix/libtwix.a: $(TWIX_OBJ)
	@echo "[AR]  $@"
	@ar rcs $(BUILDDIR)/us/twix/libtwix.a $(TWIX_OBJ)

$(BUILDDIR)/us/twix/%.o: us/twix/%.c $(MUSL_HDRS)
	@echo "[CC]  $@"
	@mkdir -p $(BUILDDIR)/us/twix
	@$(TWZCC) $(TWZCFLAGS) -c -o $@ $< -MD

-include $(TWIX_OBJ:.o=.d)

