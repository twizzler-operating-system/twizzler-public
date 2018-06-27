#include <string.h>
#include <debug.h>

objid_t kc_init_id=0, kc_bsv_id=0;

bool objid_parse(const char *name, objid_t *id)
{
	int i;
	*id = 0;
	int shift = 128;

	for(i=0;i<33;i++) {
		char c = *(name + i);
		if(c == ':' && i == 16) {
			continue;
		}
		if(!((c >= '0' && c <= '9')
					|| (c >= 'a' && c <= 'f')
					|| (c >= 'A' && c <= 'F'))) {
			printk("Malformed object name: %s\n", name);
			break;
		}
		if(c >= 'A' && c <= 'F') {
			c += 'a' - 'A';
		}

		uint128_t this = 0;
		if(c >= 'a' && c <= 'f') {
			this = c - 'a' + 0xa;
		} else {
			this = c - '0';
		}

		shift -= 4;
		*id |= this << shift;
	}
	/* finished parsing? */
	return i == 33;
}

static void _parse_kv(char *name, char *value)
{
	printk(":: <%s> = <%s>\n", name, value);
	if(!strcmp(name, "init")) {
		objid_t id;
		if(objid_parse(value, &id)) {
			kc_init_id = id;
		} else {
			printk("Malformed init directive: %s=%s\n", name, value);
		}
	}
	if(!strcmp(name, "bsv")) {
		objid_t id;
		if(objid_parse(value, &id)) {
			kc_bsv_id = id;
		} else {
			printk("Malformed bsv directive: %s=%s\n", name, value);
		}
	}

}

static size_t readl(char *data, size_t dlen, char *buf, size_t blen)
{
	if(dlen == 0 || blen == 0) return 0;
	const char *nl = strnchr(data, '\n', dlen);
	if(!nl) nl = data + dlen;
	size_t len = nl - data;
	if(len > blen) len = blen;
	strncpy(buf, data, len);

	return len;
}

void kc_parse(char *data, size_t len)
{
	char *tmp = data;
	char buf[128];
	size_t tl;
	while((tl=readl(tmp, len, buf, sizeof(buf))) > 0) {
		if(tl == sizeof(buf)) {
			panic("Increase buffer size");
		}
		char *e = strnchr(buf, '=', 128);
		if(e) {
			*e++ = 0;
			_parse_kv(buf, e);
		}

		len -= tl;
		tmp += tl;
	}
	/* load kernel configuration *
	if(!strncmp(data, "init=", 5)) {
		objid_t id;
		if(!objid_parse(data+5, &id)) {
			printk("Cannot parse initline of kc: %s\n", data);
			break;
		}
		kc_init_id = id;
	}
	*/

}

