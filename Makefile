obj-m += kfetch_mod.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

load:
	sudo insmod kfetch_mod.ko

unload:
	sudo rmmod kfetch_mod.ko

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

show:
	sudo dmesg | tail

