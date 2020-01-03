#KERN_DIR = /usr/src/linux-headers-4.4.0-116-generic
KERN_VER = $(shell uname -r)
KERN_DIR = /lib/modules/$(KERN_VER)/build	

all:
	make -C $(KERN_DIR) M=`pwd` modules 

clean:
	make -C $(KERN_DIR) M=`pwd` modules clean
	rm -rf modules.order

obj-m	+= chapter8_5.o
