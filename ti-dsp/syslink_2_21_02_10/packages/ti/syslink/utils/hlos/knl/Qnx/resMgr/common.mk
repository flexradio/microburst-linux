#   
#   @file   common.mk
#
#   @brief  QNX common makefile
#
#
#   ============================================================================
#
#   Copyright (c) 2008-2012, Texas Instruments Incorporated
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#   
#   *  Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#   
#   *  Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in the
#      documentation and/or other materials provided with the distribution.
#   
#   *  Neither the name of Texas Instruments Incorporated nor the names of
#      its contributors may be used to endorse or promote products derived
#      from this software without specific prior written permission.
#   
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
#   THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
#   PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
#   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
#   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
#   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
#   OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
#   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
#   OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
#   EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#   Contact information for paper mail:
#   Texas Instruments
#   Post Office Box 655303
#   Dallas, Texas 75265
#   Contact information: 
#   http://www-k.ext.ti.com/sc/technical-support/product-information-centers.htm?
#   DCMP=TIHomeTracking&HQS=Other+OT+home_d_contact
#   ============================================================================
#   
ifndef QCONFIG
QCONFIG=qconfig.mk
endif
include $(QCONFIG)

NAME = syslink_drv

define PINFO
PINFO DESCRIPTION=SysLink QNX Resource Manager
endef

# don't install the binaries, they are copied in base makefile
INSTALLDIR = /dev/null

# source path
EXTRA_SRCVPATH += \
        $(SYSLINK_ROOT)/ti/syslink/ipc/hlos/knl/Qnx \
        $(SYSLINK_ROOT)/ti/syslink \
        $(SYSLINK_ROOT)/ti/syslink/procMgr/hlos/knl/Qnx \
        $(SYSLINK_ROOT)/ti/syslink/utils/hlos/knl/Qnx

EXCLUDE_OBJS = RingIO.o RingIOShm.o RingIO_qnx.o RingIOShm_qnx.o

# include path
EXTRA_INCVPATH = \
        $(SYSLINK_ROOT) \
        $(subst +, ,$(SYSLINK_PKGPATH))

include $(MKFILES_ROOT)/qtargets.mk
OPTIMIZE__gcc=$(OPTIMIZE_NONE_gcc)

ifeq ("$(SYSLINK_PROFILE)","DEBUG")
CCFLAGS += -DDEBUG -DSYSLINK_BUILD_DEBUG -DSYSLINK_TRACE_ENABLE
endif

ifeq ("$(SYSLINK_BUILD_OPTIMIZE)", "1")
CCFLAGS += -DSYSLINK_BUILD_OPTIMIZE
endif

ifeq ("$(SYSLINK_PLATFORM)", "TI81XX")
CCFLAGS += -DSYSLINK_PLATFORM_TI81XX

ifeq ("$(SYSLINK_VARIANT)", "TI814X")
BIN_DEVICE = ti814x
else ifeq ("$(SYSLINK_VARIANT)", "TI811X")
BIN_DEVICE = ti811x
else
$(error SYSLINK_VARIANT must be defined)
endif

endif

CCFLAGS += -DSYSLINK_BUILDOS_QNX -DSYSLINK_BUILD_HLOS -DUSE_SYSLINK_NOTIFY

LDFLAGS += -M

# Resource Manager library
ifeq ("$(SYSLINK_PROFILE)","DEBUG")
EXTRA_LIBVPATH += \
        $(SYSLINK_ROOT)/ti/syslink/utils/hlos/knl/Qnx/resMgr_lib/arm/a.g.le.v7

LIBS += syslinkmgr_g
else
EXTRA_LIBVPATH += \
        $(SYSLINK_ROOT)/ti/syslink/utils/hlos/knl/Qnx/resMgr_lib/arm/a.le.v7

LIBS += syslinkmgr
endif

# Cache library
EXTRA_LIBVPATH += \
        $(QNX_INSTALL_DIR)/target/qnx6/armle-v7/usr/lib
LIBS += cache
