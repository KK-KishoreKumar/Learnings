# If called directly from the command line, invoke the kernel build system.
ifeq ($(KERNELRELEASE),)

	KERNEL_SOURCE := /home/pradeep/EmbeddedLinux/temp/linux-3.12.9
	PWD := $(shell pwd)
default:
	$(MAKE) -C $(KERNEL_SOURCE) SUBDIRS=$(PWD) modules ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- CFLAGS=-DDEBUG

clean:
	$(MAKE) -C $(KERNEL_SOURCE) SUBDIRS=$(PWD) clean

# Otherwise KERNELRELEASE is defined; we've been invoked from the
# kernel build system and can use its language.
else

	#src-m := $(notdir $(wildcard ${SUBDIRS}/*.c))
	#src-mod := $(notdir $(wildcard ${SUBDIRS}/*.mod.c))
	#src-m := $(filter-out ${src-mod}, ${src-m})
	#obj-m := final_char_driver.o i2c-omap.o
	obj-m := i2c.o
	obj-m := single_i2c.o
	obj-m := omap_i2c.o
        i2c-y := final_char_driver.o i2c-omap.o
        single_i2c-y := final_char_driver.o i2c-single.o

endif
