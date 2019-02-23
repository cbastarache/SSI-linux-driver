KERNEL := 4.19.23-v7+
CR_C := arm-linux-gnueabihf-
MODULES := /home/coreyb/rpi_kernel/linux-4.19.y
obj-m := robot_ssi.o

PI := pi@192.168.1.31:~/ssi_module

all:
	make ARCH=arm CROSS_COMPILE=$(CR_C) -C $(MODULES) M=$(shell pwd) modules
	scp ./$(obj-m:.o=.ko) $(PI)

clean:
	make ARCH=arm CROSS_COMPILE=$(CR_C) -C $(MODULES) M=$(shell pwd) clean

