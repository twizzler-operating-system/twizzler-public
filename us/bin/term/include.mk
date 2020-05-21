
TERM_SRCS=$(addprefix us/bin/term/,term.c kbd.c)
TERM_OBJS=$(addprefix $(BUILDDIR)/,$(TERM_SRCS:.c=.o))

#TERM_LIBS=-Wl,--whole-archive -lbacktrace -Wl,--no-whole-archive
#TERM_CFLAGS=-fsanitize=undefined

$(BUILDDIR)/us/sysroot/usr/bin/term: $(TERM_OBJS) $(SYSROOT_READY) $(SYSLIBS) $(UTILS)
	@mkdir -p $(dir $@)
	@echo "[LD]      $@"
	@$(TWZCC) $(TWZLDFLAGS) -g -o $@ -MD $(TERM_OBJS) $(TERM_LIBS) 

$(BUILDDIR)/us/bin/term/%.o: us/bin/term/%.c $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo "[CC]      $@"
	@$(TWZCC) $(TWZCFLAGS) $(TERM_CFLAGS) -o $@ -c -MD $<

$(BUILDDIR)/us/sysroot/usr/share/inconsolata.sfn: us/inconsolata.sfn
	@mkdir -p $(dir $@)
	@cp $< $@

SYSROOT_FILES+=$(BUILDDIR)/us/sysroot/usr/bin/term $(BUILDDIR)/us/sysroot/usr/share/inconsolata.sfn $(BUILDDIR)/us/sysroot/usr/share/mountains.jpeg
