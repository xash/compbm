#ifndef transform_h_INCLUDED
#define transform_h_INCLUDED

#include "mem.h"

/* transform api to transform given buffer to void* */
typedef void* (*transform_init)(union buffer *buffer);
typedef void (*transform_free)(union buffer *buffer, void *data);
struct transform_api {
	enum mem_format format;
	char *name;
	transform_init init;
	transform_free free;
};

int transform_choose(char *name, struct transform_api *transform_api);

#endif // transform_h_INCLUDED

