#!/bin/ash

#
# Copyright (C) 2013 OnApp Ltd.
# Author: (C) 2013 Michail Flouris <michail.flouris@onapp.com>
#
# This file is released under the GPL.

# Description: this is a script for ash in the controller VM to remove an existing destripe target...
# NOTE: assumes dm-destripe module is already loaded, but does not unload the module.

if [ -z $1 ] ; then
	echo "Usage: $0 <destripe device to remove in /dev/mapper/>"
	exit 2
fi

devname=$1 # need dev argument!
dm_device=/dev/mapper/$devname

dss_name=destripe
dss_loaded=`/sbin/lsmod | grep $dss_name | wc -l`
if [ $dss_loaded -eq 0 ] ; then
	echo "Warning: Cannot find dm-$dss_name loaded!"
fi

if [ ! -e $dm_device ]; then
	echo "Device $dm_device does not exist!"
	exit 2
fi

bsize=`/sbin/blockdev --getsz $dm_device`

/sbin/dmsetup remove $dm_device

