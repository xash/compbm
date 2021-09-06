#include <linux/init.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/vmalloc.h>

#include "lz4/lz4.h"
#include "zstd/zstd.h"
#include "zfs/include/sys/zstd/zstd.h"

#include "compress.h"
#define SIZE(a) (sizeof(a)/sizeof(*a))

/* in non-zfs versions we need to initialize working contexts */
void *lz4_workmem, *zstd_cworkmem, *zstd_dworkmem;
ZSTD_compressionParameters zstd_cparam;
ZSTD_parameters zstd_param;
ZSTD_CCtx *zstd_ccontext;
ZSTD_DCtx *zstd_dcontext;
LZ4_stream_t *lz4_stream;
LZ4_streamDecode_t *lz4_streamDecode;

int _memcpy_compress(union buffer *buffer, void *dest, int dest_s, void *src, int src_s, int level) {
	memcpy(dest, src, src_s);
	return src_s;
}
int _memcpy_decompress(union buffer *buffer, void *dest, int dest_s, void *src, int src_s) {
	memcpy(dest, src, src_s);
	return src_s;
}
int _zfs_zstd_compress(union buffer *buffer, void *dest, int dest_s, void *src, int src_s, int level) {
	return zfs_zstd_compress(src, dest, src_s, dest_s, level);
}
int _zfs_zstd_decompress(union buffer *buffer, void *dest, int dest_s, void *src, int src_s) {
	return zfs_zstd_decompress(src, dest, src_s, dest_s, 0);
}
int _lz4_compress(union buffer *buffer, void *dest, int dest_s, void *src, int src_s, int level) {
	return LZ4_compress_fast(src, dest, src_s, dest_s, level, lz4_workmem);
}
int _lz4_decompress(union buffer *buffer, void *dest, int dest_s, void *src, int src_s) {
	return LZ4_decompress_fast(src, dest, dest_s);
}
int _zstd_compress(union buffer *buffer, void *dest, int dest_s, void *src, int src_s, int level) {
	return ZSTD_compressCCtx(zstd_ccontext, dest, dest_s, src, src_s, zstd_param);
}
int _zstd_decompress(union buffer *buffer, void *dest, int dest_s, void *src, int src_s) {
	return ZSTD_decompressDCtx(zstd_dcontext, dest, dest_s, src, src_s);
}
int blocks_lz4_compress_stream(union buffer *buffer, void *dest, int dest_s, void *src, int src_s, int level) {
	struct block_array bba = buffer->block_array;
	int i;
	int frame_size, compressed_size = 0;

	/* compress frames */
	for (i = 0; i < bba.bs; i++) {
		frame_size = LZ4_compress_fast_continue(
			lz4_stream, bba.b[i], &((char *)dest)[compressed_size],
			/* edge case for last frame */
			(i + 1) < bba.bs ? bba.block_size : src_s - ((bba.bs - 1) * bba.block_size),
			dest_s - compressed_size, level
		);
		if (frame_size <= 0)
			return 0;
		compressed_size += frame_size;
	}
	
	return compressed_size;
}
int blocks_lz4_decompress_stream(union buffer *buffer, void *dest, int dest_s, void *src, int src_s) {
	struct block_array bba = buffer->block_array;
	int i;
	int frame_size, offset = 0;

	/* decompress frames */
	for (i = 0; i < bba.bs; i++) {
		frame_size = LZ4_decompress_fast_continue(
			lz4_streamDecode, &((char*)src)[offset], &((char*)dest)[i * bba.block_size],
			/* edge case for last frame */
			(i + 1) < bba.bs ? bba.block_size : dest_s - ((bba.bs - 1) * bba.block_size)
		);
		if (frame_size <= 0)
			return 0;
		offset += frame_size;
	}
	
	return offset;
}

/* same as blocks_lz4_compress_stream, just with PAGE_SIZE insted block_size */
int pages_lz4_compress_stream(union buffer *buffer, void *dest, int dest_s, void *src, int src_s, int level) {
	struct page_array bpa = buffer->page_array;
	int i;
	int frame_size, compressed_size = 0;

	for (i = 0; i < bpa.ps; i++) {
		frame_size = LZ4_compress_fast_continue(
			lz4_stream, page_address(bpa.p[i]), &((char *)dest)[compressed_size],
			PAGE_SIZE, dest_s - compressed_size, level
		);
		if (frame_size <= 0)
			return 0;
		compressed_size += frame_size;
	}
	
	return compressed_size;
}

int pages_lz4_decompress_stream(union buffer *buffer, void *dest, int dest_s, void *src, int src_s) {
	struct page_array bpa = buffer->page_array;
	int i;
	int frame_size, offset = 0;

	for (i = 0; i < bpa.ps; i++) {
		frame_size = LZ4_decompress_fast_continue(
			lz4_streamDecode, &((char*)src)[offset], &((char*)dest)[i * PAGE_SIZE],
			PAGE_SIZE
		);
		if (frame_size <= 0)
			return 0;
		offset += frame_size;
	}
	
	return offset;
}
struct compress_api compress_list[] = {
// list_start
	{POINTER, "dummy", _memcpy_compress, _memcpy_decompress, 0},
	{POINTER, "lz4_0", _lz4_compress, _lz4_decompress, 1},
	{POINTER, "lz4_1", _lz4_compress, _lz4_decompress, 1},
	{POINTER, "lz4_2", _lz4_compress, _lz4_decompress, 2},
	{POINTER, "lz4_3", _lz4_compress, _lz4_decompress, 3},
	{POINTER, "lz4_4", _lz4_compress, _lz4_decompress, 4},
	{POINTER, "lz4_5", _lz4_compress, _lz4_decompress, 5},
	{POINTER, "lz4_6", _lz4_compress, _lz4_decompress, 6},
	{POINTER, "lz4_7", _lz4_compress, _lz4_decompress, 7},
	{POINTER, "lz4_8", _lz4_compress, _lz4_decompress, 8},
	{POINTER, "lz4_9", _lz4_compress, _lz4_decompress, 9},
	{POINTER, "zfs_zstd_0", _zfs_zstd_compress, _zfs_zstd_decompress, 1},
	{POINTER, "zfs_zstd_1", _zfs_zstd_compress, _zfs_zstd_decompress, 1},
	{POINTER, "zfs_zstd_2", _zfs_zstd_compress, _zfs_zstd_decompress, 2},
	{POINTER, "zfs_zstd_3", _zfs_zstd_compress, _zfs_zstd_decompress, 3},
	{POINTER, "zfs_zstd_4", _zfs_zstd_compress, _zfs_zstd_decompress, 4},
	{POINTER, "zfs_zstd_5", _zfs_zstd_compress, _zfs_zstd_decompress, 5},
	{POINTER, "zfs_zstd_6", _zfs_zstd_compress, _zfs_zstd_decompress, 6},
	{POINTER, "zfs_zstd_7", _zfs_zstd_compress, _zfs_zstd_decompress, 7},
	{POINTER, "zfs_zstd_8", _zfs_zstd_compress, _zfs_zstd_decompress, 8},
	{POINTER, "zfs_zstd_9", _zfs_zstd_compress, _zfs_zstd_decompress, 9},
	{POINTER, "zstd_0", _zstd_compress, _zstd_decompress, 1},
	{POINTER, "zstd_1", _zstd_compress, _zstd_decompress, 1},
	{POINTER, "zstd_2", _zstd_compress, _zstd_decompress, 2},
	{POINTER, "zstd_3", _zstd_compress, _zstd_decompress, 3},
	{POINTER, "zstd_4", _zstd_compress, _zstd_decompress, 4},
	{POINTER, "zstd_5", _zstd_compress, _zstd_decompress, 5},
	{POINTER, "zstd_6", _zstd_compress, _zstd_decompress, 6},
	{POINTER, "zstd_7", _zstd_compress, _zstd_decompress, 7},
	{POINTER, "zstd_8", _zstd_compress, _zstd_decompress, 8},
	{POINTER, "zstd_9", _zstd_compress, _zstd_decompress, 9},
	{BLOCK_ARRAY, "blocks_lz4_stream_0", blocks_lz4_compress_stream, blocks_lz4_decompress_stream, 0},
	{BLOCK_ARRAY, "blocks_lz4_stream_1", blocks_lz4_compress_stream, blocks_lz4_decompress_stream, 1},
	{BLOCK_ARRAY, "blocks_lz4_stream_2", blocks_lz4_compress_stream, blocks_lz4_decompress_stream, 2},
	{BLOCK_ARRAY, "blocks_lz4_stream_3", blocks_lz4_compress_stream, blocks_lz4_decompress_stream, 3},
	{BLOCK_ARRAY, "blocks_lz4_stream_4", blocks_lz4_compress_stream, blocks_lz4_decompress_stream, 4},
	{BLOCK_ARRAY, "blocks_lz4_stream_5", blocks_lz4_compress_stream, blocks_lz4_decompress_stream, 5},
	{BLOCK_ARRAY, "blocks_lz4_stream_6", blocks_lz4_compress_stream, blocks_lz4_decompress_stream, 6},
	{BLOCK_ARRAY, "blocks_lz4_stream_7", blocks_lz4_compress_stream, blocks_lz4_decompress_stream, 7},
	{BLOCK_ARRAY, "blocks_lz4_stream_8", blocks_lz4_compress_stream, blocks_lz4_decompress_stream, 8},
	{BLOCK_ARRAY, "blocks_lz4_stream_9", blocks_lz4_compress_stream, blocks_lz4_decompress_stream, 9},
	{PAGE_ARRAY, "pages_lz4_stream_0", pages_lz4_compress_stream, pages_lz4_decompress_stream, 0},
	{PAGE_ARRAY, "pages_lz4_stream_1", pages_lz4_compress_stream, pages_lz4_decompress_stream, 1},
	{PAGE_ARRAY, "pages_lz4_stream_2", pages_lz4_compress_stream, pages_lz4_decompress_stream, 2},
	{PAGE_ARRAY, "pages_lz4_stream_3", pages_lz4_compress_stream, pages_lz4_decompress_stream, 3},
	{PAGE_ARRAY, "pages_lz4_stream_4", pages_lz4_compress_stream, pages_lz4_decompress_stream, 4},
	{PAGE_ARRAY, "pages_lz4_stream_5", pages_lz4_compress_stream, pages_lz4_decompress_stream, 5},
	{PAGE_ARRAY, "pages_lz4_stream_6", pages_lz4_compress_stream, pages_lz4_decompress_stream, 6},
	{PAGE_ARRAY, "pages_lz4_stream_7", pages_lz4_compress_stream, pages_lz4_decompress_stream, 7},
	{PAGE_ARRAY, "pages_lz4_stream_8", pages_lz4_compress_stream, pages_lz4_decompress_stream, 8},
	{PAGE_ARRAY, "pages_lz4_stream_9", pages_lz4_compress_stream, pages_lz4_decompress_stream, 9},
// list_end
};

int compress_choose(char *name, struct compress_api *compress_api) {
	int i;
	for (i = 0; i < SIZE(compress_list); i++)
		if (!strcmp(name, compress_list[i].name)) {
			*compress_api = compress_list[i];
			break;
		}
	return i == SIZE(compress_list);
}

int compress_init(struct compress_api compress_api) {
	/* LZ4 needs an explicit workmem. zstd allocates his own on the first run,
	 * but keeps it through runs, as the zstd module stays loaded */
	size_t cworkmem_size = 0, dworkmem_size = 0;
	
	/* alloc workmem */
	if (!(lz4_stream = kmalloc(sizeof(LZ4_stream_t), GFP_KERNEL)))
		return 1;
	memset(lz4_stream, 0, sizeof(LZ4_stream_t));
	
	if (!(lz4_streamDecode = kmalloc(sizeof(LZ4_streamDecode_t), GFP_KERNEL)))
		return 1;
	memset(lz4_streamDecode, 0, sizeof(LZ4_streamDecode_t));

	if (compress_api.compress == _lz4_compress)
		if (!(lz4_workmem = vmalloc(LZ4_MEM_COMPRESS)))
			return 1;
	if (compress_api.compress == _zstd_compress) {
		pr_alert("foo\n");
		zstd_cparam = ZSTD_getCParams(compress_api.level, 0 /* unknown input size */, 0 /* no dictionary */);
		pr_alert("foo\n");
		zstd_param = ZSTD_getParams(compress_api.level, 0 /* unknown input size */, 0 /* no dictionary */);
		pr_alert("foo\n");
		cworkmem_size = ZSTD_CCtxWorkspaceBound(zstd_cparam);
		pr_alert("foo %d\n", cworkmem_size);
		if (!(zstd_cworkmem = vmalloc(cworkmem_size)))
			return 1;
		pr_alert("foo\n");
		if (!(zstd_ccontext = ZSTD_initCCtx(zstd_cworkmem, cworkmem_size)))
			return 1;
		pr_alert("foo\n");

		dworkmem_size = ZSTD_DCtxWorkspaceBound();
		if (!(zstd_dworkmem = vmalloc(dworkmem_size)))
			return 1;
		pr_alert("foo\n");
		if (!(zstd_dcontext = ZSTD_initDCtx(zstd_dworkmem, dworkmem_size)))
			return 1;
		pr_alert("foo\n");
	}	
	return 0;
}

void compress_free(void) {
	if (lz4_workmem) vfree(lz4_workmem);
	if (zstd_cworkmem) vfree(zstd_cworkmem);
	if (zstd_dworkmem) vfree(zstd_dworkmem);
	if (lz4_stream) kfree(lz4_stream);
	if (lz4_streamDecode) kfree(lz4_streamDecode);
}
