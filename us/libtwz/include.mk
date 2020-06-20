LIBTWZ_SRC=$(addprefix us/libtwz/,object.c fault.c thread.c view.c name.c oa.c btree.c event.c mutex.c bstream.c io.c pty.c hier.c kso.c libtwz.c queue.c)

LIBTWZ_OBJ=$(addprefix $(BUILDDIR)/,$(LIBTWZ_SRC:.c=.o))

#ifneq (,$(wildcard $(shell $(TWZCC) -print-file-name=libubsan.a)))
#LIBTWZCFLAGS=-fsanitize=undefined
#endif

$(BUILDDIR)/us/libtwz/%.o: us/libtwz/%.c $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo "[CC]      $@"
	@$(TWZCC) $(TWZCFLAGS) $(LIBTWZCFLAGS) -fno-omit-frame-pointer -g -Ius/libtwz/include -include us/libtwz/include/libtwz.h -c -o $@ -MD -fPIC $<

$(BUILDDIR)/us/libtwz/queue.o: us/libtwz/queue.c $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo "[CC]      $@"
	@$(TWZCC) $(TWZCFLAGS) $(LIBTWZCFLAGS) -O3 -fno-omit-frame-pointer -g -Ius/libtwz/include -include us/libtwz/include/libtwz.h -c -o $@ -MD -fPIC $<


#if [ -f $$($(TWZCC) -print-file-name=libubsan.a) ]; then \
	#	echo "[ARx]     libubsan.a";\
	#	(cd $(dir $@)/tmp;\
	#	ar x $$($(TWZCC) -print-file-name=libubsan.a););\
	#	echo "[AR]      $@";\
	#	ar rcs $(BUILDDIR)/us/libtwz/libtwz.a $(LIBTWZ_OBJ) $(BUILDDIR)/us/libtwz/tmp/*.o;\
	#else\
	#	echo "[AR]      $@";\
	#	ar rcs $(BUILDDIR)/us/libtwz/libtwz.a $(LIBTWZ_OBJ);\
	#fi

$(BUILDDIR)/us/libtwz/libtwz.a: $(LIBTWZ_OBJ)
	@mkdir -p $(dir $@)
	@mkdir -p $(dir $@)/tmp
	@echo "[AR]      $@"
	@-rm $(BUILDDIR)/us/libtwz/libtwz.a
	@ar rcs $(BUILDDIR)/us/libtwz/libtwz.a $(LIBTWZ_OBJ)

$(BUILDDIR)/us/libtwz/libtwz.so: $(LIBTWZ_OBJ) $(BUILDDIR)/us/twix/libtwix.a
	@mkdir -p $(dir $@)
	@echo "[LD]      $@"
	@$(TWZCC) -o $(BUILDDIR)/us/libtwz/libtwz.so -shared $(LIBTWZ_OBJ) -nostdlib

-include $(LIBTWZ_OBJ:.o=.d)
