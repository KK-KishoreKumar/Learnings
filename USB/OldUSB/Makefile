# if KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
	src-m := $(wildcard ${SUBDIRS}/*.c)
	src-mod := $(wildcard ${SUBDIRS}/*.mod.c)
	src-m := $(notdir ${src-m})
	src-mod := $(notdir ${src-mod})
	src-m := $(filter-out ${src-mod}, ${src-m})
	obj-m := $(src-m:.c=.o)
# Otherwise we were called directly from the command
# line; invoke the kernel build system.
else
	KERNEL_SOURCE := /usr/src/linux
	PWD := $(shell pwd)
default:
	$(MAKE) -C $(KERNEL_SOURCE) SUBDIRS=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_SOURCE) SUBDIRS=$(PWD) clean
endif
