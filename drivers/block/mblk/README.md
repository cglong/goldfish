mblk
=========

`mblk` is a kernel module that allows users to use multiple RAM-based block drives.

Building
--------

First, initialize your environment:

    PATH=~/aosp/prebuilts/gcc/linux-x86/arm/arm-eabi-4.6/bin/:$PATH
    export ARCH=arm
    export SUBARCH=arm
    export CROSS_COMPILE=arm-eabi-

Then, build the kernel with the module added:

    cd goldfish
    make

Running
-------

First, load the module into your system, with an optional number of devices to initialize:

    adb push drivers/block/mblk/mblk.ko /data/local
    adb shell insmod /data/local/mblk.ko device_num=8

To use `mblk`, simply call it in your programs using standard C I/O functions:

    fd = open("/dev/block/mblk0", O_RDWR);
    write(fd, mesg, size);
    read(fd, readbuf, size);
    close(fd);

To unload the module, execute:

    adb shell rmmod mblk
