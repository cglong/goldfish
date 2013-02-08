myschedinfo
=========

`myschedinfo` is a kernel module that provides a simple view into the current state of an Android device's process scheduler.

Building
--------

First, initialize your environment:

    PATH=~/aosp/prebuilts/gcc/linux-x86/arm/arm-eabi-4.6/bin/:$PATH
    export ARCH=arm
    export SUBARCH=arm
    export CROSS_COMPILE=arm-eabi-

Then, build the module:

    cd goldfish/myschedinfo/
    make

Running
-------

First, load the module into your system:

    adb push myschedinfo.ko /data/local
    adb shell insmod /data/local/myschedinfo.ko

Then, read the scheduler information for process ID `10`:

    adb shell echo "10" > /proc/myschedinfo
    adb shell cat /proc/myschedinfo

To unload the module, execute:

    adb shell rmmod myschedinfo
