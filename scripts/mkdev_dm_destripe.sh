#!/bin/ash

# Description: this is a script for ash in the controller VM to create the destripe target...
# NOTE: assumes dm-destripe module is already loaded.

#
# Copyright (C) 2013 OnApp Ltd.
# Author: (C) 2013 Michail Flouris <michail.flouris@onapp.com>
#
# This file is released under the GPL.

if [ -z $5 ] ; then
	echo "Usage: $0 <block dev to destripe> <number of stripes> <stripe idx> <chunk size (sectors)> <destriped device name in /dev/mapper/>"
	exit 2
fi

devname=$1 # need target dev argument!
stripes=$2 # need number of stripes argument
stripeidx=$3 # need number of stripe idx to use argument [0..stripes-1]
chunksize=$4 # need chunk size argument (in sectors, as in dm table)
outdmdev=$5 # output device name under /dev/mapper/

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
echo "#destripe dev=$devname stripes=$stripes stridx=$stripeidx devsz=$devsize stripesz=$stripesize chunksz=$chunksize"

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
if [ $stripeidx -ge $stripes ] ; then
	echo "ERROR: Invalid stripe index, should be 0...$stripes-1 !"
	exit 2
fi

dss_name=destripe
dss_loaded=`/sbin/lsmod | grep $dss_name | wc -l`
if [ $dss_loaded -eq 0 ] ; then
	echo "Cannot find dm-$dss_name loaded!"
	exit 2
fi

dms_devs="1 $devname 0"
dss_device=/dev/mapper/$outdmdev
if [ ! -b $dss_device ] ; then
	#echo "Creating DSS device & map..."
	#echo "/sbin/dmsetup create $outdmdev --table '0 $stripesize $dss_name $stripes $stripeidx $chunksize $dms_devs'"
	/sbin/dmsetup create $outdmdev --table "0 $stripesize $dss_name $stripes $stripeidx $chunksize $dms_devs"
	if [ "$?" != 0 ] ; then
		echo "Dmsetup failed to create $outmdev!"
		exit 2
	fi
	echo '#Dev created, size: ' `/sbin/blockdev --getsz $dss_device`
else
	echo "Device $dss_device already exists!"
	exit 2
fi
