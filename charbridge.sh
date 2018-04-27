#!/bin/sh

if [ "$1" == "add" ]; then
    TYPE=`dmesg | grep "The major device=" | sed 's|\(.*\)=\(.*\).|\2|g' | tail -n 1`

    for num in `seq 0 9`; do
        numB=$(($num+100))
	rm -f /dev/cbsideA$num /dev/cbsideB$num
        mknod /dev/cbsideA$num c $TYPE $num
	mknod /dev/cbsideB$num c $TYPE $numB
        chmod 'a+rw' /dev/cbsideA$num /dev/cbsideB$num
    done
elif [ "$1" == "remove" ]; then
    rm -f /dev/cbside*
fi
