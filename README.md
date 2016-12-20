# dm-destripe-
Device-mapper de-striping (reverse striping) module

Copyright (C) 2013-2016 OnApp Ltd.
Author: (C) 2013-2016 Michail Flouris <michail.flouris@onapp.com>

Implementation of a de-striping kernel module for device mapper
----------------------------------------------------------------

Arguments to create a dm-destripe device with (reverse stripe) mapping:

<number of stripes> <de-stripe index> <chunk size (sectors)> <...device arguments...>


Cmd-line example of creating a dm-stripe device with 2 stripes, using de-stripe index 0 (of [0,1])
and chunk-size of 256KB (512 sectors):

/sbin/dmsetup create dss --table '0 3145728 destripe 2 0 512 1 /dev/sdd 0'

