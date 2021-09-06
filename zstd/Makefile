CONFIG_MODULE_SIG=n
# SPDX-License-Identifier: GPL-2.0-only
obj-m += xxhash.o
obj-m += zstd_compress.o
obj-m += zstd_decompress.o

ccflags-y += -O3

zstd_compress-y := fse_compress.o huf_compress.o compress.o \
		   entropy_common.o fse_decompress.o zstd_common.o
zstd_decompress-y := huf_decompress.o decompress.o \
		     entropy_common.o fse_decompress.o zstd_common.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

debug:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	EXTRA_CFLAGS="$(MY_CFLAGS)"

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

install:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules_install
