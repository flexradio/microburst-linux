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
#


SYSLINK_INSTALL_DIR = $(CURDIR)

# List of supported devices (choose one): OMAP3530, OMAPL1XX, TI816X, TI814X
#    TI813X, TI811X
#
DEVICE = TI816X

# Master core (GPP) OS type (choose one): Linux, Qnx, Bios
#
GPPOS = Linux

# SysLink Linux driver DEBUG/TRACE options (choose one): 0 or 1
#
# Note that these options are only used by the Linux HLOS driver.  RTOS
# side libraries, Qnx libraries, and user mode Linux libraries are always
# built in both debug and release profiles, and users can switch between
# them when linking their executable (without rebuilding SysLink).
#
# Enabling SYSLINK_BUILD_DEBUG enables runtime assertion checking and makes
# some internal 'static' methods and objects global (so you can set
# breakpoints on them and/or inspect them in a debugger).
#
# Enabling SYSLINK_TRACE_ENABLE enables tracing throughout the SysLink
# code base.  This is especially helpful during development, but may not
# be necessary at production.
#
SYSLINK_BUILD_DEBUG=0
SYSLINK_TRACE_ENABLE=0

# SysLink OPTIMIZE option (choose one): 0 or 1
#
# Enabling SYSLINK_BUILD_OPTIMIZE removes many runtime API checks.  As a
# result, if the user uses SysLink in an unsupported way, rather than getting
# a failing return value, the system may crash or become unstable.
# SYSLINK_BUILD_OPTIMIZED is intended to be used in a closed system where all
# code paths are known, and error conditions are ensured never to occur - so
# it's not for everyone.
#
# Note that SYSLINK_BUILD_OPTIMIZE removes these checks throughout the
# SysLink stack - that includes RTOS, user, and driver layers if applicable
# to your environment.
#
SYSLINK_BUILD_OPTIMIZE=1

# SysLink HLOS driver Notify options (choose one): NOTIFYDRIVERSHM,
#    NOTIFYDRIVERCIRC
#
SYSLINK_NOTIFYDRIVER=NOTIFYDRIVERSHM

# SysLink HLOS driver MessageQ Transport options (choose one): TRANSPORTSHM,
#    TRANSPORTSHMNOTIFY, TRANSPORTSHMCIRC
#
SYSLINK_TRANSPORT=TRANSPORTSHM

# Set SDK type when building for a TI SDK kit (choose one): EZSDK or NONE
#
SDK = EZSDK

# Define root dir to install SysLink driver and samples for target file-system
#
EXEC_DIR = /nfsroots/microburst

# Define file format for loader and slave executables (choose one): COFF, ELF
#
LOADER = ELF

# Optional: recommended to install all dependent components in one folder.
#
DEPOT = 

# Define the product variables for the device you will be using.
#
######## For OMAP3530 device ########
ifeq ("$(DEVICE)","OMAP3530")
LINUXKERNEL             = $(DEPOT)/_your_linux_kernel_install_
CGT_ARM_INSTALL_DIR     = $(DEPOT)/_your_arm_code_gen_install_
CGT_ARM_PREFIX          = $(CGT_ARM_INSTALL_DIR)/bin/arm-none-linux-gnueabi-
IPC_INSTALL_DIR         = $(DEPOT)/_your_ipc_install_
BIOS_INSTALL_DIR        = $(DEPOT)/_your_bios_install_
XDC_INSTALL_DIR         = $(DEPOT)/_your_xdctools_install_

# If LOADER=ELF then below elf tools path is required else set C64P path
ifeq ("$(LOADER)","ELF")
CGT_C64P_ELF_INSTALL_DIR= $(DEPOT)/_your_c64pelf_code_gen_install_
else
CGT_C64P_INSTALL_DIR    = $(DEPOT)/_your_c64p_code_gen_install_
endif

######## For TI816X device ########
else ifeq ("$(DEVICE)","TI816X")
LINUXKERNEL             = $(DEPOT)/src/flex/microburst-linux
CGT_ARM_INSTALL_DIR     = $(DEPOT)/opt/ti/CodeSourcery/Sourcery_G++_Lite
CGT_ARM_PREFIX          = $(CGT_ARM_INSTALL_DIR)/bin/arm-none-linux-gnueabi-
IPC_INSTALL_DIR         = $(DEPOT)/opt/ti/ipc_1_25_00_04
BIOS_INSTALL_DIR        = $(DEPOT)/opt/ti/bios_6_34_01_14
XDC_INSTALL_DIR         = $(DEPOT)/opt/ti/xdctools_3_24_03_33
CGT_C674_ELF_INSTALL_DIR= $(DEPOT)/opt/ti/C6000CGT7.2.7


# SYS/BIOS timer frequency (ti.sysbios.timers.dmtimer.Timer.intFreq)
TI81XXDSP_DMTIMER_FREQ  = 32768

# If SDK=NONE then below tools path is required
ifeq ("$(SDK)","NONE")
CGT_M3_ELF_INSTALL_DIR  = $(DEPOT)/_your_m3elf_code_gen_install_
endif

######## For TI814X device ########
else ifeq ("$(DEVICE)","TI814X")
# Set one of the following OS variables
LINUXKERNEL             = $(DEPOT)/_your_linux_kernel_install_
QNX_INSTALL_DIR         = $(DEPOT)/_your_qnx_install_

CGT_ARM_INSTALL_DIR     = $(DEPOT)/_your_arm_code_gen_install_
CGT_ARM_PREFIX          = $(CGT_ARM_INSTALL_DIR)/bin/arm-none-linux-gnueabi-
IPC_INSTALL_DIR         = $(DEPOT)/_your_ipc_install_
BIOS_INSTALL_DIR        = $(DEPOT)/_your_bios_install_
XDC_INSTALL_DIR         = $(DEPOT)/_your_xdctools_install_
CGT_C674_ELF_INSTALL_DIR= $(DEPOT)/_your_c674elf_code_gen_install_

# SYS/BIOS timer frequency (ti.sysbios.timers.dmtimer.Timer.intFreq)
TI81XXDSP_DMTIMER_FREQ  = 20000000

# If SDK=NONE then below tools path is required
ifeq ("$(SDK)","NONE")
CGT_M3_ELF_INSTALL_DIR  = $(DEPOT)/_your_m3elf_code_gen_install_
endif

# If GPPOS=Bios then below tools path is required
ifeq ("$(GPPOS)","Bios")
CGT_A8_ELF_INSTALL_DIR  = $(DEPOT)/_your_a8elf_code_gen_install_
endif

######## For TI813X device ########
else ifeq ("$(DEVICE)","TI813X")
LINUXKERNEL             = $(DEPOT)/_your_linux_kernel_install_
CGT_ARM_INSTALL_DIR     = $(DEPOT)/_your_arm_code_gen_install_
CGT_ARM_PREFIX          = $(CGT_ARM_INSTALL_DIR)/bin/arm-none-linux-gnueabi-
IPC_INSTALL_DIR         = $(DEPOT)/_your_ipc_install_
BIOS_INSTALL_DIR        = $(DEPOT)/_your_bios_install_
XDC_INSTALL_DIR         = $(DEPOT)/_your_xdctools_install_

# If SDK=NONE then below tools path is required
ifeq ("$(SDK)","NONE")
CGT_M3_ELF_INSTALL_DIR  = $(DEPOT)/_your_m3elf_code_gen_install_
endif

######## For TI811X device ########
else ifeq ("$(DEVICE)","TI811X")
# Set one of the following OS variables
LINUXKERNEL             = $(DEPOT)/_your_linux_kernel_install_
QNX_INSTALL_DIR         = $(DEPOT)/_your_qnx_install_

CGT_ARM_INSTALL_DIR     = $(DEPOT)/_your_arm_code_gen_install_
CGT_ARM_PREFIX          = $(CGT_ARM_INSTALL_DIR)/bin/arm-none-linux-gnueabi-
IPC_INSTALL_DIR         = $(DEPOT)/_your_ipc_install_
BIOS_INSTALL_DIR        = $(DEPOT)/_your_bios_install_
XDC_INSTALL_DIR         = $(DEPOT)/_your_xdctools_install_
CGT_C674_ELF_INSTALL_DIR= $(DEPOT)/_your_c674elf_code_gen_install_

# SYS/BIOS timer frequency (ti.sysbios.timers.dmtimer.Timer.intFreq)
TI81XXDSP_DMTIMER_FREQ  = 20000000

# If SDK=NONE then below tools path is required
ifeq ("$(SDK)","NONE")
CGT_M3_ELF_INSTALL_DIR  = $(DEPOT)/_your_m3elf_code_gen_install_
endif

######## For OMAPL1XX device ########
else ifeq ("$(DEVICE)","OMAPL1XX")
LINUXKERNEL             = $(DEPOT)/_your_linux_kernel_install_
CGT_ARM_INSTALL_DIR     = $(DEPOT)/_your_arm_code_gen_install_
CGT_ARM_PREFIX          = $(CGT_ARM_INSTALL_DIR)/bin/arm-none-linux-gnueabi-
IPC_INSTALL_DIR         = $(DEPOT)/_your_ipc_install_
BIOS_INSTALL_DIR        = $(DEPOT)/_your_bios_install_
XDC_INSTALL_DIR         = $(DEPOT)/_your_xdctools_install_

# If LOADER=ELF then below elf tools path is required else set C674 path
ifeq ("$(LOADER)","ELF")
CGT_C674_ELF_INSTALL_DIR= $(DEPOT)/_your_c674elf_code_gen_install_
else
CGT_C674_INSTALL_DIR= $(DEPOT)/_your_c674_code_gen_install_
endif
######## End of device specific variables ########

else ifeq ($(MAKECMDGOALS), clean)
else ifeq ($(MAKECMDGOALS), clobber)
else ifeq ($(MAKECMDGOALS), .show-products)
else ifeq ($(MAKECMDGOALS), help)
else
    $(error DEVICE is set to "$(DEVICE)", which is invalid. Set this in <SysLink Install>/products.mak. Refer to the SysLink Install Guide for more information)
endif

# Use this goal to print your product variables.
.show-products:
	@echo "DEPOT                    = $(DEPOT)"
	@echo "DEVICE                   = $(DEVICE)"
	@echo "GPPOS                    = $(GPPOS)"
	@echo "SDK                      = $(SDK)"
	@echo "TI81XXDSP_DMTIMER_FREQ   = $(TI81XXDSP_DMTIMER_FREQ)"
	@echo "SYSLINK_BUILD_DEBUG      = $(SYSLINK_BUILD_DEBUG)"
	@echo "SYSLINK_BUILD_OPTIMIZE   = $(SYSLINK_BUILD_OPTIMIZE)"
	@echo "SYSLINK_TRACE_ENABLE     = $(SYSLINK_TRACE_ENABLE)"
	@echo "LOADER                   = $(LOADER)"
	@echo "SYSLINK_INSTALL_DIR      = $(SYSLINK_INSTALL_DIR)"
	@echo "IPC_INSTALL_DIR          = $(IPC_INSTALL_DIR)"
	@echo "BIOS_INSTALL_DIR         = $(BIOS_INSTALL_DIR)"
	@echo "XDC_INSTALL_DIR          = $(XDC_INSTALL_DIR)"
	@echo "LINUXKERNEL              = $(LINUXKERNEL)"
	@echo "QNX_INSTALL_DIR          = $(QNX_INSTALL_DIR)"
	@echo "CGT_ARM_PREFIX           = $(CGT_ARM_PREFIX)"
	@echo "CGT_C64P_INSTALL_DIR     = $(CGT_C64P_INSTALL_DIR)"
	@echo "CGT_C64P_ELF_INSTALL_DIR = $(CGT_C64P_INSTALL_DIR)"
	@echo "CGT_C674_INSTALL_DIR     = $(CGT_C674_INSTALL_DIR)"
	@echo "CGT_C674_ELF_INSTALL_DIR = $(CGT_C674_ELF_INSTALL_DIR)"
	@echo "CGT_M3_ELF_INSTALL_DIR   = $(CGT_M3_ELF_INSTALL_DIR)"
	@echo "CGT_A8_ELF_INSTALL_DIR   = $(CGT_A8_ELF_INSTALL_DIR)"
	@echo "EXEC_DIR                 = $(EXEC_DIR)"
