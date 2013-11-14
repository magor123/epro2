	# Makefile – makefile of halogen lights driver #

obj-m += microwave.o

CROSS = arm-linux-gnueabi-

KDIR = /home/matija/faks/DES/kernel/kernel/

PWD := $(shell pwd)

all:
	make ARCH=arm -C $(KDIR) M=$(PWD) CROSS_COMPILE=$(CROSS) modules
	scp microwave.ko root@192.168.7.2:/home/root/modules/
	make -C $(KDIR) M=$(PWD) CROSS_COMPILE=$(CROSS) clean
clean:
	make -C $(KDIR) M=$(PWD) CROSS_COMPILE=$(CROSS) clean

	
