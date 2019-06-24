test_srcs=$(addprefix us/test/,test.c)

test_objs=$(addprefix $(BUILDDIR)/,$(test_srcs:.c=.o))
test_deps=$(addprefix $(BUILDDIR)/,$(test_srcs:.c=.d))

test_all: $(BUILDDIR)/us/test/test

$(BUILDDIR)/us/test/test: $(test_objs) $(US_LIBDEPS)
	@echo "[CLD] $@"
	$(TWZCC) $(US_LDFLAGS) $(US_CFLAGS) -o $@ -nostdlib $(US_PRELINK) $< $(US_POSTLINK) -MD

-include $(test_deps)

.PHONY: test_all
