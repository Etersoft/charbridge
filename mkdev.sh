#!/bin/sh

for num in `seq 0 9`; do

	numB=`echo $num+100 | bc`
	mknod /dev/cbsideA$num c 254 $num
	mknod /dev/cbsideB$num c 254 $numB
	chmod 'a+rw' /dev/cbsideA$num /dev/cbsideB$num

done
