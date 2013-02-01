mymeminfo
=========

`mymeminfo` is a kernel module that provides a simple view into the current state of an Android device's state of memory.

Building
--------

First, initialize your environment:

    PATH=~/aosp/prebuilts/gcc/linux-x86/arm/arm-eabi-4.6/bin/:$PATH
    export ARCH=arm
    export SUBARCH=arm
    export CROSS_COMPILE=arm-eabi-

Then, build the module:

    cd goldfish/mymeminfo/
    make

Running
-------

First, load the module into your system:

    adb push meminfo.ko /data/local
    adb shell insmod /data/local/mymeminfo.ko

Then, read the current memory usage:

    adb shell cat /proc/mymeminfo

To unload the module, execute:

    adb shell rmmod mymeminfo
