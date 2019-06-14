init_srcs=$(addprefix us/init/,init.c)

init_objs=$(addprefix $(BUILDDIR)/,$(init_srcs:.c=.o))
init_deps=$(addprefix $(BUILDDIR)/,$(init_srcs:.c=.d))

init_all: $(BUILDDIR)/us/init/init

$(BUILDDIR)/us/init/init: $(init_objs) $(US_LIBDEPS)
	@echo "[CLD] $@"
	@$(TWZCC) $(US_LDFLAGS) $(US_CFLAGS) -o $@ -nostdlib $(US_PRELINK) $< $(US_POSTLINK) -MD

-include $(init_deps)

.PHONY: init_all
