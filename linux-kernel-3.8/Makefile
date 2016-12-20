
#choose your build platform
DMO_PLATFORM=x86_64
#DMO_PLATFORM=i686

# Any extra cflags?
ifeq ($(DMO_PLATFORM),x86_64)
#EXTRA_CFLAGS += -mhard-float -msse -mmmx -m3dnow # -mtune=opteron
# Set to 0 or 1 if we compile on 64-bit architecture
IS_x86_64_ARCH ?= $(shell uname -m | grep 64 | wc -l )
endif
ifeq ($(DMO_PLATFORM),i686)
EXTRA_CFLAGS += -mtune=i686
IS_x86_64_ARCH ?= $(shell uname -m | grep 64 | wc -l )
endif

MAJ_KERNEL_VERSION ?= $(shell uname -r | cut -c1-3)
ifeq ($(IS_x86_64_ARCH),0)
EXTRA_CFLAGS += -DNOT_64_ARCH
endif

# Add heavy debugging??
#DFLAGS = -g -g3 -ggdb
#EXTRA_CFLAGS += $(DFLAGS)
#$(warning CAUTION -> DEBUG flags enabled!)

# Directory for Module installation
KERNEL_VERSION ?= $(shell uname -r)
BASEKERNDIR := 
MODDIR=/lib/modules/$(KERNEL_VERSION)/kernel/drivers/md/

DMOBJS = # other files except dm-destripe.o
KMODNAME = dm-destripe

obj-m += $(KMODNAME).o
dm-destripe-objs += $(DMOBJS)

# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
	obj-m += $(KMODNAME).o
	dm-destripe-objs += $(DMOBJS)

# Otherwise we were called directly from the command
# line; invoke the kernel build system.
else
	#KERNELDIR ?= $(BASEKERNDIR)/lib/modules/$(shell uname -r)/build
	KERNELDIR ?= $(BASEKERNDIR)/lib/modules/$(KERNEL_VERSION)/build
	#PWD := $(shell pwd)
	#SUBDIRS=$(PWD)/kern_code
endif

.PHONY: all kern38vm dm_destripe_mod ins lsm rmm install clean wc
all: dm_destripe_mod # tags types.vim

kern38vm: KERNELDIR = /mnt/kernel/linux-3.8.5/
kern38vm: all

dm_destripe_mod:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

ins:
	/sbin/insmod $(KMODNAME).ko
	@/sbin/lsmod | grep destripe

lsm:
	@/sbin/lsmod | grep destripe

rmm:
	/sbin/rmmod $(KMODNAME).ko

install:
	cp $(KMODNAME).ko $(MODDIR)
	/sbin/depmod -a $(KERNEL_VERSION)

clean:
	\rm -rf *.o .*.o.d .depend *.ko .*.cmd *.mod.c .tmp* Module.markers Module.symvers
	\rm -rf modules.order $(KMODNAME).ko.unsigned
	\rm -rf centos-kernel
	\rm -f types.vim tags

wc:
	@echo -n "Code lines (excl. blank lines): "
	@cat *.[ch] | grep -v "^$$" | grep -v "^[ 	]*$$" | wc -l

dm-destripe.o: dm-destripe.h dm-destripe.c dm.h dm-bio-record.h

tags:: *.[ch]
	@\rm -f tags
	@ctags -R --languages=c

types.vim: *.[ch]
	@echo "==> Updating tags !"
	@\rm -f $@
	@ctags -R --c-types=+gstu -o- *.[ch] | awk '{printf("%s\n", $$1)}' | uniq | sort | \
	awk 'BEGIN{printf("syntax keyword myTypes\t")} {printf("%s ", $$1)} END{print ""}' > $@
	@ctags -R --c-types=+cd -o- *.[ch] | awk '{printf("%s\n", $$1)}' | uniq | sort | \
	awk 'BEGIN{printf("syntax keyword myDefines\t")} {printf("%s ", $$1)} END{print ""}' >> $@
	@ctags -R --c-types=+v-gstucd -o- *.[ch] | awk '{printf("%s\n", $$1)}' | uniq | sort | \
	awk 'BEGIN{printf("syntax keyword myVariables\t")} {printf("%s ", $$1)} END{print ""}' >> $@

