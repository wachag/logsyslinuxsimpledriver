obj-m += simple.o

all:
	make  ARCH=microblaze CROSS_COMPILE=microblazeel-linux- -C /opt/linux/ddk/linux-4.14.14 M=$(PWD) modules

clean:
	make  ARCH=microblaze CROSS_COMPILE=microblazeel-linux- -C /opt/linux/ddk/linux-4.14.14 M=$(PWD) clean
