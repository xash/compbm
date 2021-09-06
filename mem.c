#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/vmalloc.h>
#include "mem.h"

#define SIZE(a) (sizeof(a)/sizeof(*a))

/* -------------
 * void * buffer
 * ------------- */

/* vmalloc'd buffer */
int buffer_vmalloc_init(union buffer *buffer, void *data, size_t size) {
	struct pointer *bp = &buffer->pointer;
	if (!(bp->p = vmalloc(size)))
		return 1;
	if (data)
		memcpy(bp->p, data, size);
	bp->ps = size;
	return 0;
}
void buffer_vmalloc_free(union buffer *buffer) {
	struct pointer *bp = &buffer->pointer;
	if (bp->p) vfree(bp->p);
}

/* kmalloc'd buffer */
int buffer_kmalloc_init(union buffer *buffer, void *data, size_t size) {
	struct pointer *bp = &buffer->pointer;
	if (size > KMALLOC_MAX_SIZE)
		return 1;
	if (!(bp->p = kmalloc(size, GFP_KERNEL)))
		return 1;
	if (data)
		memcpy(bp->p, data, size);
	bp->ps = size;
	return 0;
}
void buffer_kmalloc_free(union buffer *buffer) {
	struct pointer *bp = &buffer->pointer;
	if (bp->p) kfree(bp->p);
}

/* -----------------
 * page array buffer
 * ----------------- */

/* connected pages */
int buffer_cpages_init(union buffer *buffer, void *data, size_t size) {
	struct page_array *bpa = &buffer->page_array;
	void *pages;
	int i, order;
	bpa->ps = DIV_ROUND_UP(size, PAGE_SIZE);
	order = __builtin_ffs(bpa->ps) - 1;

	/* buffer to store struct page ** */
	if (!(bpa->p = kmalloc(bpa->ps * sizeof(struct page *), GFP_KERNEL)))
		return 1;
	for (i = 0; i < bpa->ps; i++)
		bpa->p[i] = 0;

	/* allocate physically connected pages */
	if (!(pages = (void*)__get_free_pages(GFP_KERNEL, order)))
		return 1;

	/* copy into pages */
	if (data)
		memcpy(pages, data, size);

	/* get struct page pointer from data pointer, store into struct page ** */
	for (i = 0; i < bpa->ps; i++)
		bpa->p[i] = virt_to_page(pages + PAGE_SIZE * i);

	return 0;
}
void buffer_cpages_free(union buffer *buffer) {
	struct page_array *bpa = &buffer->page_array;
	if (!bpa->p) return;
	__free_pages(bpa->p[0], __builtin_ffs(bpa->ps) - 1);
	kfree(bpa->p);
}

/* disconnected pages */
int buffer_dpages_init(union buffer *buffer, void *data, size_t size) {
	struct page_array *bpa = &buffer->page_array;
	int i;
	bpa->ps = DIV_ROUND_UP(size, PAGE_SIZE);

	/* buffer to store struct page ** */
	if (!(bpa->p = kmalloc(bpa->ps * sizeof(struct page *), GFP_KERNEL)))
		return 1;

	/* set to 0, so cleanup can work even if alloc fails inbetween */
	for (i = 0; i < bpa->ps; i++)
		bpa->p[i] = 0;

	/* alloc single pages */
	for (i = 0; i < bpa->ps; i++) {
		if (!(bpa->p[i] = alloc_page(GFP_KERNEL)))
			return 1;
		memcpy(page_address(bpa->p[i]), data + i * PAGE_SIZE, PAGE_SIZE);
	}
	return 0;
}
void buffer_dpages_free(union buffer *buffer) {
	struct page_array *bpa = &buffer->page_array;
	int i;
	if (!bpa->p) return;
	for (i = 0; i < bpa->ps; i++)
		if (bpa->p[i])
			__free_page(bpa->p[i]);
	kfree(bpa->p);
}

/* ------------------
 * block array buffer
 * ------------------ */

/* block array, vmalloc */
int buffer_varray_init(union buffer *buffer, void *data, size_t size, size_t block_size) {
	struct block_array *bba = &buffer->block_array;
	int i;
	bba->bs = DIV_ROUND_UP(size, block_size);
	bba->block_size = block_size;
	if (!(bba->b = kmalloc(bba->bs * sizeof(void *), GFP_KERNEL)))
		return 1;
	for (i = 0; i < bba->bs; i++)
		bba->b[i] = 0;

	for (i = 0; i < bba->bs; i++) {
		if (!(bba->b[i] = vmalloc(block_size)))
			return 1;
		if (data)
			memcpy(bba->b[i], data + i * block_size, block_size);
	}
	return 0;
}
void buffer_varray_free(union buffer *buffer) {
	struct block_array *bba = &buffer->block_array;
	int i;
	if (!bba->b) return;
	for (i = 0; i < bba->bs; i++)
		if (bba->b[i])
			vfree(bba->b[i]);
	kfree(bba->b);
}
#define buffer_varray_init_variant(block_size) \
int buffer_varray_init_ ## block_size (union buffer *buffer, void *data, size_t size) { \
	return buffer_varray_init(buffer, data, size, 1 << block_size); \
}
buffer_varray_init_variant(16)
buffer_varray_init_variant(17)
buffer_varray_init_variant(18)
buffer_varray_init_variant(19)
buffer_varray_init_variant(20)
buffer_varray_init_variant(21)
buffer_varray_init_variant(22)
buffer_varray_init_variant(23)
buffer_varray_init_variant(24)

/* block array, kmalloc */
int buffer_karray_init(union buffer *buffer, void *data, size_t size, size_t block_size) {
	struct block_array *bba = &buffer->block_array;
	int i;
	bba->bs = DIV_ROUND_UP(size, block_size);
	bba->block_size = block_size;

	if (block_size > KMALLOC_MAX_SIZE) 
		return 1;
	if (!(bba->b = kmalloc(bba->bs * sizeof(void *), GFP_KERNEL)))
		return 1;
	for (i = 0; i < bba->bs; i++)
		bba->b[i] = 0;

	for (i = 0; i < bba->bs; i++) {
		if (!(bba->b[i] = kmalloc(block_size, GFP_KERNEL)))
			return 1;
		if (data)
			memcpy(bba->b[i], data + i * block_size, (i + 1) < bba->bs ? block_size : size - block_size * (bba->bs - 1));
	}
	return 0;
}
void buffer_karray_free(union buffer *buffer) {
	struct block_array *bba = &buffer->block_array;
	int i;
	if (!bba->b) return;
	for (i = 0; i < bba->bs; i++)
		if (bba->b[i])
			kfree(bba->b[i]);
	kfree(bba->b);
}
#define buffer_karray_init_variant(block_size) \
int buffer_karray_init_ ## block_size (union buffer *buffer, void *data, size_t size) { \
	return buffer_karray_init(buffer, data, size, 1 << block_size); \
}
buffer_karray_init_variant(16)
buffer_karray_init_variant(17)
buffer_karray_init_variant(18)
buffer_karray_init_variant(19)
buffer_karray_init_variant(20)
buffer_karray_init_variant(21)
buffer_karray_init_variant(22)
buffer_karray_init_variant(23)
buffer_karray_init_variant(24)

struct mem_api mem_formats[] = {
// list_start
	{POINTER, "vmalloc", buffer_vmalloc_init, buffer_vmalloc_free},
	{POINTER, "kmalloc", buffer_kmalloc_init, buffer_kmalloc_free},
	{PAGE_ARRAY, "cpages", buffer_cpages_init, buffer_cpages_free},
	{PAGE_ARRAY, "dpages", buffer_dpages_init, buffer_dpages_free},
	{BLOCK_ARRAY, "vblocks_64K", buffer_varray_init_16, buffer_varray_free},
	{BLOCK_ARRAY, "vblocks_128K", buffer_varray_init_17, buffer_varray_free},
	{BLOCK_ARRAY, "vblocks_256K", buffer_varray_init_18, buffer_varray_free},
	{BLOCK_ARRAY, "vblocks_512K", buffer_varray_init_19, buffer_varray_free},
	{BLOCK_ARRAY, "vblocks_1M", buffer_varray_init_20, buffer_varray_free},
	{BLOCK_ARRAY, "vblocks_2M", buffer_varray_init_21, buffer_varray_free},
	{BLOCK_ARRAY, "vblocks_4M", buffer_varray_init_22, buffer_varray_free},
	{BLOCK_ARRAY, "vblocks_8M", buffer_varray_init_23, buffer_varray_free},
	{BLOCK_ARRAY, "vblocks_16M", buffer_varray_init_24, buffer_varray_free},
	{BLOCK_ARRAY, "kblocks_64K", buffer_karray_init_16, buffer_karray_free},
	{BLOCK_ARRAY, "kblocks_128K", buffer_karray_init_17, buffer_karray_free},
	{BLOCK_ARRAY, "kblocks_256K", buffer_karray_init_18, buffer_karray_free},
	{BLOCK_ARRAY, "kblocks_512K", buffer_karray_init_19, buffer_karray_free},
	{BLOCK_ARRAY, "kblocks_1M", buffer_karray_init_20, buffer_karray_free},
	{BLOCK_ARRAY, "kblocks_2M", buffer_karray_init_21, buffer_karray_free},
	{BLOCK_ARRAY, "kblocks_4M", buffer_karray_init_22, buffer_karray_free},
	{BLOCK_ARRAY, "kblocks_8M", buffer_karray_init_23, buffer_karray_free},
	{BLOCK_ARRAY, "kblocks_16M", buffer_karray_init_24, buffer_karray_free},
// list_end
};

int mem_choose(char *name, struct mem_api *mem_api) {
	int i;
	for (i = 0; i < SIZE(mem_formats); i++)
		if (!strcmp(name, mem_formats[i].name)) {
			*mem_api = mem_formats[i];
			break;
		}
	return i == SIZE(mem_formats);
}
