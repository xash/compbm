#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/printk.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/vmalloc.h>

#include "mem.h"
#include "transform.h"
#include "compress.h"

#define MAX_FILE_SIZE (1024*1024*1024)

static char *path = "/dev/null";
static char *format_name = "dummy";
static char *compression_name = "dummy";
static char *transformation_name = "dummy";

#define ABORT(error, goto_target) { state = error; goto goto_target; }
enum state { OK, BUFFER, TRANSFORM, OUTPUT, COMPRESS, CHECK };
char *state_names[] = { "ok", "buffer_error", "transform_error", "output_buffer_error", "compress_error", "check_failed" };
long long memory_usage(void) {
	struct sysinfo mem_info;
	si_meminfo(&mem_info);
	return mem_info.freeram;
}

/* run a test for a given file and mem/transform/compression API */
void test(void *file, size_t file_size, struct mem_api mem, struct compress_api compress, struct transform_api transform) {
	union buffer buffer;
	enum state state = OK;
	void *buffer_pointer = NULL, *output = NULL, *check = NULL;
	unsigned long transform_time = 0, compression_time = 0, decompression_time = 0;
	long long memory_cost = 0;
	int compressed_size = 0;
	int output_len = (file_size * 3) / 2; // some padding for worst case

  /* init the initial format */
  if (mem.init(&buffer, file, file_size))
  	ABORT(BUFFER, EXIT0);

	/* compression needs a pointer, so transform the buffer */
	if (compress.type == POINTER) {
	  memory_cost = memory_usage();
	  transform_time = jiffies;
	  if (!(buffer_pointer = transform.init(&buffer)))
		  ABORT(TRANSFORM, EXIT1);
	  transform_time = jiffies - transform_time;
	  memory_cost = memory_usage() - memory_cost;
	}

  /* get output buffer */
  if (!(output = vmalloc(output_len)))
	  ABORT(OUTPUT, EXIT2);

	/* compress */
  compression_time = jiffies;
	if (!(compressed_size = compress.compress(&buffer, output, output_len, buffer_pointer, file_size, compress.level)))
		ABORT(COMPRESS, EXIT3);
	compression_time = jiffies - compression_time;

  /* get check buffer */
  if (!(check = vmalloc(file_size)))
	  ABORT(OUTPUT, EXIT4);

	/* decompress */
  decompression_time = jiffies;
	compress.decompress(&buffer, check, file_size, output, compressed_size);
	decompression_time = jiffies - decompression_time;

	if (memcmp(file, check, file_size))
		ABORT(CHECK, EXIT5);

EXIT5:
	vfree(check);
EXIT4:
EXIT3:
	vfree(output);
EXIT2:
	if (compress.type == POINTER)
		transform.free(&buffer, buffer_pointer);
EXIT1:
	mem.free(&buffer);
EXIT0:
	pr_alert("%s %s %s %s %s %lu %d %lu %lu %lu %lld\n",
           mem.name,
           transform.name,
           compress.name,
           path,
           state_names[state],
           file_size,
           compressed_size,
           transform_time,
           compression_time,
           decompression_time,
           memory_cost);
}

static int __init compbm_init(void) {
	struct mem_api buffer_api;
	struct compress_api compress_api;
	struct transform_api transform_api = { 0 };
	struct file *file = NULL;
	void *file_buffer = NULL;
	size_t file_size = 0;
	loff_t off = 0;

	/* read file into buffer */
	file = filp_open(path, O_RDONLY, 0);
	if (file == NULL) {
		pr_alert("could not open file %s\n", path);
		return 0;
	}

	file_buffer = vmalloc(MAX_FILE_SIZE);
	if (!file_buffer) {
		pr_alert("could not allocate buffer\n");
		filp_close(file, NULL);
		return 0;
	}
	file_size = kernel_read(file, off, file_buffer, MAX_FILE_SIZE);
	filp_close(file, NULL);

	/* find function structs based on parameter names */
	/* transform is only needed iff compressor expects a pointer */
	if (mem_choose(format_name, &buffer_api)) {
		pr_alert("format %s not found\n", format_name);
		goto INIT_ERR;
	}
	if (compress_choose(compression_name, &compress_api)) {
		pr_alert("compression %s not found\n", compression_name);
		goto INIT_ERR;
	}
	if (compress_api.type == POINTER && transform_choose(transformation_name, &transform_api)) {
		pr_alert("transformation %s not found\n", transformation_name);
		goto INIT_ERR;
	}
	if (compress_api.type == POINTER && buffer_api.format != transform_api.format) {
		pr_alert("transformation %s not working with %s\n", transformation_name, format_name);
		goto INIT_ERR;
	}
	/* zfs_zstd saves context between runs. So we will init non-zfs versions
   * a context before the benchmark. */
	if (compress_init(compress_api)) {
		pr_alert("compress_init failed\n");
		goto INIT_ERR;
	}

	test(file_buffer, file_size, buffer_api, compress_api, transform_api);
	
	compress_free();

INIT_ERR:
	vfree(file_buffer);
	return 0;
}


static void __exit compbm_exit(void) {
}

module_init(compbm_init);
module_exit(compbm_exit);

module_param(format_name, charp, 0000);
MODULE_PARM_DESC(format_name, "Format to use");
module_param(compression_name, charp, 0000);
MODULE_PARM_DESC(compression_name, "Compression to use");
module_param(transformation_name, charp, 0000);
MODULE_PARM_DESC(transformation_name, "Transformation to use");
module_param(path, charp, 0000);
MODULE_PARM_DESC(path, "Absolute path to the test file");

MODULE_SOFTDEP("post: zzstd");
MODULE_SOFTDEP("post: lz4_compress");
MODULE_SOFTDEP("post: lz4_decompress");

MODULE_LICENSE("GPL");
