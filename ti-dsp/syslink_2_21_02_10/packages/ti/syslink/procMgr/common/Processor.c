/*
 *  @file   Processor.c
 *
 *  @brief      Generic Processor that calls into specific Processor instance
 *              through function table interface.
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



#if defined(SYSLINK_BUILD_RTOS)
#include <xdc/std.h>
#endif

#if defined(SYSLINK_BUILD_HLOS)
#include <ti/syslink/Std.h>
#endif

/*-------------------------   OSAL and utils   -------------------------------*/
#include <ti/syslink/utils/Trace.h>

/*-------------------------   Module headers   -------------------------------*/
#include <ti/syslink/inc/knl/ProcDefs.h>
#include <ti/syslink/inc/knl/Processor.h>


/* =============================================================================
 * Functions called by ProcMgr
 * =============================================================================
 */
/*!
 *  @brief      Function to attach to the Processor.
 *
 *              This function calls into the specific Processor implementation
 *              to attach to it.
 *              This function is called from the ProcMgr attach function, and
 *              hence is used to perform any activities that may be required
 *              once the slave is powered up.
 *              Depending on the type of Processor, this function may or may not
 *              perform any activities.
 *
 *  @param      handle     Handle to the Processor object
 *  @param      params     Attach parameters
 *
 *  @sa         Processor_detach
 */
inline
Int
Processor_attach (Processor_Handle handle, Processor_AttachParams * params)
{
    Int                status     = PROCESSOR_SUCCESS;
    Processor_Object * procHandle = (Processor_Object *) handle;

    GT_2trace(curTrace, GT_ENTER, "Processor_attach", handle, params);

    GT_assert (curTrace, (handle != NULL));
    GT_assert (curTrace, (params != NULL));

    /* No parameter validation here since this is an internal module, and
     * validation has already happened at the ProcMgr level.
     */
    GT_assert (curTrace, (procHandle->procFxnTable.attach != NULL));

    procHandle->bootMode = params->params->bootMode;
    status = procHandle->procFxnTable.attach (handle, params);

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (status < 0) {
        GT_setFailureReason(curTrace, GT_4CLASS, "Processor_attach", status,
                "Failed to attach to the specific Processor!");
    }
    else {
#endif
        if (procHandle->bootMode == ProcMgr_BootMode_Boot) {
            procHandle->state = ProcMgr_State_Powered;
        }
        else if ((procHandle->bootMode == ProcMgr_BootMode_NoLoad_Pwr) ||
                 (procHandle->bootMode == ProcMgr_BootMode_NoLoad_NoPwr)) {
            procHandle->state = ProcMgr_State_Loaded;
        }
        else if (procHandle->bootMode == ProcMgr_BootMode_NoBoot) {
            procHandle->state = ProcMgr_State_Running;
            /* TBD: Check actual state from h/w. */
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif

    GT_1trace(curTrace, GT_LEAVE, "Processor_attach", status);

    return (status);
}


/*!
 *  @brief      Function to detach from the Processor.
 *
 *              This function calls into the specific Processor implementation
 *              to detach from it.
 *              This function is called from the ProcMgr detach function, and
 *              hence is useful to perform any activities that may be required
 *              before the slave is powered down.
 *              Depending on the type of Processor, this function may or may not
 *              perform any activities.
 *
 *  @param      handle     Handle to the Processor object
 *
 *  @sa         Processor_attach
 */
inline
Int
Processor_detach (Processor_Handle handle)
{
    Int                status     = PROCESSOR_SUCCESS;
    Processor_Object * procHandle = (Processor_Object *) handle;

    GT_1trace (curTrace, GT_ENTER, "Processor_detach", handle);

    GT_assert (curTrace, (handle != NULL));

    /* No parameter validation here since this is an internal module, and
     * validation has already happened at the ProcMgr level.
     */
    GT_assert (curTrace, (procHandle->procFxnTable.detach != NULL));
    status = procHandle->procFxnTable.detach (handle);
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (status < 0) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "Processor_detach",
                             status,
                             "Failed to detach from the specific Processor!");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        /* For all boot modes, at the end of detach, the Processor is in
         * unknown state.
         */
        procHandle->state = ProcMgr_State_Unknown;
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */

    GT_1trace (curTrace, GT_LEAVE, "Processor_detach", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to start the processor.
 *
 *              This function calls into the specific Processor implementation
 *              to start the slave processor running.
 *              This function starts the slave processor running, in most
 *              devices, by programming its entry point into the boot location
 *              of the slave processor and releasing it from reset.
 *              The handle specifies the specific Processor instance to be used.
 *
 *  @param      handle     Handle to the Processor object
 *  @param      entryPt    Entry point of the file loaded on the slave Processor
 *
 *  @sa         Processor_stop
 */
inline
Int
Processor_start (Processor_Handle        handle,
                 UInt32                  entryPt,
                 Processor_StartParams * params)
{
    Int                status     = PROCESSOR_SUCCESS;
    Processor_Object * procHandle = (Processor_Object *) handle;

    GT_3trace (curTrace, GT_ENTER, "Processor_start", handle, entryPt, params);

    GT_assert (curTrace, (handle != NULL));
    /* entryPt may be 0 for some devices. Cannot check for valid/invalid. */
    GT_assert (curTrace, (params != NULL));

    /* No parameter validation here since this is an internal module, and
     * validation has already happened at the ProcMgr level.
     */
    GT_assert (curTrace, (procHandle->procFxnTable.start != NULL));
    status = procHandle->procFxnTable.start (handle, entryPt, params);
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (status < 0) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "Processor_start",
                             status,
                             "Failed to start the slave processor!");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        if (    (procHandle->bootMode == ProcMgr_BootMode_Boot)
            ||  (procHandle->bootMode == ProcMgr_BootMode_NoLoad_Pwr)
            ||  (procHandle->bootMode == ProcMgr_BootMode_NoLoad_NoPwr)) {
            procHandle->state = ProcMgr_State_Running;
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */

    GT_1trace (curTrace, GT_LEAVE, "Processor_start", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to stop the processor.
 *
 *              This function calls into the specific Processor implementation
 *              to stop the slave processor.
 *              This function stops the slave processor running, in most
 *              devices, by placing it in reset.
 *              The handle specifies the specific Processor instance to be used.
 *
 *  @param      handle     Handle to the Processor object
 *
 *  @sa         Processor_start
 */
inline
Int
Processor_stop (Processor_Handle handle)
{
    Int                status     = PROCESSOR_SUCCESS;
    Processor_Object * procHandle = (Processor_Object *) handle;

    GT_1trace (curTrace, GT_ENTER, "Processor_stop", handle);

    GT_assert (curTrace, (handle != NULL));

    /* No parameter validation here since this is an internal module, and
     * validation has already happened at the ProcMgr level.
     */
    GT_assert (curTrace, (procHandle->procFxnTable.stop != NULL));
    status = procHandle->procFxnTable.stop (handle);
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (status < 0) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "Processor_stop",
                             status,
                             "Failed to stop the slave processor!");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        if (    (procHandle->bootMode == ProcMgr_BootMode_Boot)
            ||  (procHandle->bootMode == ProcMgr_BootMode_NoLoad_Pwr)
            ||  (procHandle->bootMode == ProcMgr_BootMode_NoLoad_NoPwr)) {
            procHandle->state = ProcMgr_State_Reset;
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */

    GT_1trace (curTrace, GT_LEAVE, "Processor_stop", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to read from the slave processor's memory.
 *
 *              This function calls into the specific Processor implementation
 *              to read from the slave processor's memory. It reads from the
 *              specified address in the processor's address space and copies
 *              the required number of bytes into the specified buffer.
 *              It returns the number of bytes actually read in the numBytes
 *              parameter.
 *              Depending on the processor implementation, it may result in
 *              reading from shared memory or across a peripheral physical
 *              connectivity.
 *              The handle specifies the specific Processor instance to be used.
 *
 *  @param      handle     Handle to the Processor object
 *  @param      procAddr   Address in host processor's address space of the
 *                         memory region to read from.
 *  @param      numBytes   IN/OUT parameter. As an IN-parameter, it takes in the
 *                         number of bytes to be read. When the function
 *                         returns, this parameter contains the number of bytes
 *                         actually read.
 *  @param      buffer     User-provided buffer in which the slave processor's
 *                         memory contents are to be copied.
 *
 *  @sa         Processor_write
 */
inline
Int
Processor_read (Processor_Handle handle,
                UInt32           procAddr,
                UInt32 *         numBytes,
                Ptr              buffer)
{
    Int                status     = PROCESSOR_SUCCESS;
    Processor_Object * procHandle = (Processor_Object *) handle;

    GT_4trace (curTrace, GT_ENTER, "Processor_read",
               handle, procAddr, numBytes, buffer);

    GT_assert (curTrace, (handle   != NULL));
    GT_assert (curTrace, (numBytes != NULL));
    GT_assert (curTrace, (buffer   != NULL));

    /* No parameter validation here since this is an internal module, and
     * validation has already happened at the ProcMgr level.
     */
    GT_assert (curTrace, (procHandle->procFxnTable.read != NULL));
    status = procHandle->procFxnTable.read (handle, procAddr, numBytes, buffer);
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (status < 0) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "Processor_read",
                             status,
                             "Failed to read from the slave processor!");
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */

    GT_1trace (curTrace, GT_LEAVE, "Processor_read", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to write into the slave processor's memory.
 *
 *              This function calls into the specific Processor implementation
 *              to write into the slave processor's memory. It writes into the
 *              specified address in the processor's address space and copies
 *              the required number of bytes from the specified buffer.
 *              It returns the number of bytes actually written in the numBytes
 *              parameter.
 *              Depending on the processor implementation, it may result in
 *              writing into shared memory or across a peripheral physical
 *              connectivity.
 *              The handle specifies the specific Processor instance to be used.
 *
 *  @param      handle     Handle to the Processor object
 *  @param      procAddr   Address in host processor's address space of the
 *                         memory region to write into.
 *  @param      numBytes   IN/OUT parameter. As an IN-parameter, it takes in the
 *                         number of bytes to be written. When the function
 *                         returns, this parameter contains the number of bytes
 *                         actually written.
 *  @param      buffer     User-provided buffer from which the data is to be
 *                         written into the slave processor's memory.
 *
 *  @sa         Processor_read
 */
inline
Int
Processor_write (Processor_Handle handle,
                 UInt32           procAddr,
                 UInt32 *         numBytes,
                 Ptr              buffer)
{
    Int                status     = PROCESSOR_SUCCESS;
    Processor_Object * procHandle = (Processor_Object *) handle;

    GT_4trace (curTrace, GT_ENTER, "Processor_write",
               handle, procAddr, numBytes, buffer);

    GT_assert (curTrace, (handle   != NULL));
    GT_assert (curTrace, (numBytes != NULL));
    GT_assert (curTrace, (buffer   != NULL));

    /* No parameter validation here since this is an internal module, and
     * validation has already happened at the ProcMgr level.
     */
    GT_assert (curTrace, (procHandle->procFxnTable.write != NULL));
    status = procHandle->procFxnTable.write (handle,
                                             procAddr,
                                             numBytes,
                                             buffer);
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (status < 0) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "Processor_write",
                             status,
                             "Failed to write into the slave processor!");
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */

    GT_1trace (curTrace, GT_LEAVE, "Processor_write", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to get the current state of the slave Processor.
 *
 *              This function gets the state of the slave processor as
 *              maintained on the master Processor state machine. It does not
 *              go to the slave processor to get its actual state at the time
 *              when this API is called.
 *
 *  @param      handle   Handle to the Processor object
 *
 *  @sa         Processor_setState
 */
ProcMgr_State
Processor_getState (Processor_Handle handle)
{
    Processor_Object * procHandle = (Processor_Object *) handle;

    GT_1trace (curTrace, GT_ENTER, "Processor_getState", handle);

    GT_assert (curTrace, (handle != NULL));

    /* No parameter validation here since this is an internal module, and
     * validation has already happened at the ProcMgr level.
     */

    GT_1trace (curTrace, GT_LEAVE, "Processor_getState", procHandle->state);

    /*! @retval Processor state */
    return procHandle->state;
}


/*!
 *  @brief      Function to set the current state of the slave Processor
 *              to specified value.
 *
 *              This function is used to set the state of the processor to the
 *              value as specified. This function may be used by external
 *              entities that affect the state of the slave processor, such as
 *              PwrMgr, error handler, or ProcMgr.
 *
 *  @param      handle   Handle to the Processor object
 *  @param      state    State that needs to be set for the Processor object
 *
 *  @sa         Processor_getState
 */
Void
Processor_setState (Processor_Handle handle, ProcMgr_State state)
{
    Processor_Object * procHandle = (Processor_Object *) handle;

    GT_1trace (curTrace, GT_ENTER, "Processor_setState", handle);

    GT_assert (curTrace, (handle != NULL));

    GT_2trace (curTrace,
               GT_4CLASS,
               "Processor_setState: Old state: %d, New state: %d",
               procHandle->state,
               state);

    procHandle->state = state;

    /* No parameter validation here since this is an internal module, and
     * validation has already happened at the ProcMgr level.
     */

    GT_0trace (curTrace, GT_LEAVE, "Processor_setState");
}


/*!
 *  @brief      Function to perform device-dependent operations.
 *
 *              This function calls into the specific Processor implementation
 *              to perform device dependent control operations. The control
 *              operations supported by the device are exposed directly by the
 *              specific implementation of the Processor interface. These
 *              commands and their specific argument types are used with this
 *              function.
 *
 *  @param      handle     Handle to the Processor object
 *  @param      cmd        Device specific processor command
 *  @param      arg        Arguments specific to the type of command.
 *
 *  @sa
 */
inline
Int
Processor_control (Processor_Handle handle, Int32 cmd, Ptr arg)
{
    Int                status     = PROCESSOR_SUCCESS;
    Processor_Object * procHandle = (Processor_Object *) handle;

    GT_3trace (curTrace, GT_ENTER, "Processor_control", handle, cmd, arg);

    GT_assert (curTrace, (handle   != NULL));
    /* cmd and arg can be 0/NULL, so cannot check for validity. */

    /* No parameter validation here since this is an internal module, and
     * validation has already happened at the ProcMgr level.
     */
    GT_assert (curTrace, (procHandle->procFxnTable.control != NULL));
    status = procHandle->procFxnTable.control (handle, cmd, arg);
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (status < 0) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "Processor_control",
                             status,
                             "Processor control command failed!");
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */

    GT_1trace (curTrace, GT_LEAVE, "Processor_control", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to translate slave physical address to master physical
 *              address.
 *
 *  @param      handle     Handle to the Processor object
 *  @param      dstAddr    Returned: master physical address.
 *  @param      srcAddr    Slave physical address.
 *
 *  @sa
 */
inline
Int Processor_translateAddr(Processor_Handle handle, UInt32 *dstAddr,
        UInt32 srcAddr)
{
    Int                status     = PROCESSOR_SUCCESS;
    Processor_Object * procHandle = (Processor_Object *) handle;

    GT_3trace (curTrace, GT_ENTER, "Processor_translateAddr",
               handle, dstAddr, srcAddr);

    GT_assert (curTrace, (handle        != NULL));
    GT_assert (curTrace, (dstAddr       != NULL));

    /* No parameter validation here since this is an internal module, and
     * validation has already happened at the ProcMgr level.
     */
    GT_assert (curTrace, (procHandle->procFxnTable.translateAddr != NULL));
    status = procHandle->procFxnTable.translateAddr(handle, dstAddr, srcAddr);

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (status < 0) {
        GT_setFailureReason(curTrace, GT_4CLASS, "Processor_translateAddr",
                status, "Processor address translation failed!");
    }
#endif

    GT_1trace(curTrace, GT_LEAVE, "Processor_translateAddr", status);

    return (status);
}


/*!
 *  @brief      Function to map address to slave address space.
 *
 *              This function maps the provided slave address to a host address
 *              and returns the mapped address and size.
 *
 *  @param      handle      Handle to the Processor object
 *  @param      dstAddr     Return parameter: Pointer to receive the mapped
 *                          address.
 *  @param      srcAddr     Source address in the source address space
 *  @param      size        Size of the region to be mapped.
 *
 *  @sa
 */
inline
Int
Processor_map (Processor_Handle handle,
               UInt32 *         dstAddr,
               UInt32           nSegs,
               Memory_SGList *  sglist)
{
    Int                status     = PROCESSOR_SUCCESS;
    Processor_Object * procHandle = (Processor_Object *) handle;

    GT_4trace (curTrace, GT_ENTER, "Processor_map",
               handle, dstAddr, nSegs, sglist);

    GT_assert (curTrace, (handle  != NULL));
    GT_assert (curTrace, (dstAddr != NULL));
    GT_assert (curTrace, (sglist  != NULL));
    GT_assert (curTrace, (nSegs   != 0));

    /* No parameter validation here since this is an internal module, and
     * validation has already happened at the ProcMgr level.
     */
    GT_assert (curTrace, (procHandle->procFxnTable.map != NULL));
    status = procHandle->procFxnTable.map (handle,
                                           dstAddr,
                                           nSegs,
                                           sglist);
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (status < 0) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "Processor_map",
                             status,
                             "Processor address map operation failed!");
    }
#endif

    GT_1trace(curTrace, GT_LEAVE, "Processor_map", status);

    return (status);
}


/*!
 *  @brief      Function to unmap address to slave address space.
 *
 *  @param      handle      Handle to the Processor object
 *  @param      dstAddr     Return parameter: Pointer to receive the mapped
 *                          address.
 *  @param      size        Size of the region to be mapped.
 */
inline
Int Processor_unmap(Processor_Handle handle, UInt32 addr, UInt32 size)
{
    Int                status     = PROCESSOR_SUCCESS;
    Processor_Object * procHandle = (Processor_Object *) handle;

    GT_3trace(curTrace, GT_ENTER, "Processor_unmap", handle, addr, size);

    GT_assert (curTrace, (handle != NULL));
    GT_assert (curTrace, (size   != 0));

    /* No parameter validation here since this is an internal module, and
     * validation has already happened at the ProcMgr level.
     */
    GT_assert (curTrace, (procHandle->procFxnTable.unmap != NULL));
    status = procHandle->procFxnTable.unmap(handle, addr, size);

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (status < 0) {
        GT_setFailureReason (curTrace, GT_4CLASS, "Processor_unmap", status,
                "Processor address unmap operation failed!");
    }
#endif

    GT_1trace (curTrace, GT_LEAVE, "Processor_unmap", status);

    return (status);
}
