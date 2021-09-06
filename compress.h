#ifndef compress_h_INCLUDED
#define compress_h_INCLUDED

#include "mem.h"

/* de/compression api */

typedef int (*compress_cc)(union buffer *buffer, void *dest, int dest_s, void *src, int src_s, int level);
typedef int (*compress_dc)(union buffer *buffer, void *dest, int dest_s, void *src, int src_s);

struct compress_api {
	enum mem_format type;
	char *name;
	compress_cc compress;
	compress_dc decompress;
	int level;
};

int compress_init(struct compress_api api);
void compress_free(void);
int compress_choose(char *name, struct compress_api *compress_api);

#endif // compress_h_INCLUDED

