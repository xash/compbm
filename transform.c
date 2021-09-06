#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/vmalloc.h>

#include "mem.h"
#include "transform.h"

#define SIZE(a) (sizeof(a)/sizeof(*a))

/* -------------
 * void * buffer
 * ------------- */

/* dummy passing pointer along */
void * transform_pointer_dummy_init(union buffer *buffer) {
	struct pointer *bp = &buffer->pointer;
	return bp->p;
}
void transform_pointer_dummy_free(union buffer *buffer, void *data) {
	return;
}

/* vmalloc dupe */
void * transform_pointer_vmalloc_init(union buffer *buffer) {
	void *ret;
	struct pointer *bp = &buffer->pointer;
	if (!(ret = vmalloc(bp->ps)))
		return NULL;
	memcpy(ret, bp->p, bp->ps);
	return ret;
}
void transform_pointer_vmalloc_free(union buffer *buffer, void *data) {
	if (data) vfree(data);
}

/* kmalloc dupe */
void * transform_pointer_kmalloc_init(union buffer *buffer) {
	void *ret;
	struct pointer *bp = &buffer->pointer;
	if (!(ret = kmalloc(bp->ps, GFP_KERNEL)))
		return NULL;
	memcpy(ret, bp->p, bp->ps);
	return ret;
}
void transform_pointer_kmalloc_free(union buffer *buffer, void *data) {
	if (data) kfree(data);
}

/* -----------------
 * page array buffer
 * ----------------- */

/* pages vmalloc dupe */
void * transform_pages_vmalloc_init(union buffer *buffer) {
	struct page_array *bpa = &buffer->page_array;
	void *ret;
	int i;
	if (!(ret = vmalloc(bpa->ps * PAGE_SIZE)))
		return NULL;
	for (i = 0; i < bpa->ps; i++)
		memcpy(ret + i * PAGE_SIZE,
           page_address(bpa->p[i]),
           PAGE_SIZE);
	return ret;
}
void transform_pages_vmalloc_free(union buffer *buffer, void *data) {
	if (data) vfree(data);
}

/* pages kmalloc dupe */
void * transform_pages_kmalloc_init(union buffer *buffer) {
	struct page_array *bpa = &buffer->page_array;
	void *ret;
	int i;
	if (!(ret = kmalloc(bpa->ps * PAGE_SIZE, GFP_KERNEL)))
		return NULL;
	for (i = 0; i < bpa->ps; i++)
		memcpy(ret + i * PAGE_SIZE,
           page_address(bpa->p[i]),
           PAGE_SIZE);
	return ret;
}
void transform_pages_kmalloc_free(union buffer *buffer, void *data) {
	if (data) kfree(data);
}

/* vmap pages */
void * transform_pages_vmap_init(union buffer *buffer) {
	struct page_array *bpa = &buffer->page_array;
	void *ret;
	if (!(ret = vmap(bpa->p, bpa->ps, VM_MAP, PAGE_KERNEL)))
		return NULL;
	return ret;
}
void transform_pages_vmap_free(union buffer *buffer, void *data) {
	if (data) vunmap(data);
}

/* vm_map_ram pages */
void * transform_pages_vm_map_ram_init(union buffer *buffer) {
	struct page_array *bpa = &buffer->page_array;
	void *ret;
	if (!(ret = vm_map_ram(bpa->p, bpa->ps, get_cpu(), PAGE_KERNEL)))
		return NULL;
	return ret;
}
void transform_pages_vm_map_ram_free(union buffer *buffer, void *data) {
	struct page_array *bpa = &buffer->page_array;
	if (data) vm_unmap_ram(data, bpa->ps);
}

/* ------------------
 * block array buffer
 * ------------------ */

/* vmalloc dupe */
void * transform_blocks_vmalloc_init(union buffer *buffer) {
	struct block_array *bba = &buffer->block_array;
	void *ret;
	int i;
	if (!(ret = vmalloc(bba->bs * bba->block_size)))
		return NULL;
	for (i = 0; i < bba->bs; i++)
		memcpy(ret + i * bba->block_size, bba->b[i], bba->block_size);
	return ret;
}
void transform_blocks_vmalloc_free(union buffer *buffer, void *data) {
	if (data) vfree(data);
}

/* kmalloc dupe */
void * transform_blocks_kmalloc_init(union buffer *buffer) {
	struct block_array *bba = &buffer->block_array;
	void *ret;
	int i;
	if (!(ret = kmalloc(bba->bs * bba->block_size, GFP_KERNEL)))
		return NULL;
	for (i = 0; i < bba->bs; i++)
		memcpy(ret + i * bba->block_size, bba->b[i], bba->block_size);
	return ret;
}
void transform_blocks_kmalloc_free(union buffer *buffer, void *data) {
	if (data) kfree(data);
}

struct transform_api transform_formats[] = {
// list_start
	{POINTER, "pointer_dummy", transform_pointer_dummy_init, transform_pointer_dummy_free},
	{POINTER, "pointer_vmalloc", transform_pointer_vmalloc_init, transform_pointer_vmalloc_free},
	{POINTER, "pointer_kmalloc", transform_pointer_kmalloc_init, transform_pointer_kmalloc_free},
	{PAGE_ARRAY, "pages_vmalloc", transform_pages_vmalloc_init, transform_pages_vmalloc_free},
	{PAGE_ARRAY, "pages_kmalloc", transform_pages_kmalloc_init, transform_pages_kmalloc_free},
	{PAGE_ARRAY, "pages_vmap", transform_pages_vmap_init, transform_pages_vmap_free},
	{PAGE_ARRAY, "pages_vm_map_ram", transform_pages_vm_map_ram_init, transform_pages_vm_map_ram_free},
	{BLOCK_ARRAY, "blocks_vmalloc", transform_blocks_vmalloc_init, transform_blocks_vmalloc_free},
	{BLOCK_ARRAY, "blocks_kmalloc", transform_blocks_kmalloc_init, transform_blocks_kmalloc_free},
// list_end
};

int transform_choose(char *name, struct transform_api *transform_api) {
	int i;
	for (i = 0; i < SIZE(transform_formats); i++)
		if (!strcmp(name, transform_formats[i].name)) {
			*transform_api = transform_formats[i];
			break;
		}
	return i == SIZE(transform_formats);
}
