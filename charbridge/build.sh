#!/bin/sh

# Usage: ./build.sh [kversion-flavour]

#kf=$(uname -r | sed -e "s|-alt.*||")
kf=$(uname -r)
[ -n "$1" ] && kf="$1"

if [ ! -d /usr/src/linux-$kf ] ; then
    echo "No such sources: /usr/src/linux-$kf"
    echo "Check the dir list:"
    ls -1d /usr/src/linux-*
    exit 1
fi

. /usr/src/linux-$kf/gcc_version.inc

PWD=`pwd`

make -C /usr/src/linux-$kf M=$PWD V=1
