#!/bin/bash

#
# Copyright (C) 2013 OnApp Ltd.
# Author: (C) 2013 Michail Flouris <michail.flouris@onapp.com>
#
# This file is released under the GPL.

if [ -z $4 ] ; then
	echo "Usage: $0 <device for destripe> <number of stripes> <stripe idx> <chunk size (sectors)>"
	exit -1
fi

dname=$1 # need dev argument!
destripes=$2 # need number of destripes argument
destripeidx=$3 # need number of destripes argument
chunksize=$4 # need chunk size argument

#bsize=`/sbin/blockdev --getsz /dev/$dname`
bsize=`/sbin/blockdev --getsize /dev/$dname`

let "singlestripesize = $bsize / $destripes"
echo "singlestripesize= $singlestripesize"

dms_devs="1 /dev/$dname 0"

if [ ! -e "/sys/block/$dname" ] || [ ! -e "/dev/$dname" ]; then
	echo "Device /dev/$dname does not exist!"
	exit -1
fi

dss_name=destripe
dss_module=dm-$dss_name
if [ ! -e */$dss_module.ko ] ; then
	echo "Cannot find $dss_module.ko !"
	echo "Perhaps you should run make?"
	exit -1
fi

dss_loaded=`/sbin/lsmod | grep $dss_name | wc -l`

if [ $dss_loaded -eq 0 ] ; then
	echo "Loading Destripe module..."
	make ins || exit -1
	echo "Destripe module ($dss_module) loaded!"
fi
sync;sync;sync

dss_devname=dss
dss_device=/dev/mapper/$dss_devname
if [ ! -b $dss_device ] ; then
	echo "Creating DSS device & map..."
	echo "/sbin/dmsetup create $dss_devname --table '0 $singlestripesize $dss_name $destripes $destripeidx $chunksize $dms_devs'"
	/sbin/dmsetup create $dss_devname --table "0 $singlestripesize $dss_name $destripes $destripeidx $chunksize $dms_devs"
	echo 'DMSETUP Created OK!'
	echo Size: `/sbin/blockdev --getsize $dss_device`
fi
sync;sync;sync

echo 'DONE!'

# DMSETUP CMD INFO:
# =================

# dmsetup targets
# -> shows the loaded device mapper targets... destripe should be in there if loaded!

# Setup of destripe:
# dmsetup create dss --table '0 400430 destripe 2 0 512 1 /dev/sdc 0'

# Removal of destripe device
# dmsetup remove dss

# Useful commands for dss device
# dmsetup table dss
# dmsetup ls
# dmsetup info
# dmsetup targets

