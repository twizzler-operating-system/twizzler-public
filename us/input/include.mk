input_srcs=$(addprefix us/input/,input.c)

input_objs=$(addprefix $(BUILDDIR)/,$(input_srcs:.c=.o))
input_deps=$(addprefix $(BUILDDIR)/,$(input_srcs:.c=.d))

input_all: $(BUILDDIR)/us/input/input

$(BUILDDIR)/us/input/input: $(input_objs) $(US_LIBDEPS)
	@echo "[CLD] $@"
	$(TWZCC) $(US_LDFLAGS) $(US_CFLAGS) -o $@ -nostdlib $(US_PRELINK) $(input_objs) $(US_POSTLINK) -MD
-include $(input_deps)

.PHONY: input_all
