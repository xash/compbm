CONFIG_MODULE_SIG=n

obj-m += lz4_compress.o
obj-m += lz4_decompress.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

debug:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	EXTRA_CFLAGS="$(MY_CFLAGS)"

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

install:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules_install
