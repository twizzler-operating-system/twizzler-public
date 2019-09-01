term_srcs=$(addprefix us/term/,term.c)

term_objs=$(addprefix $(BUILDDIR)/,$(term_srcs:.c=.o))
term_deps=$(addprefix $(BUILDDIR)/,$(term_srcs:.c=.d))

term_all: $(BUILDDIR)/us/term/term

$(BUILDDIR)/us/term/term: $(term_objs) $(US_LIBDEPS)
	@echo "[CLD] $@"
	$(TWZCC) $(US_LDFLAGS) $(US_CFLAGS) -o $@ -nostdlib $(US_PRELINK) $< $(US_POSTLINK) -MD
-include $(term_deps)

.PHONY: term_all
