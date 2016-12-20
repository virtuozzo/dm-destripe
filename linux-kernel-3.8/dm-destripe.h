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

/* --------------------------------------------------------------
 *   CONFIGURABLE OPTIONS
 * -------------------------------------------------------------- */

/* --------------------------------------------------------------
 *   NON-CONFIGURABLE OPTIONS - FRAGILE !
 * -------------------------------------------------------------- */

#ifndef DEBUGMSG
//#define DEBUGMSG	/* CAUTION: enables VERBOSE debugging messages, decreases performance */
#undef DEBUGMSG
#endif
#ifndef ASSERTS
#define ASSERTS		/* enables assertions, may decrease performance a little */
#endif

/* shortcut for kernel printing... */
#define kprint(x...) printk( KERN_ALERT x )
#define NOOP	do {} while (0)

#ifdef DEBUGMSG
#define DRSDEBUG(x...) printk( KERN_ALERT x )
#define DRSDEBUG_CALL(x...) printk( KERN_ALERT x )
#else
#define DRSDEBUG(x...)	NOOP /* disabled */
#define DRSDEBUG_CALL(x...) NOOP
#endif

/* CAUTION: use for ultra-targeted debugging or ultra-verbosity */
#define DRSDEBUGX(x...) NOOP
//#define DRSDEBUGX(x...) printk( KERN_ALERT x )

/* CAUTION: assert() and assert_bug() MUST BE USED ONLY FOR DEBUGGING CHECKS !! */
#ifdef ASSERTS
#define assert(x) if (unlikely(!(x))) { printk( KERN_ALERT "ASSERT: %s failed @ %s(): line %d\n", \
						#x, __FUNCTION__,__LINE__); }

#define assert_return(x,r) if (unlikely(!(x))) { \
        printk( KERN_ALERT "RETURN ASSERT: %s failed @ %s(): line %d\n", #x, __FUNCTION__,__LINE__); \
        return r; }

/* CAUTION: This is a show-stopper... use carefully!! */
#define assert_bug(x) if (unlikely(!(x))) { \
        printk( KERN_ALERT "$$$ BUG ASSERT: %s failed @ %s(): line %d\n", #x, __FUNCTION__,__LINE__); \
        * ((char *) 0) = 0; }
//==============================================
#else
/* CAUTION: disabling ALL assertions... */
#define assert(x)		NOOP
#define assert_bug(x)	NOOP
//==============================================
#endif

#undef DISABLE_UNPLUGS /* enable only for debugging... */

#define MAX_ERR_MESSAGES 20

/*-----------------------------------------------------------------
 * Destripe (reverse stripe) state structures.
 *---------------------------------------------------------------*/

struct destripe {
	struct dm_dev *dev;
	sector_t physical_start;
	sector_t physical_secs;

	atomic_t error_count;
};

#define DEVNAME_MAXLEN 16

struct destripe_set {
	uint32_t destripes;
	uint32_t destripe_idx;

	/* The physical size of this target == target len * num. of de-stripes */
	sector_t physical_size;

	uint32_t chunk_size;
	int chunk_size_shift;

	/* Needed for handling events */
	struct dm_target *ti;

	atomic_t supress_err_messages;		/* Counter/flag of printing I/O error messages. */

	atomic_t suspend; /* flag set for suspend... */

	/* Total & Outstanding I/O counters */
	atomic_t read_ios_total;
	atomic_t read_ios_pending;
	atomic_t write_ios_total;
	atomic_t write_ios_pending;

	/* Work struct used for triggering events*/
	struct work_struct trigger_event;

	char name[ DEVNAME_MAXLEN ];

	struct destripe destripe[0];
};

