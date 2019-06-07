LIBTWZ_SRC=$(addprefix us/libtwz/,object.c fault.c thread.c view.c name.c oa.c btree.c event.c mutex.c)

LIBTWZ_OBJ=$(addprefix $(BUILDDIR)/,$(LIBTWZ_SRC:.c=.o))

$(BUILDDIR)/us/libtwz/libtwz.a: $(LIBTWZ_OBJ)
	@mkdir -p $(dir $@)
	@echo "[AR]  $@"
	@ar rcs $(BUILDDIR)/us/libtwz/libtwz.a $(LIBTWZ_OBJ)

-include $(LIBTWZ_OBJ:.o=.d)
