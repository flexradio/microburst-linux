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
#include <unistd.h>
#include <semaphore.h>

/* package header files */
#include <ti/syslink/Std.h>     /* must be first */
#include <ti/ipc/HeapBufMP.h>
#include <ti/ipc/MessageQ.h>
#include <ti/ipc/MultiProc.h>
#include <ti/ipc/Notify.h>
#include <ti/ipc/SharedRegion.h>


/* local header files */
#include "../shared/AppCommon.h"
#include "../shared/SystemCfg.h"
#include "App.h"

#define QUEUESIZE 8

/* module structure */
typedef struct {
    UInt16                  remoteProcId;
    UInt16                  lineId;     // notify line id
    UInt32                  eventId;    // notify event id
    sem_t                   semH;
    UInt32                  eventQue[QUEUESIZE];
    UInt                    head;       // queue head pointer
    UInt                    tail;       // queue tail pointer
    UInt32                  error;
    HeapBufMP_Handle        msgHeap;    // created locally
    MessageQ_Handle         hostQue;    // created locally
    MessageQ_QueueId        videoQue;   // opened remotely
    UInt16                  heapId;     // MessageQ heapId
    UInt32                  msgSize;
} App_Module;

/* private functions */
static Void     App_notifyCB(
        UInt16  procId,
        UInt16  lineId,
        UInt32  eventId,
        UArg    arg,
        UInt32  payload);

static UInt32   App_waitForEvent(Void);

/* private data */
static App_Module Module;


/*
 *  ======== App_create ========
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

Int App_create(UInt16 remoteProcId)
{
    Int                 status    =0;
    int                 retStatus =0;
    UInt32              event     =0;
    HeapBufMP_Params    heapParams;
    MessageQ_Params     msgqParams;

    printf("--> App_create:\n");

    /* setting default values */
    Module.remoteProcId = remoteProcId;
    Module.lineId = SystemCfg_LineId;
    Module.eventId = SystemCfg_AppEventId;
    Module.head = 0;
    Module.tail = 0;
    Module.error = 0;
    Module.msgHeap = NULL;
    Module.hostQue = NULL;
    Module.videoQue = MessageQ_INVALIDMESSAGEQ;
    Module.heapId = App_MsgHeapId;
    Module.msgSize = sizeof(App_Msg);

    /* 1. create sync object */
    retStatus = sem_init(&Module.semH, 0, 0);

    if (retStatus == -1) {
        printf("App_create: Failed to create a semaphore\n");
        goto leave;
    }

    /* 2. register notify callback */
    status = Notify_registerEventSingle(Module.remoteProcId, Module.lineId,
        Module.eventId, App_notifyCB, (UArg)&Module);

    if (status < 0) {
        printf("App_create: Host failed to register an event\n");
        goto leave;
    }

    /* 3. wait until remote core has also registered notify callback */
    do {
        status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
            Module.eventId, App_CMD_NOP, TRUE);

        if (status == Notify_E_EVTNOTREGISTERED) {
            sleep(1);
        }
    } while (status == Notify_E_EVTNOTREGISTERED);

    if (status < 0) {
        printf("App_create: Failed to send event\n");
        goto leave;
    }

    /* 4. create local & shared resources (to be opened by remote processor) */

    /* create heap for messages */
    HeapBufMP_Params_init(&heapParams);

    heapParams.name = App_MsgHeapName;
    heapParams.regionId = App_MsgHeapSrId;
    heapParams.blockSize = 64;
    heapParams.numBlocks = 10;

    Module.msgHeap = HeapBufMP_create(&heapParams);

    if (Module.msgHeap == NULL) {
        printf("App_create: Failed to create a HeapBufMP\n");
        status = -1;
        goto leave;
    }

    /* register heap with MessageQ */
    status = MessageQ_registerHeap((Ptr)(Module.msgHeap), App_MsgHeapId);

    if (status < 0) {
        printf("App_create: Failed to register HeapBufMP with MessageQ\n");
        goto leave;
    }

    /* create local message queue (inbound messages) */
    MessageQ_Params_init(&msgqParams);

    Module.hostQue = MessageQ_create(App_HostMsgQueName, &msgqParams);

    if (Module.hostQue == NULL) {
        printf("App_create: Failed creating MessageQ\n");
        status = -1;
        goto leave;
    }

    /* 5. send resource ready event */
    status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
        Module.eventId, App_CMD_RESRDY, TRUE);

    if (status < 0) {
        printf("App_create: Failed to send event\n");
        goto leave;
    }

    /* 6. wait for remote resource ready event */
    do {
        event = App_waitForEvent();

        if (event >= App_E_FAILURE) {
            status = -1;
            printf("App_create: Failed waiting for event\n");
            goto leave;
        }
    } while (event != App_CMD_RESRDY);

    /* 7. open remote resources */

    /* open the video message queue */
    status = MessageQ_open(App_VideoMsgQueName, &Module.videoQue);

    if (status < 0) {
        printf("App_create: Failed opening MessageQ\n");
        goto leave;
    }

    /* 8. send application ready event */
    status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
        Module.eventId, App_CMD_READY, TRUE);

    if (status < 0) {
        printf("App_create: Failed to send event\n");
        goto leave;
    }

    /* 9. wait for remote server ready event */
    do {
        event = App_waitForEvent();

        if (event >= App_E_FAILURE) {
            status = -1;
            printf("App_create: Failed waiting for event\n");
            goto leave;
        }
    } while (event != App_CMD_READY);

    printf("App_create: Host is ready\n");
leave:
    printf("<-- App_create:\n");
    return(status);
}


/*
 *  ======== App_delete ========
 *
 *  1. send shutdown event
 *  2. wait for shutdown acknowledgement event
 *
 *  3. close remote resources
 *  4. send close done event
 *  5. wait for remote close done event
 *
 *  6. delete shared resoures
 *  7. send disconnect event (last sent event)
 *  8. wait for disconnect event
 *
 *  9. unregister notify callback
 * 10. delete sync object
 */
Int App_delete(Void)
{
    Int         status;
    UInt32      event;

    /* 1. send shutdown command (out-of-band) */
    status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
        Module.eventId, App_CMD_SHUTDOWN, TRUE);

    if (status < 0) {
        goto leave;
    }

    /* 2. wait for shutdown acknowledgement */
    do {
        event = App_waitForEvent();

        if (event >= App_E_FAILURE) {
            status = -1;
            goto leave;
        }
    } while (event != App_CMD_SDACK);

    /* 3. close remote resources */
    status = MessageQ_close(&Module.videoQue);

    if (status < 0) {
        goto leave;
    }

    /* 4. send close done event */
    status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
        Module.eventId, App_CMD_CLOSED, TRUE);

    if (status < 0) {
        goto leave;
    }

    /* 5. wait for remote close done event */
    do {
        event = App_waitForEvent();

        if (event >= App_E_FAILURE) {
            status = -1;
            goto leave;
        }
    } while (event != App_CMD_CLOSED);

    /* 6. delete shared resoures */

    /* delete the host message queue */
    status = MessageQ_delete(&Module.hostQue);

    if (status < 0) {
        goto leave;
    }

    /* unregister heap with MessageQ */
    status = MessageQ_unregisterHeap(App_MsgHeapId);

    if (status < 0) {
        goto leave;
    }

    /* delete the message heap */
    status = HeapBufMP_delete(&Module.msgHeap);

    if (status < 0) {
        goto leave;
    }

    /* 7. send disconnect event (last sent event) */
    status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
        Module.eventId, App_CMD_DONE, TRUE);

    if (status < 0) {
        goto leave;
    }

    /* 8. wait for disconnect event (last event received) */
    do {
        event = App_waitForEvent();

        if (event >= App_E_FAILURE) {
            status = -1;
            goto leave;
        }
    } while (event != App_CMD_DONE);

    /* 9. unregister notify callback */
    status = Notify_unregisterEventSingle(Module.remoteProcId, Module.lineId,
        Module.eventId);

    if (status < 0) {
        goto leave;
    }

    /* 10. delete sync object */
    sem_destroy(&Module.semH);

leave:
    return(status);
}


/*
 *  ======== App_exec ========
 */
Int App_exec(Void)
{
    Int         status;
    Int         i;
    App_Msg *   msg;

    printf("--> App_exec:\n");

    /* fill process pipeline */
    for (i = 1; i <= 3; i++) {
        printf("App_exec: sending message %d\n", i);

        /* allocate message */
        msg = (App_Msg *)MessageQ_alloc(Module.heapId, Module.msgSize);

        if (msg == NULL) {
            status = -1;
            goto leave;
        }

        /* set the return address in the message header */
        MessageQ_setReplyQueue(Module.hostQue, (MessageQ_Msg)msg);

        /* fill in message payload */
        msg->cmd = App_CMD_NOP;

        /* send message */
        MessageQ_put(Module.videoQue, (MessageQ_Msg)msg);
    }

    /* process steady state (keep pipeline full) */
    for (i = 4; i <= 15; i++) {

        /* wait for return message */
        status = MessageQ_get(Module.hostQue, (MessageQ_Msg *)&msg,
            MessageQ_FOREVER);

        if (status < 0) {
            goto leave;
        }

        /* extract message payload */

        /* free the message */
        MessageQ_free((MessageQ_Msg)msg);

        printf("App_exec: message received, sending message %d\n", i);

        /* allocate message */
        msg = (App_Msg *)MessageQ_alloc(Module.heapId, Module.msgSize);

        if (msg == NULL) {
            status = -1;
            goto leave;
        }

        /* set the return address in the message header */
        MessageQ_setReplyQueue(Module.hostQue, (MessageQ_Msg)msg);

        /* fill in message payload */
        msg->cmd = App_CMD_NOP;

        /* send message */
        MessageQ_put(Module.videoQue, (MessageQ_Msg)msg);
    }

    /* drain process pipeline */
    for (i = 1; i <= 3; i++) {
        printf("App_exec: message received\n");

        /* wait for return message */
        status = MessageQ_get(Module.hostQue, (MessageQ_Msg *)&msg,
            MessageQ_FOREVER);

        if (status < 0) {
            goto leave;
        }

        /* extract message payload */

        /* free the message */
        MessageQ_free((MessageQ_Msg)msg);
    }

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
    UInt next;

    /* ignore no-op events */
    if (payload == App_CMD_NOP) {
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
    sem_post(&Module.semH);
}


/*
 *  ======== App_waitForEvent ========
 */
static UInt32 App_waitForEvent(Void)
{

    Int status;
    UInt32 event;

    if (Module.error >= App_E_FAILURE) {
        event = Module.error;
        goto leave;
    }

    /* use counting semaphore to wait for next event */
    status = sem_wait(&Module.semH);

    if (status < 0) {
        event = Module.error;
        goto leave;
    }

    /* remove next command from queue */
    event = Module.eventQue[Module.tail];
    Module.tail = (Module.tail + 1) % QUEUESIZE;

leave:
    return(event);
}
