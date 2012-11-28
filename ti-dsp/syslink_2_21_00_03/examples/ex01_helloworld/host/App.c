/*
 * Copyright (c) 2012, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ======== App.c ========
 *
 */

/* host header files */
#include <stdio.h>

/* package header files */
#include <ti/syslink/Std.h>     /* must be first */
#include <ti/ipc/Notify.h>
#include <ti/syslink/utils/OsalSemaphore.h>

/* local header files */
#include "../shared/SystemCfg.h"
#include "App.h"

/* private functions */
static Void     App_notifyCB(
        UInt16  procId,
        UInt16  lineId,
        UInt32  eventId,
        UArg    arg,
        UInt32  payload);

static OsalSemaphore_Handle Module_semH = NULL;

/*
 *  ======== App_exec ========
 */
Int App_exec(UInt16 remoteProcId)
{
    Int status;

    printf("--> App_exec:\n");

    /* create a semaphore object */
    Module_semH = OsalSemaphore_create(OsalSemaphore_Type_Counting);

    if (Module_semH == NULL) {
        status = -1;
        goto leave;
    }

    /* register notify callback */
    status = Notify_registerEventSingle(remoteProcId,
        SystemCfg_LineId, SystemCfg_EventId, App_notifyCB, (UArg)NULL);

    if (status < 0) {
        goto leave;
    }

    /* wait for event */
    OsalSemaphore_pend(Module_semH, OSALSEMAPHORE_WAIT_FOREVER);

    printf("App_exec: event received from procId=%d\n", remoteProcId);

    /* unregister notify callback */
    status = Notify_unregisterEventSingle(remoteProcId,
        SystemCfg_LineId, SystemCfg_EventId);

    if (status < 0) {
        goto leave;
    }

    /* delete sync object */
    OsalSemaphore_delete(&Module_semH);

leave:
    printf("<-- App_exec: %d\n", status);
    return(status);
}


/*
 *  ======== App_notifyCB ========
 */
static Void App_notifyCB(
    UInt16      procId,
    UInt16      lineId,
    UInt32      eventId,
    UArg        arg,
    UInt32      payload)
{
    /* signal semaphore that event has been received */
    OsalSemaphore_post(Module_semH);
}
