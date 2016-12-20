#!/bin/bash

#
# Copyright (C) 2013 OnApp Ltd.
# Author: (C) 2013 Michail Flouris <michail.flouris@onapp.com>
#
# This file is released under the GPL.

if [ $# -ne 2 ] ; then  # Must have 2 command-line args
	echo "Usage: $0 <device to destripe> <number of stripes>"
	exit -1
fi

dname=$1 # need dev argument!
destripes=$2 # need number of destripes argument

if [ $destripes -lt 2 ] || [ $destripes -gt 8 ] ; then
	echo "Stripes must be between 2 and 8!"
	exit -1
fi

if [ ! -e "/sys/block/$dname" ] || [ ! -e "/dev/$dname" ]; then
	echo "Device /dev/$dname does not exist!"
	exit -1
fi

bsize=`/sbin/blockdev --getsize /dev/$dname`

dss_name=destripe
dss_module=dm-$dss_name

dss_loaded=`/sbin/lsmod | grep $dss_name | wc -l`

if [ $dss_loaded -eq 0 ] ; then
	echo "Destripe module is not loaded! Aborting..."
	exit -1
fi
sync;sync;sync

# create as many destripe devs as the stripe count
dss_basename="dss_${dname}"

# Done with test, now remove...
for idx in `seq 0 $(($destripes-1))`; do
	dss_devname=${dss_basename}$idx
	dss_device=/dev/mapper/$dss_devname
	/sbin/dmsetup remove $dss_devname
done

# Do not unload the module...

echo 'DMSETUP REMOVED!'
