#ifndef mem_h_INCLUDED
#define mem_h_INCLUDED

/* possible buffer formats */
enum mem_format { PAGE_ARRAY, BLOCK_ARRAY, POINTER };

struct page_array {
	struct page **p;
	size_t ps;
};

struct block_array {
	void **b;
	size_t bs, block_size;
};

struct pointer {
	void *p;
	size_t ps;
};

union buffer {
	struct block_array block_array;
	struct pointer pointer;
	struct page_array page_array;
};

/* buffer api */
typedef int (*buffer_init)(union buffer *buffer, void *data, size_t size);
typedef void (*buffer_free)(union buffer *buffer);

struct mem_api {
	enum mem_format format;
	char *name;
	buffer_init init;
	buffer_free free;
};

int mem_choose(char *name, struct mem_api *mem_api);

#endif // mem_h_INCLUDED

