#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <twz/_objid.h>
objid_t str_to_objid(char *s)
{
	if(!s)
		return 0;
	objid_t res = 0;
	char *o = s;
	for(; *s; s++) {
		if(*s == ':')
			continue;
		if(*s == 'x')
			continue;
		if(*s == '0' && *(s + 1) == 'x')
			continue;
		res <<= 4;
		if(*s >= '0' && *s <= '9')
			res += *s - '0';
		else if(*s >= 'a' && *s <= 'f')
			res += *s - 'a' + 10;
		else if(*s >= 'A' && *s <= 'F')
			res += *s - 'A' + 10;
		else {
			fprintf(stderr, "invalid ID string: %s (%c)\n", o, *s);
			exit(1);
		}
	}
	return res;
}
