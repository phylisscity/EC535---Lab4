# Makefile for mytraffic kernel module (cross-compiled for ARM)

obj-m += mytraffic.o

# path to ARM kernel source - adjust this to match workspace
KDIR := $(HOME)/EC535/lab4/stock-linux-4.19.82-ti-rt-r33

# cross-compiler for ARM architecture
CROSS_COMPILE := arm-linux-gnueabihf-
ARCH := arm

PWD := $(shell pwd)

# default target - builds the kernel module
all:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) modules

# removes all generated files
clean:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) clean

.PHONY: all clean
