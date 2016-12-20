#!/bin/bash

#
# Copyright (C) 2013 OnApp Ltd.
# Author: (C) 2013 Michail Flouris <michail.flouris@onapp.com>
#
# This file is released under the GPL.

if [ $# -ne 3 ] ; then  # Must have 3 command-line args
	echo "Usage: $0 <device to destripe> <number of stripes> <chunk size (in sectors)>"
	exit -1
fi

dname=$1 # need dev argument!
destripes=$2 # need number of destripes argument
chunksize=$3 # need chunk size argument

if [ $destripes -lt 2 ] || [ $destripes -gt 8 ] ; then
	echo "Stripes must be between 2 and 8!"
	exit -1
fi

if [ $chunksize -lt 8 ] || [ $chunksize -gt 16384 ] ; then
	echo "Chunksize must be between 8 and 16384 sectors and power of 2!"
	exit -1
fi

let "chunkdiv = $chunksize % 2"
if [ $chunkdiv -ne 0 ] ; then
	echo "Chunksize must be power of 2!"
	exit -1
fi

bsize=`/sbin/blockdev --getsize /dev/$dname`

let "singlestripesize = $bsize / $destripes"
echo "Device_size=$bsize, Size_per_stripe=$singlestripesize"

dms_devs="1 /dev/$dname 0"

if [ ! -e "/sys/block/$dname" ] || [ ! -e "/dev/$dname" ]; then
	echo "Device /dev/$dname does not exist!"
	exit -1
fi

dss_name=destripe
dss_module=dm-$dss_name
if [ -e /lib/modules/`uname -r`/kernel/drivers/md/$dss_module.ko ] ; then
	dssmod=/lib/modules/`uname -r`/kernel/drivers/md/$dss_module.ko
elif [ -e /modules/`uname -r`/$dss_module.ko ] ; then
	dssmod=/modules/`uname -r`/$dss_module.ko
elif [ -e */$dss_module.ko ] ; then
	dssmod=''
else
	echo "Cannot find $dss_module.ko, perhaps you should install it?"
	exit -1
fi

dss_loaded=`/sbin/lsmod | grep $dss_name | wc -l`

if [ $dss_loaded -eq 0 ] ; then
	echo "Loading Destripe module..."
	if [ -z $dssmod ] ; then
		make ins || exit -1
	else
		insmod $dssmod
	fi
	dss_loaded=`/sbin/lsmod | grep $dss_name | wc -l`
	if [ $dss_loaded -eq 0 ] ; then
		echo "Failed to load destripe module ($dss_module)!"
		exit -1
	fi

	echo "Destripe module ($dss_module) loaded!"
else
	echo "Destripe module ($dss_module) is already loaded!"
fi
sync;sync;sync

# create as many destripe devs as the stripe count
dss_basename="dss_${dname}"

for idx in `seq 0 $(($destripes-1))`; do
	dss_devname=${dss_basename}$idx
	dss_device=/dev/mapper/$dss_devname
	if [ ! -b $dss_device ] ; then
		#echo "Creating DSS device $dss_devname..."
		#echo "/sbin/dmsetup create $dss_devname --table '0 $singlestripesize $dss_name $destripes $idx $chunksize $dms_devs'"
		/sbin/dmsetup create $dss_devname --table "0 $singlestripesize $dss_name $destripes $idx $chunksize $dms_devs"
		echo "DMSETUP Created dev $dss_devname OK, Size: `/sbin/blockdev --getsize $dss_device`"
	fi
done

# run tests [disabled by default - CAUTION: do NOT enable this ]
run_tests=false
if $run_tests ; then

sync && echo 3 > /proc/sys/vm/drop_caches && sync

for idx in `seq 0 $(($destripes-1))`; do
	dss_devname=${dss_basename}$idx
	dss_device=/dev/mapper/$dss_devname

	echo "dd of=/dev/null if=$dss_device bs=1M count=100 iflag=direct"
	dd of=/dev/null if=$dss_device bs=1M count=100 iflag=direct
done
fi # run_tests
echo 'DONE!'

