#!/bin/sh

#kversion=2.6.16
#kversion=2.6.12
kversion=2.6.28
flavour=lks-wks
. /usr/src/linux-$kversion-$flavour/gcc_version.inc

PWD=`pwd`

#make -C /usr/src/linux SUBDIRS=$PWD V=1 modules
make -C /usr/src/linux-$kversion-$flavour SUBDIRS=$PWD V=1 modules

# пока не реализован..
#gcc -o ioctl ioctl.c
