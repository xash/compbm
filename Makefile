KBUILD_EXTRA_SYMBOLS += $(PWD)/zfs/module/Module.symvers
KBUILD_EXTRA_SYMBOLS += $(PWD)/lz4/Module.symvers
KBUILD_EXTRA_SYMBOLS += $(PWD)/zstd/Module.symvers
MY_CFLAGS += -g -DDEBUG
ccflags-y += ${MY_CFLAGS}
CC += ${MY_CFLAGS}
obj-m += compbm.o
compbm-objs += mod.o mem.o compress.o transform.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

debug:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	EXTRA_CFLAGS="$(MY_CFLAGS)"

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

install:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules_install


test_zero:
	dd if=/dev/zero of=./test_zero count=2K

test_random:
	dd if=/dev/urandom of=./test_random count=2K

tests.sh: test_zero test_random mem.c transform.c compress.c
	./generate_tests.sh
