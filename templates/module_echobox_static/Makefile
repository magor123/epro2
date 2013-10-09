obj-m += echobox.o

CROSS = arm-linux-gnueabi-

KDIR = ~/SDU/EPRO2/source/kernel/kernel

PWD := $(shell pwd)

all:
	make ARCH=arm -C $(KDIR) M=$(PWD) CROSS_COMPILE=$(CROSS) modules
	scp echobox.ko root@192.168.7.2:/home/root/modules/
	scp init_echobox.sh root@192.168.7.2:/home/root/modules/
	make -C $(KDIR) M=$(PWD) CROSS_COMPILE=$(CROSS) clean
clean:
	make -C $(KDIR) M=$(PWD) CROSS_COMPILE=$(CROSS) clean

