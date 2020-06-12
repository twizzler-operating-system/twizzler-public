
NVME_SRCS=$(addprefix us/drivers/nvme/,nvme.cpp)
NVME_OBJS=$(addprefix $(BUILDDIR)/,$(NVME_SRCS:.cpp=.o))

#NVME_LIBS=-Wl,--whole-archive -lbacktrace -Wl,--no-whole-archive
#NVME_CFLAGS=-fsanitize=undefined

$(BUILDDIR)/us/sysroot/usr/bin/nvme: $(NVME_OBJS) $(SYSROOT_READY) $(SYSLIBS) $(UTILS)
	@mkdir -p $(dir $@)
	@echo "[LD]      $@"
	@$(TWZCXX) $(TWZLDFLAGS) -g -o $@ -MD $< $(NVME_LIBS) 

$(BUILDDIR)/us/drivers/nvme/%.o: us/drivers/nvme/%.cpp $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo "[CC]      $@"
	@$(TWZCXX) $(TWZCFLAGS) $(NVME_CFLAGS) -o $@ -c -MD $<

SYSROOT_FILES+=$(BUILDDIR)/us/sysroot/usr/bin/nvme

