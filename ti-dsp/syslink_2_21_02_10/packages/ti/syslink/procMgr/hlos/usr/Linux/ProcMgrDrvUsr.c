/*
 *  @file   ProcMgrDrvUsr.c
 *
 *  @brief      User-side OS-specific implementation of ProcMgr driver for Linux
 *
 *
 *  ============================================================================
 *
 *  Copyright (c) 2008-2012, Texas Instruments Incorporated
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  
 *  *  Neither the name of Texas Instruments Incorporated nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *  
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 *  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 *  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 *  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 *  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *  Contact information for paper mail:
 *  Texas Instruments
 *  Post Office Box 655303
 *  Dallas, Texas 75265
 *  Contact information: 
 *  http://www-k.ext.ti.com/sc/technical-support/product-information-centers.htm?
 *  DCMP=TIHomeTracking&HQS=Other+OT+home_d_contact
 *  ============================================================================
 *  
 */



/* Linux specific header files */
#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

/* Standard headers */
#include <ti/syslink/Std.h>

/* OSAL & Utils headers */
#include <ti/syslink/utils/Trace.h>
#include <ti/syslink/utils/Memory.h>
#include <ti/syslink/utils/Dev.h>
#include <ti/syslink/inc/usr/ProcMgrDrvUsr.h>

/* Module headers */
#include <ti/syslink/inc/ProcMgrDrvDefs.h>


/** ============================================================================
 *  Macros and types
 *  ============================================================================
 */
/*!
 *  @brief  Driver name for ProcMgr.
 */
#define ProcMgr_DRIVER_NAME         "/dev/syslinkipc_ProcMgr"


/** ============================================================================
 *  Globals
 *  ============================================================================
 */
/*!
 *  @brief  Driver handle for ProcMgr in this process.
 */
static Int32 ProcMgrDrvUsr_handle = -1;

/*!
 *  @brief  Reference count for the driver handle.
 */
static UInt32 ProcMgrDrvUsr_refCount = 0;


/** ============================================================================
 *  Functions
 *  ============================================================================
 */
/*!
 *  @brief  Function to open the ProcMgr driver.
 *
 *  @sa     ProcMgrDrvUsr_close
 */
Int
ProcMgrDrvUsr_open (Void)
{
    Int status      = ProcMgr_S_SUCCESS;
    int osStatus    = 0;

    GT_0trace (curTrace, GT_ENTER, "ProcMgrDrvUsr_open");

    if (ProcMgrDrvUsr_refCount == 0) {
        /* TBD: Protection for refCount. */
        ProcMgrDrvUsr_refCount++;

        ProcMgrDrvUsr_handle = Dev_pollOpen (ProcMgr_DRIVER_NAME, O_SYNC | O_RDWR);
        if (ProcMgrDrvUsr_handle < 0) {
            perror ("ProcMgr driver open: " ProcMgr_DRIVER_NAME);
            /*! @retval ProcMgr_E_OSFAILURE Failed to open ProcMgr driver with
                        OS */
            status = ProcMgr_E_OSFAILURE;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "ProcMgrDrvUsr_open",
                                 status,
                                 "Failed to open ProcMgr driver with OS!");
        }
        else {
            osStatus = fcntl (ProcMgrDrvUsr_handle, F_SETFD, FD_CLOEXEC);
            if (osStatus != 0) {
                /*! @retval ProcMgr_E_OSFAILURE Failed to set file descriptor
                                                flags */
                status = ProcMgr_E_OSFAILURE;
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "ProcMgrDrvUsr_open",
                                     status,
                                     "Failed to set file descriptor flags!");
            }
        }
    }
    else {
        ProcMgrDrvUsr_refCount++;
    }

    GT_1trace (curTrace, GT_LEAVE, "ProcMgrDrvUsr_open", status);

    /*! @retval ProcMgr_S_SUCCESS Operation successfully completed. */
    return status;
}


/*!
 *  @brief  Function to close the ProcMgr driver.
 *
 *  @sa     ProcMgrDrvUsr_open
 */
Int
ProcMgrDrvUsr_close (Void)
{
    Int status      = ProcMgr_S_SUCCESS;
    int osStatus    = 0;

    GT_0trace (curTrace, GT_ENTER, "ProcMgrDrvUsr_close");

    /* TBD: Protection for refCount. */
    ProcMgrDrvUsr_refCount--;
    if (ProcMgrDrvUsr_refCount == 0) {
        osStatus = close (ProcMgrDrvUsr_handle);
        if (osStatus != 0) {
            perror ("ProcMgr driver close: " ProcMgr_DRIVER_NAME);
            /*! @retval ProcMgr_E_OSFAILURE Failed to open ProcMgr driver
                                            with OS */
            status = ProcMgr_E_OSFAILURE;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "ProcMgrDrvUsr_close",
                                 status,
                                 "Failed to close ProcMgr driver with OS!");
        }
        else {
            ProcMgrDrvUsr_handle = 0;
        }
    }

    GT_1trace (curTrace, GT_LEAVE, "ProcMgrDrvUsr_close", status);

    /*! @retval ProcMgr_S_SUCCESS Operation successfully completed. */
    return status;
}


/*!
 *  @brief  Function to invoke the APIs through ioctl.
 *
 *  @param  cmd     Command for driver ioctl
 *  @param  args    Arguments for the ioctl command
 *
 *  @sa
 */
Int ProcMgrDrvUsr_ioctl(UInt32 cmd, Ptr args)
{
    Int     status          = ProcMgr_S_SUCCESS;
    Int     tmpStatus       = ProcMgr_S_SUCCESS;
    int     osStatus        = 0;
    Bool    driverOpened    = FALSE;

    GT_2trace(curTrace, GT_ENTER, "ProcMgrDrvUsr_ioctl", cmd, args);

    if (ProcMgrDrvUsr_handle < 0) {
        /* Need to open the driver handle. It was not opened from this process.
         */
        driverOpened = TRUE;
        status = ProcMgrDrvUsr_open();
        if (status < 0) {
            GT_setFailureReason(curTrace, GT_4CLASS, "ProcMgrDrvUsr_ioctl",
                    status, "Failed to open OS driver handle!");
        }
    }

    GT_assert(curTrace, (ProcMgrDrvUsr_refCount > 0));

    if (status >= 0) {
        osStatus = ioctl(ProcMgrDrvUsr_handle, cmd, args);
        if (osStatus < 0) {
            /*! @retval ProcMgr_E_OSFAILURE Driver ioctl failed */
            status = ProcMgr_E_OSFAILURE;
            GT_setFailureReason(curTrace, GT_4CLASS, "ProcMgrDrvUsr_ioctl",
                    status, "Driver ioctl failed!");
        }
        else {
            /* First field in the structure is the API status. */
            status = ((ProcMgr_CmdArgs *) args)->apiStatus;
        }

        GT_1trace(curTrace, GT_2CLASS,
                "ProcMgrDrvUsr_ioctl: API Status [0x%x]", status);
    }

    if (driverOpened == TRUE) {
        /* If the driver was temporarily opened here, close it. */
        tmpStatus = ProcMgrDrvUsr_close();
        if ((status > 0) && (tmpStatus < 0)) {
            status = tmpStatus;
            GT_setFailureReason(curTrace, GT_4CLASS, "ProcMgrDrvUsr_ioctl",
                    status, "Failed to close OS driver handle!");
        }
        ProcMgrDrvUsr_handle = -1;
    }

    GT_1trace(curTrace, GT_LEAVE, "ProcMgrDrvUsr_ioctl", status);

    return (status);
}
