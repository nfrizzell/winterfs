ifneq ($(KERNELRELEASE),)
	obj-m += winterfs.o
	winterfs-y := super.o
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD  := $(shell pwd)
default:
	make -C $(KERNELDIR) M=$(PWD) modules

clean:
	make -C $(KERNELDIR) M=$(PWD) clean

install: winterfs.ko
	install -d /lib/modules/$(shell uname -r)/kernel/fs/winterfs
	install -m 644 winterfs.ko /lib/modules/$(shell uname -r)/kernel/fs/winterfs
	depmod
endif
