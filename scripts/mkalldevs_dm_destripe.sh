#!/bin/ash

#
# Copyright (C) 2013 OnApp Ltd.
# Author: (C) 2013 Michail Flouris <michail.flouris@onapp.com>
#
# This file is released under the GPL.

# Description: this ash script in the controller VM creates all destriped devices from a striped target...
# NOTE: assumes dm-destripe module is already loaded.

if [ -z $4 ] ; then
	echo "Usage: $0 <block dev to destripe> <number of stripes> <chunk size (sectors)> <destriped devs prefix in /dev/mapper/>"
	exit 2
fi

devname=$1 # need target dev argument!
stripes=$2 # need number of stripes argument
chunksize=$3 # need chunk size argument (in sectors, as in dm table)
outdmdev=$4 # output device name under /dev/mapper/

if [ ! -e "$devname" ]; then
	echo "Device $devname does not exist!"
	exit 2
fi
if [ -e "/dev/mapper/$outdmdev" ]; then
	echo "Output Device /dev/mapper/$outdmdev already exists!"
	exit 2
fi

devsize=`/sbin/blockdev --getsz $devname`

let "stripesize = $devsize / $stripes"
echo "#destripe dev=$devname stripes=$stripes devsz=$devsize stripesz=$stripesize chunksz=$chunksize"

let "checkdivsize = $devsize % $stripesize"
if [ $checkdivsize != 0 ] ; then
	echo "ERROR: Device size is not a multiple of stripesize - $devsize / $stripesize !"
	exit 2
fi
let "checkchunksize = $stripesize % $chunksize"
if [ $checkchunksize != 0 ] ; then
	echo "ERROR: Stripe size is not a multiple of chunksize - $stripesize / $chunksize !"
	exit 2
fi

dss_name=destripe
dss_loaded=`/sbin/lsmod | grep $dss_name | wc -l`
if [ $dss_loaded -eq 0 ] ; then
	echo "Cannot find dm-$dss_name loaded!"
	exit 2
fi

let "maxsidx = $stripes - 1"
stripeset="`seq 0 $maxsidx`"
dms_devs="1 $devname 0"

for stripeidx in $stripeset
do

sidevice="${outdmdev}_${stripeidx}"
dss_device="/dev/mapper/$sidevice"
echo -n "[$stripeidx] Creating device $dss_device : "

if [ ! -b $dss_device ] ; then
	/sbin/dmsetup create $sidevice --table "0 $stripesize $dss_name $stripes $stripeidx $chunksize $dms_devs"
	if [ "$?" != 0 ] ; then
		echo "FAIL, dmsetup failure, aborting!"
		exit 2
	else
		echo 'OK, size: ' `/sbin/blockdev --getsz $dss_device`
	fi
else
	echo "FAIL, device already exists!"
fi

done
