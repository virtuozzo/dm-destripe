#
# Makefile for the Device mapper destripe (i.e. reverse striping) driver.
#
# Copyright (C) 2013 OnApp Ltd.
# Author: (C) 2013 Michail Flouris <michail.flouris@onapp.com>

# Add debugging??
#DFLAGS = -g -g3 -ggdb
#EXTRA_CFLAGS += $(DFLAGS)

# try to detect the Linux distro type
ifneq ($(wildcard /etc/redhat-release),) 
    LINUX_TYPE := Redhat
else 
	ifneq ($(wildcard /etc/debian_version),) 
	    LINUX_TYPE := Debian
		ifneq ($(wildcard /etc/lsb-release),) 
		    LINUX_TYPE = Ubuntu
		else 
		    LINUX_TYPE := Unknown
		endif
	else 
		ifneq ($(wildcard /etc/SuSE-release),) 
		    LINUX_TYPE := SuSE
		else
			LINUX_TYPE := Unknown
		endif
	endif
endif

KERNEL_VERSION ?= $(shell uname -r)
KERN_MAJOR_VER := $(shell echo '$(KERNEL_VERSION)' | cut -d '.' -f 1)
KERN_MINOR_VER := $(shell echo '$(KERNEL_VERSION)' | cut -d '.' -f 2)

# 0 or 1 if we compile on 64-bit architecture
IS_64_ARCH := $(shell uname -m | grep 64 | wc -l )

# Directory for building module
BUILDDIR :=

ifeq ($(LINUX_TYPE),Redhat)
CENTOS_VERSION := $(shell cat /etc/redhat-release | grep 'CentOS' | cut -c 16 )

# Check which version of centos to build on... 
ifeq ($(CENTOS_VERSION),)
	BUILDDIR := $(error CentOS not found! Current makefiles support only CentOS. Aborting!)
endif
ifeq ($(CENTOS_VERSION),5)
	BUILDDIR := $(error dm-destripe built not supported on Centos 5 or lower version! Aborting!)
endif
ifeq ($(CENTOS_VERSION),6)
	ifeq ($(KERN_MAJOR_VER),3)
		ifeq ($(shell echo '$(KERN_MINOR_VER) < 13' | bc), 1)
			BUILDDIR := "linux-kernel-3.8"
		else
			BUILDDIR := "linux-kernel-3.13"
		endif
		ifeq ($(KERN_MINOR_VER),4)
			BUILDDIR := "linux-kernel-3.4"
		endif
	else
		BUILDDIR := $(error Build not supported on 2.x kernel versions!)
	endif
endif
else
	# we go by kernel version number in here...
	ifeq ($(LINUX_TYPE),Ubuntu)
		ifeq ($(KERN_MAJOR_VER),3)
			ifeq ($(shell echo '$(KERN_MINOR_VER) < 13' | bc), 1)
				BUILDDIR := "linux-kernel-3.8"
			else
				BUILDDIR := "linux-kernel-3.13"
			endif
		else
			BUILDDIR := $(error Ubuntu Kernel version < 3! Change into a 2.x kernel version subdir and 'make' in there!)
		endif
	else
		BUILDDIR := $(error Unsupported linux distro! Change into a kernel version subdir and 'make' in there!)
	endif
endif

# Do we support the build on the current Linux distro & kernel version?
ifeq ($(BUILDDIR),)
	BUILDDIR := $(error dm-destripe built not supported on current distro/kernel version! Aborting!)
endif

BINS= qhash_test

.PHONY: all destripe_mod ins lsm rmm test install clean wc
.PHONY: utils 

all: destripe_mod # utils tags types.vim

destripe_mod:
	@echo Detected LINUX_TYPE=\"${LINUX_TYPE}\"
	@echo Building on CENTOS_VERSION=\"$(CENTOS_VERSION)\"
	@echo BUILDDIR= $(BUILDDIR)
	(cd $(BUILDDIR) ; $(MAKE))

utils::
	(cd utils ; make $(TARGET))

ins:
	(cd $(BUILDDIR) ; $(MAKE) $@)

lsm:
	(cd $(BUILDDIR) ; $(MAKE) $@)

rmm:
	(cd $(BUILDDIR) ; $(MAKE) $@)

test:
	(cd $(BUILDDIR) ; $(MAKE) $@)

install:
	(cd $(BUILDDIR) ; $(MAKE) $@)
	(cd utils ; make $@)

clean:
	(cd $(BUILDDIR) ; $(MAKE) $@)
	(cd utils ; make $@)
	\rm -rf *.o .*.o.d .depend *.ko .*.cmd *.mod.c .tmp* Module.markers Module.symvers
	\rm -f types.vim tags $(BINS)

wc:
	(cd $(BUILDDIR) ; $(MAKE) $@)
	@echo -n "Code lines (excl. blank lines): "
	@cat *.[ch] utils/*.[ch] | grep -v "^$$" | grep -v "^[ 	]*$$" | wc -l

tags:: *.[ch]
	(cd $(BUILDDIR) ; $(MAKE) $@)
	@\rm -f tags
	@ctags -R --languages=c

types.vim: *.[ch]
	(cd $(BUILDDIR) ; $(MAKE) $@)
	@echo "==> Updating tags !"
	@\rm -f $@
	@ctags -R --c-types=+gstu -o- *.[ch] utils/*.[ch] | awk '{printf("%s\n", $$1)}' | uniq | sort | \
	awk 'BEGIN{printf("syntax keyword myTypes\t")} {printf("%s ", $$1)} END{print ""}' > $@
	@ctags -R --c-types=+cd -o- *.[ch] utils/*.[ch] | awk '{printf("%s\n", $$1)}' | uniq | sort | \
	awk 'BEGIN{printf("syntax keyword myDefines\t")} {printf("%s ", $$1)} END{print ""}' >> $@
	@ctags -R --c-types=+v-gstucd -o- *.[ch] utils/*.[ch] | awk '{printf("%s\n", $$1)}' | uniq | sort | \
	awk 'BEGIN{printf("syntax keyword myVariables\t")} {printf("%s ", $$1)} END{print ""}' >> $@

