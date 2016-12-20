/**
 * Device mapper destripe (i.e. reverse striping) driver.
 *
 * Copyright (C) 2013 OnApp Ltd.
 *
 * Author: Michail Flouris <michail.flouris@onapp.com>
 *
 * This file is part of the device mapper destriping driver/module.
 * 
 * The dm-destripe driver is free software: you can redistribute 
 * it and/or modify it under the terms of the GNU General Public 
 * License as published by the Free Software Foundation, either 
 * version 2 of the License, or (at your option) any later version.
 * 
 * Some open source application is distributed in the hope that it will 
 * be useful, but WITHOUT ANY WARRANTY; without even the implied warranty 
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
 */

/* Include some original dm header files */
#include <linux/device-mapper.h>

#include <linux/module.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/bio.h>
#include <linux/slab.h>
#include <linux/log2.h>

#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/mempool.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>

#include "dm-destripe.h"		/* Local destripe header file */

#define DM_MSG_PREFIX "destripe"
#define DM_IO_ERROR_THRESHOLD 15


static void destripe_map_sector(struct destripe_set *dss,
					sector_t sector, sector_t *mapped_sec)
{
	sector_t chunk = dm_target_offset(dss->ti, sector);
	sector_t chunk_offset, stripe_set_offset;

	DRSDEBUG("destripe_map_sector() ENTER  sector= %lu, chunk= %lu \n",
				(unsigned long)sector, (unsigned long)chunk );

	if (dss->chunk_size_shift < 0)
		chunk_offset = sector_div(chunk, dss->chunk_size);
	else {
		chunk_offset = chunk & (dss->chunk_size - 1);
		chunk >>= dss->chunk_size_shift;
	}

	stripe_set_offset = chunk * dss->destripes; /* spread chunk to stripe length */

	chunk = stripe_set_offset + dss->destripe_idx;

	if (dss->chunk_size_shift < 0)
		chunk *= dss->chunk_size;
	else
		chunk <<= dss->chunk_size_shift;

	*mapped_sec = chunk + chunk_offset;

	DRSDEBUG("destripe_map_sector() END    map_sec= %lu, chunk= %lu, chunk_offset= %lu\n",
				(unsigned long)*mapped_sec, (unsigned long)chunk, (unsigned long)chunk_offset );
}

/*----------------------------------------------------------------- */

static int destripe_map_range(struct destripe_set *dss, struct bio *bio)
{
	sector_t begin, end;

	destripe_map_sector(dss, bio->bi_sector, &begin);
	destripe_map_sector(dss, bio->bi_sector + bio_sectors(bio), &end);
	if (begin < end) {
		bio->bi_bdev = dss->destripe[0].dev->bdev;
		bio->bi_sector = begin + dss->destripe[0].physical_start;
		bio->bi_size = to_bytes(end - begin);
		return DM_MAPIO_REMAPPED;
	} else {
		/* The range doesn't map to the target stripe */
		bio_endio(bio, 0);
		return DM_MAPIO_SUBMITTED;
	}
}
/* ----------------------------------------------------------------
 * Destripe mapping function -> All the I/O action goes through here!
 */
static int destripe_map(struct dm_target *ti, struct bio *bio,
		      union map_info *map_context)

{
	struct destripe_set *dss = ti->private;
	int rw = bio_rw(bio);

	if (bio->bi_rw & REQ_FLUSH) {
		BUG_ON(map_context->target_request_nr != 0);
		bio->bi_bdev = dss->destripe[0].dev->bdev;
		return DM_MAPIO_REMAPPED;
	}
	if (unlikely(bio->bi_rw & REQ_DISCARD)) {
		BUG_ON(map_context->target_request_nr != 0);
		return destripe_map_range(dss, bio);
	}

	destripe_map_sector(dss, bio->bi_sector, &bio->bi_sector);

	bio->bi_sector += dss->destripe[0].physical_start;
	bio->bi_bdev = dss->destripe[0].dev->bdev;

	/* Handling writes... fwd them and get a callback at destripe_end_io() */
	if (rw == WRITE) {

		DRSDEBUG("[%s] dm-destripe REQ: WRITE Addr: %lld Size: %d\n", dm_device_name(dsd),
		   				(unsigned long long)bio->bi_sector << 9, bio->bi_size);

		atomic_inc( &dss->write_ios_total );
	   	atomic_inc( &dss->write_ios_pending );

	} else { /* It's all about the reads here... */

		DRSDEBUG("[%s] dm-destripe REQ: READ Addr: %lld Size: %d\n", dm_device_name(dsd),
						(unsigned long long)bio->bi_sector << 9, bio->bi_size);

		atomic_inc( &dss->read_ios_total );
	   	atomic_inc( &dss->read_ios_pending );
	}

	return DM_MAPIO_REMAPPED;
}

/*----------------------------------------------------------------- */

/* NOTE: the destripe_end_io handler is called after the async
 *       read/write_callback() functions... */

static int destripe_end_io(struct dm_target *ti, struct bio *bio,
			 int error, union map_info *map_context)
{
	struct destripe_set *dss = (struct destripe_set *) ti->private;
	char major_minor[16];

	DRSDEBUG_CALL("destripe_end_io called...\n");

	/* Update our pending I/O counters... */
	if ( bio_rw(bio) == WRITE)
	   	atomic_dec( &dss->write_ios_pending );
	else
		atomic_dec( &dss->read_ios_pending );

	if (!error)
		return 0; /* No error, I/O completed successfully */

	/* Oops... error occurred... */
	if ((error == -EWOULDBLOCK) && (bio->bi_rw & REQ_RAHEAD))
		return error;

	if (error == -EOPNOTSUPP)
		return error;

	memset(major_minor, 0, sizeof(major_minor));
	sprintf(major_minor, "%d:%d",
		MAJOR(disk_devt(bio->bi_bdev->bd_disk)),
		MINOR(disk_devt(bio->bi_bdev->bd_disk)));

	/*
	 * Test to see which stripe drive triggered the error event
	 * and increment error count for all stripes on that device.
	 * If the error count for a given device exceeds the threshold
	 * value we will no longer trigger any further events.
	 */
	if (!strcmp(dss->destripe[0].dev->name, major_minor)) {
		atomic_inc(&(dss->destripe[0].error_count));
		if (atomic_read(&(dss->destripe[0].error_count)) <
		    DM_IO_ERROR_THRESHOLD)
			schedule_work(&dss->trigger_event);
	}

	return error;
}

/*----------------------------------------------------------------- */

static void destripe_presuspend(struct dm_target *ti)
{
	struct destripe_set *dss = (struct destripe_set *) ti->private;

	DRSDEBUG_CALL("destripe_presuspend called...\n");
	atomic_set(&dss->suspend, 1);
}

/*----------------------------------------------------------------- */

static void destripe_postsuspend(struct dm_target *ti)
{
	struct destripe_set *dss = (struct destripe_set *) ti->private;

	DRSDEBUG_CALL("destripe_postsuspend called...\n");
	assert( atomic_read(&dss->suspend) == 1); // should already be suspended...
}

/*----------------------------------------------------------------- */

static void destripe_resume(struct dm_target *ti)
{
	struct destripe_set *dss = (struct destripe_set *) ti->private;

	DRSDEBUG_CALL("destripe_resume called...\n");

	/* NOTE: this assertion is wrong, because resume is called also at device init...
	assert( atomic_read(&dss->suspend) == 1);
	 */

	atomic_set(&dss->suspend, 0); /* lower suspend flag... */
}

/*----------------------------------------------------------------- */

/* Set read policy & parameters via the message interface. */
static int destripe_message(struct dm_target *ti, unsigned argc, char **argv)
{
	struct destripe_set *dss = ti->private;
	//char dummy;

	DRSDEBUG_CALL("destripe_message called...\n");

	/* INFO: valid message forms [ALWAYS 4 args - use 0 for unused values]:
	 * io_cmd <command_type> <cmd_arg1> <cmd_arg2>
	 *
	 * io_cmd could be: ...
	 */
	if (argc != 4 || strncmp(argv[0], "io_cmd", strlen(argv[0])) ) {

		DMERR("[%s] Invalid command or argument number (need 4 args)", dss->name);
		return -EINVAL;
	}

	DMERR("[%s] No command currently implemented via message", dss->name);
	return -EINVAL;
}

/*----------------------------------------------------------------- */

/* Returns status information about the destripe dev... */

static int destripe_status(struct dm_target *ti,
			 status_type_t type, char *result, unsigned int maxlen)
{
	unsigned int sz = 0;
	struct destripe_set *dss = (struct destripe_set *) ti->private;

	switch (type) {
	case STATUSTYPE_INFO:
		DRSDEBUG("destripe_status STATUSTYPE_INFO...\n");
		DMEMIT("\ndestripe[%s] stripes=%u idx=%u"
				"chunk_size=%u chunk_size_shift=%d phys_size=%lu",
				dss->name, dss->destripes, dss->destripe_idx,
				dss->chunk_size, dss->chunk_size_shift,
				(unsigned long)dss->physical_size);
		DMEMIT("\ndestripe[%s] IO Count: TRD: %d ORD: %d TWR: %d OWR: %d", dss->name,
				atomic_read( &dss->read_ios_total ), atomic_read( &dss->read_ios_pending ),
				atomic_read( &dss->write_ios_total ), atomic_read( &dss->write_ios_pending) );
		break;

	case STATUSTYPE_TABLE:
		DRSDEBUG("destripe_status STATUSTYPE_TABLE...\n");
		DMEMIT("1 %llu %s %llu", (unsigned long long)dss->chunk_size,
				dss->destripe[0].dev->name,
				(unsigned long long)dss->destripe[0].physical_start);
		break;
	}

	return 0;
}

/*----------------------------------------------------------------- */

/*
 * An event is triggered whenever a drive
 * drops out of a destripe volume.
 */
static void trigger_event(struct work_struct *work)
{
	struct destripe_set *dss = container_of(work, struct destripe_set,
					   trigger_event);
	dm_table_event(dss->ti->table);
}

static inline struct destripe_set *alloc_ds_context(void)
{
	size_t len;

	if (dm_array_too_big(sizeof(struct destripe_set), sizeof(struct destripe), 1))
		return NULL;

	len = sizeof(struct destripe_set) + sizeof(struct destripe);

	return kmalloc(len, GFP_KERNEL);
}

/*-----------------------------------------------------------------
 * Target functions
 *---------------------------------------------------------------*/

/*
 * Construct a destripe (reverse stripe) mapping:
 *
 * Arguments: <number of stripes> <de-stripe index> <chunk size (sectors)> <...device arguments...>
 *
 */
static int destripe_ctr(struct dm_target *ti, unsigned int argc, char **argv)
{
	struct destripe_set *dss;
	struct mapped_device *dsd;
	uint32_t destripes, destripe_idx, chunk_size;
	unsigned long long start;
	char *end;
	char dummy;
	int r;


	DRSDEBUG_CALL("destripe_ctr called...\n");

	if (argc < 3) {
		ti->error = "Not enough arguments (need at least 3)";
		return -EINVAL;
	}

	destripes = simple_strtoul(argv[0], &end, 10);
	if ( !destripes || *end || destripes < 2 || destripes > 16 ) {
		ti->error = "Invalid stripe count (must be 2-16)";
		return -EINVAL;
	}

	destripe_idx = simple_strtoul(argv[1], &end, 10);
	if ( *end || destripe_idx >= destripes) {
		ti->error = "Invalid stripe index (must be 0 - stripes-1)";
		return -EINVAL;
	}

	chunk_size = simple_strtoul(argv[2], &end, 10);
	if ( *end || !chunk_size ) {
		ti->error = "Invalid chunk_size";
		return -EINVAL;
	}

	/*
	 * chunk_size is a power of two
	 */
	if (!is_power_of_2(chunk_size) ||
	    (chunk_size < (PAGE_SIZE >> SECTOR_SHIFT))) {
		ti->error = "Invalid chunk size";
		return -EINVAL;
	}

	/*
	 * chunk_size is a power of two
	 */
	if (!is_power_of_2(chunk_size) ||
	    (chunk_size < (PAGE_SIZE >> SECTOR_SHIFT))) {
		ti->error = "Invalid chunk size";
		return -EINVAL;
	}

	if (ti->len & (chunk_size - 1)) {
		ti->error = "Target length not divisible by chunk size";
		return -EINVAL;
	}

	/* We only need 1 output dev for destripe (2 dev args, 1 no of devs) */
	if (argc != 6) {
		ti->error = "Destripe needs 3 arguments and 1 destination device specified";
		return -EINVAL;
	}

	/* set maximum size of I/O submitted to a target to chunk (more will be split) */
	ti->split_io = chunk_size;

	if ( !(dss = alloc_ds_context()) ) {
		ti->error = "Memory allocation for destripe context failed";
		return -ENOMEM;
	}

	dsd = dm_table_get_md(ti->table);

	/* check the name of the device... this is actually the major:minor device
	 * name in the kernel and should not exceed 10 chars... */
	if ( strlen(dm_device_name(dsd)) >= DEVNAME_MAXLEN ) {
		ti->error = "Internal error: dm-device name too long!";
		kfree(dss);
		return -EINVAL;
	}
	/* copy the device name locally... */
	memset( dss->name, 0, DEVNAME_MAXLEN );
	memcpy( dss->name, dm_device_name(dsd), strlen( dm_device_name(dsd) ) );

	INIT_WORK(&dss->trigger_event, trigger_event);

	/* Set pointer to dm target; used in trigger_event */
	dss->ti = ti;
	dss->destripes = destripes;
	dss->destripe_idx = destripe_idx;
	dss->physical_size = ti->len * dss->destripes;

	/* check out include/linux/device-mapper.h for tuning more settings... */
	ti->num_flush_requests = 1;
	ti->num_discard_requests = 1;

	dss->chunk_size = chunk_size;
	if (chunk_size & (chunk_size - 1))
		dss->chunk_size_shift = -1;
	else
		dss->chunk_size_shift = __ffs(chunk_size);

	/*
	 * Get the destination device by parsing the <dev> <sector> pair
	 */
	argv += 4;

	if (sscanf(argv[1], "%llu%c", &start, &dummy) != 1) {
		ti->error = "Couldn't parse destripe destination device";
		kfree(dss);
		return -EINVAL;
	}

	if (dm_get_device(ti, argv[0],
			dm_table_get_mode(ti->table), &dss->destripe[0].dev)) {
		ti->error = "Invalid destripe destination device";
		//dm_put_device(ti, dss->destripe[0].dev);
		kfree(dss);
		return -ENXIO;
	}

	/* check the output device size... it should be at least destripes * ti->len,
	 * else we cannot support the required de-striping ! */
	r = ioctl_by_bdev( dss->destripe[0].dev->bdev, BLKGETSIZE, (sector_t) &dss->destripe[0].physical_secs );
	if (r) {
		ti->error = "Error reading physical device size via BLKGETSIZE ioctl()";
fail_ctr_invalid:
		kfree(dss);
		return -EINVAL;
	}

	/* target length must be at least destripes * ti->len to support target address space... */
	if (dss->destripe[0].physical_secs < dss->physical_size ) {
		ti->error = "Physical device capacity not enough to support destripes on requested target length";
		goto fail_ctr_invalid;
	}

	if (dss->destripe[0].physical_secs > dss->physical_size )
		DMWARN("[%s] WARNING: Larger physical space than required! DeStripe using only %lu of %lu sectors.",
				dss->name, (unsigned long) dss->physical_size,
				(unsigned long) dss->destripe[0].physical_secs );

	dss->destripe[0].physical_start = start;

	atomic_set(&(dss->destripe[0].error_count), 0);

	/* initialize IO counters... */
	atomic_set( &dss->read_ios_total, 0 );
	atomic_set( &dss->read_ios_pending, 0 );
	atomic_set( &dss->write_ios_total, 0 );
	atomic_set( &dss->write_ios_pending, 0 );

	ti->private = dss;

	DMINFO("Device %s INIT OK: len=%lu destripes=%u idx:%u phys_size=%lu "
	   		"chunk_size=%u ck_sz_shift=%d",
			dss->name, ti->len, dss->destripes, dss->destripe_idx,
			(unsigned long)dss->physical_size, dss->chunk_size, dss->chunk_size_shift);

	return 0;
}

/*----------------------------------------------------------------- */

static void destripe_dtr(struct dm_target *ti)
{
	struct destripe_set *dss = (struct destripe_set *) ti->private;

	DRSDEBUG_CALL("destripe_dtr called...\n");
	DMWARN("[%s] DeStripe Device EXIT.", dss->name);

	dm_put_device(ti, dss->destripe[0].dev);

	flush_work_sync(&dss->trigger_event);
	kfree(dss);
}

static int destripe_iterate_devices(struct dm_target *ti,
				  iterate_devices_callout_fn fn, void *data)
{
	struct destripe_set *dss = (struct destripe_set *) ti->private;

	DRSDEBUG_CALL("destripe_iterate_devices called...\n");

	/* we only have 1 device, so no iteration... */
	return fn(ti, dss->destripe[0].dev, dss->destripe[0].physical_start, ti->len, data);
}

/*----------------------------------------------------------------- */

static void destripe_io_hints(struct dm_target *ti,
			    struct queue_limits *limits)
{
	struct destripe_set *dss = ti->private;
	unsigned chunk_size = dss->chunk_size << SECTOR_SHIFT;

	blk_limits_io_min(limits, chunk_size);
	blk_limits_io_opt(limits, chunk_size);
}

/*----------------------------------------------------------------- */

static int destripe_merge(struct dm_target *ti, struct bvec_merge_data *bvm,
			struct bio_vec *biovec, int max_size)
{
	struct destripe_set *dss = ti->private;
	sector_t bvm_sector = bvm->bi_sector;
	struct request_queue *q;

	destripe_map_sector(dss, bvm_sector, &bvm_sector);

	q = bdev_get_queue(dss->destripe[0].dev->bdev);
	if (!q->merge_bvec_fn)
		return max_size;

	bvm->bi_bdev = dss->destripe[0].dev->bdev;
	bvm->bi_sector = dss->destripe[0].physical_start + bvm_sector;

	return min(max_size, q->merge_bvec_fn(q, bvm, biovec));
}

/*----------------------------------------------------------------- */

static struct target_type destripe_target = {
	.name	 = "destripe",
	.version = {1, 0, 0},
	.module	 = THIS_MODULE,
	.ctr	 = destripe_ctr,	/* Contructor function */
	.dtr	 = destripe_dtr,	/* Destructor function */
	.map	 = destripe_map,	/* Map function */
	.end_io	 = destripe_end_io,	/* End_io function */
	.presuspend = destripe_presuspend,	/* Pre-suspend function */
	.postsuspend = destripe_postsuspend,	/* Post-suspend function */
	.resume	 = destripe_resume,	/* Resume function */
	.message = destripe_message,	/* Message function */
	.status	 = destripe_status,	/* Status function */
	.iterate_devices = destripe_iterate_devices,
	.io_hints = destripe_io_hints,
	.merge  = destripe_merge,
};


static int __init dm_destripe_init(void)
{
	int r = -ENOMEM;

	r = dm_register_target(&destripe_target);
	if (r < 0) {
		DMERR("[%s] Failed to register destripe target", destripe_target.name);
		return r;
	}

	printk(KERN_INFO "dm-destripe L34 [Build: %s %s]: Loaded OK.\n", __DATE__, __TIME__);

	return r;
}

static void __exit dm_destripe_exit(void)
{
	printk(KERN_INFO "dm-destripe L34 [Build: %s %s]: Exiting.\n", __DATE__, __TIME__);

	dm_unregister_target(&destripe_target);
}

/* Module hooks */
module_init(dm_destripe_init);
module_exit(dm_destripe_exit);

MODULE_AUTHOR("Michail Flouris <michail.flouris at onapp.com>");
MODULE_DESCRIPTION(
	"(C) Copyright OnApp Ltd. 2012-2013  All Rights Reserved.\n"
	DM_NAME " destripe target for reversing stripe-mapped data to a single volume "
);
MODULE_LICENSE("GPL");
