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
 *  ======== Server.c ========
 *
 */

/* this define must precede inclusion of any xdc header file */
#define Registry_CURDESC Test__Desc
#define MODULE_NAME "Server"

/* xdctools header files */
#include <xdc/std.h>
#include <xdc/runtime/Assert.h>
#include <xdc/runtime/Diags.h>
#include <xdc/runtime/Log.h>
#include <xdc/runtime/Registry.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/knl/Cache.h>

/* package header files */
#include <ti/ipc/HeapBufMP.h>
#include <ti/ipc/MessageQ.h>
#include <ti/ipc/MultiProc.h>
#include <ti/ipc/Notify.h>
#include <ti/ipc/SharedRegion.h>

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Task.h>

/* local header files */
#include "../shared/AppCommon.h"
#include "../shared/SystemCfg.h"

/* module header file */
#include "Server.h"

/* max number of outstanding commands minus one */
#define QUEUESIZE   8   


/* module structure */
typedef struct {
    UInt16              hostProcId;         // host processor id
    UInt16              lineId;             // notify line id
    UInt32              eventId;            // notify event id
    Semaphore_Struct    semObj;             // semaphore object
    Semaphore_Handle    semH;               // handle to object above
    UInt32              eventQue[QUEUESIZE];  // command queue
    UInt                head;               // queue head pointer
    UInt                tail;               // queue tail pointer
    UInt32              error;
    MessageQ_Handle     videoQue;           // created locally
} Server_Module;

/* private functions */
Void Server_notifyCB(UInt16 procId, UInt16 lineId, UInt32 eventId, UArg arg,
        UInt32 payload);

static UInt32   Server_waitForEvent(Void);

/* private data */
Registry_Desc               Registry_CURDESC;
static Int                  Module_curInit = 0;
static Server_Module        Module;


/*
 *  ======== Server_init ========
 */
Void Server_init(Void)
{
    Registry_Result result;

    if (Module_curInit++ != 0) {
        return;  /* already initialized */
    }

    /* register with xdc.runtime to get a diags mask */
    result = Registry_addModule(&Registry_CURDESC, MODULE_NAME);
    Assert_isTrue(result == Registry_SUCCESS, (Assert_Id)NULL);

    /* initialize module object state */
    Module.hostProcId = MultiProc_getId("HOST");
    Module.lineId = SystemCfg_LineId;
    Module.eventId = SystemCfg_AppEventId;
    Module.semH = NULL;
    Module.head = 0;
    Module.tail = 0;
    Module.error = 0;
}


/*
 *  ======== Server_create ========
 *
 *  1. create sync object
 *  2. register notify callback
 *  3. wait until remote core has also registered notify callback
 *
 *  4. create local & shared resources (to be opened by remote processor)
 *  5. send resource ready event
 *  6. wait for remote resource ready event
 *
 *  7. open remote resources
 *  8. send application ready event
 *  9. wait for remote server ready event
 */
Int Server_create()
{
    Int                 status;
    UInt32              event;
    Semaphore_Params    semParams;
    MessageQ_Params     msgqParams;

    /* enable some log events */
    Diags_setMask(MODULE_NAME"+EXF");

    /* 1. create sync object */
    Semaphore_Params_init(&semParams);
    semParams.mode = Semaphore_Mode_COUNTING;
    Semaphore_construct(&Module.semObj, 0, &semParams);
    Module.semH = Semaphore_handle(&Module.semObj);

    /* 2. register notify callback */
    status = Notify_registerEventSingle(Module.hostProcId, Module.lineId,
        Module.eventId, Server_notifyCB, (UArg)&Module);

    if (status < 0) {
        Log_error0("Server_create: Device failed to register notify event");
        goto leave;
    }

    /* 3. wait until remote core has also registered notify callback */
    do {
        status = Notify_sendEvent(Module.hostProcId,Module.lineId,
                Module.eventId, App_CMD_NOP, TRUE);

        if (status == Notify_E_EVTNOTREGISTERED) {
            Task_sleep(100);
        }
    } while (status == Notify_E_EVTNOTREGISTERED);

    if (status < 0 ) {
        Log_error0("Server_create: Host failed to register callback");
        goto leave;
    }

    /* 4. create local & shared resources (to be opened by remote processor) */

    /* create local message queue (inbound messages) */
    MessageQ_Params_init(&msgqParams);

    Module.videoQue = MessageQ_create(App_VideoMsgQueName, &msgqParams);

    if (Module.videoQue == NULL) {
        status = -1;
        goto leave;
    }

    /* 5. send resource ready event */
    status = Notify_sendEvent(Module.hostProcId, Module.lineId, Module.eventId,
        App_CMD_RESRDY, TRUE);

    if (status < 0) {
        goto leave;
    }

    /* 6. wait for remote resource ready event */
    do {
        event = Server_waitForEvent();

        if (event >= App_E_FAILURE) {
            status = -1;
            goto leave;
        }
    } while (event != App_CMD_RESRDY);

    /* 7. open remote resources */

    /* 8. send application ready event */
    status = Notify_sendEvent(Module.hostProcId, Module.lineId, Module.eventId,
        App_CMD_READY, TRUE);

    if (status < 0) {
        goto leave;
    }

    /* 9. wait for remote server ready event */
    do {
        event = Server_waitForEvent();

        if (event >= App_E_FAILURE) {
            status = -1;
            goto leave;
        }
    } while (event != App_CMD_READY);
	
    Log_print0(Diags_INFO,"Server_create: Slave is ready");
leave:
    Log_print1(Diags_EXIT, "<-- Server_create: %d", (IArg)status);
    return (status);
}




/*
 *  ======== Server_exec ========
 */
Int Server_exec()
{
    Int                 status;
    Bool                running = TRUE;
    App_Msg *           msg;
    MessageQ_QueueId    queId;

    Log_print0(Diags_ENTRY | Diags_INFO, "--> Server_exec:");

    while (running) {

        /* wait for inbound message */
        status = MessageQ_get(Module.videoQue, (MessageQ_Msg *)&msg,
            MessageQ_FOREVER);

        if (status == MessageQ_E_UNBLOCKED) {
            status = 0; /* this is an expected return status */
            running = FALSE;

            /* send shutdown acknowledgement */
            status = Notify_sendEvent(Module.hostProcId, Module.lineId,
                Module.eventId, App_CMD_SDACK, TRUE);
            break;
        }
        else if (status < 0) {
            goto leave;
        }

        /* process the message */
        Log_print1(Diags_INFO, "Server_exec: processed cmd=0x%x", msg->cmd);

        /* send message back */
        queId = MessageQ_getReplyQueue(msg); /* type-cast not needed */
        MessageQ_put(queId, (MessageQ_Msg)msg);
    } /* while (running) */

leave:
    Log_print1(Diags_EXIT, "<-- Server_exec: %d", (IArg)status);
    return(status);
}

/*
 *  ======== Server_delete ========
 *
 *  1. close remote resources
 *  2. send close done event
 *  3. wait for remote close done event
 *
 *  4. delete shared resoures
 *  5. send disconnect event (last sent event)
 *  6. wait for disconnect event
 *
 *  7. unregister notify callback
 *  8. delete sync object
 */
    
Int Server_delete()
{
    Int         status;
    UInt32      event;

    Log_print0(Diags_ENTRY, "--> Server_delete:");

    /* 1. close remote resources */

    /* 2. send close done event */
    status = Notify_sendEvent(Module.hostProcId, Module.lineId, Module.eventId,
        App_CMD_CLOSED, TRUE);

    if (status < 0) {
        goto leave;
    }

    /* 3. wait for remote close done event */
    do {
        event = Server_waitForEvent();

        if (event >= App_E_FAILURE) {
            status = -1;
            goto leave;
        }
    } while (event != App_CMD_CLOSED);

    /* 4. delete shared resoures */

    /* delete the video message queue */
    status = MessageQ_delete(&Module.videoQue);

    if (status < 0) {
        goto leave;
    }

    /* 5. send disconnect event (last sent event) */
    status = Notify_sendEvent(Module.hostProcId, Module.lineId, Module.eventId,
        App_CMD_DONE, TRUE);

    if (status < 0) {
        goto leave;
    }

    /* 6. wait for disconnect event (last event received) */
    do {
        event = Server_waitForEvent();

        if (event >= App_E_FAILURE) {
            status = -1;
            goto leave;
        }
    } while (event != App_CMD_DONE);

    /* 7. unregister notify callback */
    status = Notify_unregisterEventSingle(Module.hostProcId, Module.lineId,
        Module.eventId);

    /* 8. delete sync object */
    Semaphore_destruct(&Module.semObj);
    Module.semH = NULL;

    if (status < 0) {
        goto leave;
    }

leave:
    if (status < 0) {
        Log_error1("Server_finish: error=0x%x", (IArg)status);
    }

    /* disable log events */
    Log_print1(Diags_EXIT, "<-- Server_delete: %d", (IArg)status);
    Diags_setMask(MODULE_NAME"-EXF");

    return(status);
}

/*
 *  ======== Server_exit ========
 */

Void Server_exit(Void)
{

    if (Module_curInit-- != 1) {
        return;  /* object still being used */
    }

    /*
     * Note that there isn't a Registry_removeModule() yet:
     *     https://bugs.eclipse.org/bugs/show_bug.cgi?id=315448
     *
     * ... but this is where we'd call it.
     */
}

/*
 *  ======== Server_notifyCB ========
 */
Void Server_notifyCB(
    UInt16      procId,
    UInt16      lineId,
    UInt32      eventId,
    UArg        arg,
    UInt32      payload)
{
    UInt next;

    /* ignore no-op events */
    if (payload == App_CMD_NOP) {
        return;
    }

    /* handle shutdown command here */
    if ((App_CMD_MASK & payload) == App_CMD_SHUTDOWN) {
        MessageQ_unblock(Module.videoQue);
        return;
    }

    /* compute next slot in queue */
    next = (Module.head + 1) % QUEUESIZE;

    if (next == Module.tail) {
        /* queue is full, drop event and set error flag */
        Module.error = App_E_OVERFLOW;
        return;
    }
    else {
        Module.eventQue[Module.head] = payload;
        Module.head = next;
    }

    /* signal semaphore (counting) that new event is in queue */
    Semaphore_post(Module.semH);
}


/*
 *  ======== Server_waitForEvent ========
 */
static UInt32 Server_waitForEvent(Void)
{
    UInt32 event;

    if (Module.error >= App_E_FAILURE) {
        event = Module.error;
        goto leave;
    }

    /* use counting semaphore to wait for next event */
    Semaphore_pend(Module.semH, BIOS_WAIT_FOREVER);

    /* remove next command from queue */
    event = Module.eventQue[Module.tail];
    Module.tail = (Module.tail + 1) % QUEUESIZE;

leave:
    return(event);
}
