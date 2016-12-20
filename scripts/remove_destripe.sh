#!/bin/bash

#
# Copyright (C) 2013 OnApp Ltd.
# Author: (C) 2013 Michail Flouris <michail.flouris@onapp.com>
#
# This file is released under the GPL.

sync;sync;sync

/sbin/dmsetup remove dss
echo 'DMSETUP REMOVE OK!'

#/sbin/rmmod dm-destripe.ko
make rmm
echo 'ALL DONE!'

