#include <twz/_kso.h>

#define MAX_DEVICE_SYNCS 64

#define DEVICE_INPUT 1
#define DEVICE_IO 2

#define DEVICE_ID_KEYBOARD 1
#define DEVICE_ID_SERIAL 2

struct device_repr {
	struct kso_hdr hdr;
	uint64_t device_type;
	uint64_t device_id;

	void *dshdr;

	long syncs[MAX_DEVICE_SYNCS];
};

#define DEVICE_TYPE_PCIE 0
#define DEVICE_TYPE_USB 1

#define KACTION_CMD_DEVICE_ENABLE_IOMMU 1002
