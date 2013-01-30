VERSION = 2
PATCHLEVEL = 6
SUBLEVEL = 29
#EXTRAVERSION = 

obj-m += mymeminfo.o

KDIR=~/aosp/goldfish
PWD := $(shell pwd)

default:
    make -C $(KDIR) SUBDIRS=$(PWD) modules

clean:
    make -C $(KDIR) SUBDIRS=$(PWD) clean

