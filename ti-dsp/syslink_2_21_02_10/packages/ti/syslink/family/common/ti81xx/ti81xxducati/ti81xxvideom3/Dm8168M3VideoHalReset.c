/*
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
 */

/**
 *  @file   ti/syslink/gamily/common/ti81xx/ti81xxducati/ti81xxvideom3/Dm8168M3VideoHalReset.c
 *
 *  @brief  Reset control module.
 *
 *          This module is responsible for handling reset-related hardware-
 *          specific operations.
 *          The implementation is specific to DM8168VIDEOM3.
 */

#if defined(SYSLINK_BUILD_RTOS)
#include <xdc/std.h>
#include <xdc/runtime/Error.h>
#endif

#if defined(SYSLINK_BUILD_HLOS)
#include <ti/syslink/Std.h>
#endif

#include <ti/syslink/utils/Trace.h>

#include <ti/syslink/inc/Bitops.h>
/* Module level headers */
#include <ti/syslink/inc/knl/_ProcDefs.h>
#include <ti/syslink/inc/knl/Processor.h>

/* Hardware Abstraction Layer headers */
#include <ti/syslink/inc/knl/Dm8168M3VideoHal.h>
#include <ti/syslink/inc/knl/Dm8168M3VideoHalReset.h>


/* =============================================================================
 *  Macros and types
 * =============================================================================
 */
#define CM_DEFAULT_DUCATI_CLKSTCTRL 0x00000518
#define CM_DEFAULT_DUCATI_CLKCTRL   0x00000574
#define RM_DEFAULT_RSTCTRL          0x00000B10
#define RM_DEFAULT_RSTST            0x00000B14


/* =============================================================================
 * APIs called by DM8168VIDEOM3PROC module
 * =============================================================================
 */
/*!
 *  @brief      Function to control reset operations
 *
 *  @param      halObj  Pointer to the HAL object
 *  @param      cmd     Reset control command
 *  @param      arg     Arguments specific to the reset control command
 *
 *  @sa
 */
Int
DM8168VIDEOM3_halResetCtrl(Ptr halObj, Processor_ResetCtrlCmd cmd, Ptr args)
{
    Int                  status    = PROCESSOR_SUCCESS;
    DM8168VIDEOM3_HalObject * halObject = NULL;

    GT_3trace(curTrace, GT_ENTER, "DM8168VIDEOM3_halResetCtrl", halObj, cmd, args);

    GT_assert(curTrace, (halObj != NULL));
    GT_assert(curTrace, (cmd < Processor_ResetCtrlCmd_EndValue));

    halObject = (DM8168VIDEOM3_HalObject *) halObj ;

    switch (cmd) {
        case Processor_ResetCtrlCmd_Reset:
        {
            /*Put ONLY Ducati M3_0 to Reset*/
            SET_BIT (REG((halObject->prcmBase) + RM_DEFAULT_RSTCTRL), 0x2);
            /* clear the status bit only if it is set*/
            if(TEST_BIT(REG((halObject->prcmBase) + RM_DEFAULT_RSTST)  , 0x2)) {
                REG((halObject->prcmBase) + RM_DEFAULT_RSTST) = 0x4;
            }
        }
        break;

        case Processor_ResetCtrlCmd_Release:
        {
            /*Bring ONLY Ducati M3_0 out of Reset*/
            CLEAR_BIT (REG((halObject->prcmBase) + RM_DEFAULT_RSTCTRL), 0x2);
            /*Check for Ducati M3_0 out of Reset*/
            while((((REG(halObject->prcmBase + RM_DEFAULT_RSTST)&0x04))!=0x04) &&
                  (((REG(halObject->prcmBase + RM_DEFAULT_RSTST)&0x14))!=0x14) &&
                  (((REG(halObject->prcmBase + RM_DEFAULT_RSTST)&0x1C))!=0x1C)) ;
            /*Check Module is in Functional Mode */
            while(((REG(halObject->prcmBase + CM_DEFAULT_DUCATI_CLKCTRL)&0x30000)>>16)!=0) ;
        }
        break;

        case Processor_ResetCtrlCmd_PeripheralUp:
        {
            /* Nothing to be done to bringup the peripherals for this device. */
        }
        break;

        default:
        {
            /*! @retval PROCESSOR_E_INVALIDARG Invalid argument */
            status = PROCESSOR_E_INVALIDARG;
            GT_setFailureReason (curTrace, GT_4CLASS,
                    "DM8168VIDEOM3_halResetCtrl", status,
                    "Unsupported reset ctrl cmd specified");
        }
        break;
    }

    GT_1trace(curTrace, GT_LEAVE, "DM8168VIDEOM3_halResetCtrl",status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}
