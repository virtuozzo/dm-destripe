#!/bin/ash

# Description: this is a script for ash in the controller VM to remove all existing destripe devices on a target...
# NOTE: assumes dm-destripe module is already loaded, but does not unload the module.

#
# Copyright (C) 2013 OnApp Ltd.
# Author: (C) 2013 Michail Flouris <michail.flouris@onapp.com>
#
# This file is released under the GPL.

if [ -z $2 ] ; then
	echo "Usage: $0 <destriped devs prefix in /dev/mapper/> <number of stripes>"
	exit 2
fi

outdmdev=$1 # device name under /dev/mapper/
stripes=$2 # need number of stripes argument

dss_name=destripe
dss_loaded=`/sbin/lsmod | grep $dss_name | wc -l`
if [ $dss_loaded -eq 0 ] ; then
	echo "Warning: Cannot find dm-$dss_name loaded!"
fi

#echo "#destripe stripes=$stripes outdevprefix=$outdmdev"

let "maxsidx = $stripes - 1"
stripeset="`seq 0 $maxsidx`"

for stripeidx in $stripeset
do
sidevice="${outdmdev}_${stripeidx}"
dss_device="/dev/mapper/$sidevice"
echo -n "[$stripeidx] Removing device $dss_device : "
if [ -b $dss_device ] ; then
	/sbin/dmsetup remove $sidevice
	if [ "$?" != 0 ] ; then
		echo "FAIL, dmsetup failure!"
	else
		echo 'OK'
	fi
else
	echo "FAIL, device does not exist!"
fi

done
