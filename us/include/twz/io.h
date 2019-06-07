#pragma once

struct twzio_hdr {
	ssize_t (*read)(struct object *, void *, size_t len, unsigned flags);
	ssize_t (*write)(struct object *, const void *, size_t len, unsigned flags);
};
