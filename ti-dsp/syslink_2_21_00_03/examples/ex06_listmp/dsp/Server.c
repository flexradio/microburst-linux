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

/* slave header files */
#include <string.h>

/* xdctools header files */
#include <xdc/std.h>
#include <xdc/runtime/Assert.h>
#include <xdc/runtime/Diags.h>
#include <xdc/runtime/Log.h>
#include <xdc/runtime/Registry.h>
#include <xdc/runtime/System.h>

/* package header files */
#include <ti/ipc/Notify.h>
#include <ti/ipc/SharedRegion.h>
#include <ti/ipc/HeapBufMP.h>
#include <ti/ipc/ListMP.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/BIOS.h>

/* local header files */
#include "../shared/SystemCfg.h"
#include "../shared/AppCommon.h"

/* module header file */
#include "Server.h"

/* max number of outstanding commands minus one */
#define QUEUESIZE   8   

/* queue structure */
typedef struct 
{
    UInt32              queue[QUEUESIZE];   /* command queue */
    UInt                head;               /* queue head pointer */
    UInt                tail;               /* queue tail pointer */
    UInt32              error;              /* error flag */
    Semaphore_Struct    semObj;             /* semaphore object */
    Semaphore_Handle    semH;               /* handle to object above */
} Event_Queue;   

/* module structure */
typedef struct {
    Event_Queue         eventQueue;
    UInt16              remoteProcId;
    UInt16              lineId;                 /* notify line id */
    UInt32              eventId;                /* notify event id */
    ListMP_Handle       ListMP_localHandle;
    ListMP_Handle       ListMP_remoteHandle;
} Server_Module;

/* private functions */
static UInt32 Server_waitForEvent(Event_Queue* eventQueue);
static Void Server_notifyCB(UInt16 procId, UInt16 lineId, UInt32 eventId, 
        UArg arg, UInt32 payload);

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
}


/*
 *  ======== Server_create ========
 *  1. create sync object
 *  2. register notify callback
 *  3. wait until remote core has also registered notify callback
 *  4. create local ListMP 
 *  5. send ListMP create command
 *  6. open remote ListMP
 */
Int Server_create(UInt16 remoteProcId)
{
    Int                 status      = 0;
    UInt32              event       = 0;
    Semaphore_Params    semParams;
    ListMP_Params       listParams;
    
    Log_print0(Diags_ENTRY, "--> Server_create:");
    
    /* setting default values */
    Module.eventQueue.head          = 0;               
    Module.eventQueue.tail          = 0;              
    Module.eventQueue.error         = 0;       
    Module.eventQueue.semH          = NULL;
    Module.lineId                   = SystemCfg_LineId;
    Module.eventId                  = SystemCfg_EventId;
    Module.remoteProcId             = remoteProcId;
    Module.ListMP_localHandle       = 0;
    Module.ListMP_remoteHandle      = 0;
    
    /* enable some log events */
    Diags_setMask(MODULE_NAME"+EXF");

    /* 1. create sync object */
    Semaphore_Params_init(&semParams);
    semParams.mode = Semaphore_Mode_COUNTING;
    
    Semaphore_construct(&Module.eventQueue.semObj, 0, &semParams);

    Module.eventQueue.semH = Semaphore_handle(&Module.eventQueue.semObj);

    /* 2. register notify callback */
    status = Notify_registerEvent(Module.remoteProcId, Module.lineId, 
            Module.eventId, Server_notifyCB,(UArg)  &Module.eventQueue);

    if (status < 0) {
        Log_error0("Server_create: Device failed to register notify event");
        goto leave;
    }

    /* 3. wait until remote core has also registered notify callback */
    do {
        status = Notify_sendEvent(Module.remoteProcId,Module.lineId,
                Module.eventId, APP_CMD_NOP, TRUE);

        if (status == Notify_E_EVTNOTREGISTERED) {
            Task_sleep(100);
        }
        
    } while (status == Notify_E_EVTNOTREGISTERED);

    if (status < 0 ) {
        Log_error0("Server_create: Host failed to register callback");
        goto leave;
    }
    
    /* 4. create local ListMP */
    ListMP_Params_init(&listParams);
    listParams.name     = SLAVE_LISTMP_NAME;
    listParams.regionId = SHARED_REGION_1;

    Module.ListMP_localHandle = ListMP_create(&listParams);

    if(Module.ListMP_localHandle == NULL) {
        Log_error0("Server_create: Creating ListMP has failed");
        status = -1;
        goto leave;
    }

    /* 5. send ListMP created command */
    status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
            Module.eventId, APP_CMD_LIST_CREATED, TRUE);

    if (status < 0 ) {
        Log_error0("Server_create: Failed to send ListMP create command");
        goto leave;
    }

    /* wait for ListMP create command */
    event = Server_waitForEvent(&Module.eventQueue);

    if (event >= APP_E_FAILURE) {
        Log_error1("Server_create: Received queue error: %d",event);
        status = -1;
        goto leave;
    }

    /* 6. open remote ListMP */
    status = ListMP_open(GPP_LISTMP_NAME,&Module.ListMP_remoteHandle);

    if (status < 0) {
        Log_error0("Server_create: Failed to open remote ListMP");
        goto leave;
    }
    
    Log_print0(Diags_INFO, "Server_create: Slave is ready");

leave:
    Log_print1(Diags_EXIT, "<-- Server_create: %d", (IArg)status);
    return (status);
}


/*
 *  ======== Server_exec ========
 *  1. wait for process ListMP command
 *  2. get node from the tail of the remote ListMP
 *  3. convert lowercase string to uppercase
 *  4. add new node to the head of the local ListMP
 *  5. send operation complete command
 */
Int Server_exec()
{
    Int                 status      = 0;
    UInt32              event       = 0;
    Char*               bufferPtr   = 0;
    ListMP_Node*        node        = 0;
    
    Log_print0(Diags_ENTRY | Diags_INFO, "--> Server_exec:");

    /* 1. wait for process ListMP command */
    event = Server_waitForEvent(&Module.eventQueue);
    
    if (event >= APP_E_FAILURE) {
        Log_error1("Server_exec: Received queue error: %d",event); 
        status = -1;       
        goto leave;
    } 
   
    /* grab all nodes from the remote ListMP */
    while(ListMP_empty(Module.ListMP_remoteHandle) == FALSE) {
        
        /* 2. get node from the tail of the remote ListMP */
        node = (ListMP_Node*) ListMP_getTail(Module.ListMP_remoteHandle);

        /* set pointer to new node's buffer */
        bufferPtr = node->buffer;
    
        /* 3. convert lowercase string to uppercase */
        while (*bufferPtr != 0) {
            if (*bufferPtr >= 0x61 && *bufferPtr <= 0x7A) {
                *bufferPtr = *bufferPtr - 0x20;
            }
            bufferPtr++;
        }

        /* 4. add new node to the head of the local ListMP */
        status = ListMP_putHead(Module.ListMP_localHandle,
                (ListMP_Elem *) node);

        if (status < 0) {
            Log_error0("Server_exec: Failed to add node to the end of the local "
                    "ListMP");
            goto leave;
        }
    }

    Log_print0(Diags_INFO, "Server_exec: Completed lowercase to uppercase "
            "string operation"); 

    /* 5. send operation complete command */
    status = Notify_sendEvent(Module.remoteProcId, Module.lineId, 
            Module.eventId, APP_CMD_OP_COMPLETE, TRUE);

    if (status < 0 )  {
        Log_error0("Server_exec: Error sending operation complete"
                "command");
        goto leave;
    }    

leave:
    Log_print1(Diags_EXIT, "<-- Server_exec: %d", (IArg)status);
    return (status);
}

/*
 *  ======== Server_delete ========
 *  1. wait for shutdown command
 *  2. close remote instance of ListMP
 *  3. send ListMP cleanup command
 *  4. delete local ListMP
 *  5. unregister notify callback
 *  6. delete sync object
 */
    
Int Server_delete()
{
    Int     status = 0; 
    UInt32  event  = 0;

    Log_print0(Diags_ENTRY, "--> Server_delete:");
    
    /* 1. wait for shutdown command */
    event = Server_waitForEvent(&Module.eventQueue);

    if (event >= APP_E_FAILURE) {
        Log_error1("Server_delete: Received queue error: %d",event); 
        status = -1;       
        goto leave;
    } 

    /* send shutdown command acknowledge */
    status = Notify_sendEvent(Module.remoteProcId, Module.lineId, 
            Module.eventId, APP_CMD_SHUTDOWN_ACK, TRUE);

    if (status < 0 )  {
        Log_error0("Server_delete: Error sending shutdown command acknowledge");
        goto leave;
    }

    
    /* 2. close remote instance of ListMP */
    status = ListMP_close(&Module.ListMP_remoteHandle);

    if (status < 0) {
        Log_error0("Server_delete: Failed to close remote ListMP");
        goto leave;
    }
    
    /* 3. send ListMP cleanup command */
    status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
            Module.eventId, APP_CMD_CLEANUP_LIST, TRUE);

    if (status < 0 ){
        Log_error0("Server_delete: Error sending cleanup ListMP command");
        goto leave;
    }

    /* wait for ListMP cleanup command */
    event = Server_waitForEvent(&Module.eventQueue);

    if (event >= APP_E_FAILURE) {
        Log_error1("Server_delete: Received queue error: %d",event);
        status = -1;
        goto leave;
    }

    /* 4. delete local ListMP */
    status = ListMP_delete(&Module.ListMP_localHandle);

    if (status < 0) {
        Log_error0("Server_delete: Failed to delete ListMP");
        goto leave;
    }

    /* 5. unregister notify callback */
    status = Notify_unregisterEvent(Module.remoteProcId, Module.lineId, 
            Module.eventId, Server_notifyCB,(UArg) &Module.eventQueue);

    if (status < 0) {
        Log_error0("Server_delete: Unregistering event has failed");
        goto leave;
    }

    /* 6. delete sync object */
    Semaphore_destruct(&Module.eventQueue.semObj);
    
    Log_print0(Diags_INFO,"Server_delete: Cleanup complete");

leave:
    Log_print1(Diags_EXIT, "<-- Server_delete: %d", (IArg)status);
    return (status);
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
 *
 *  This function is called from within an ISR
 */
Void Server_notifyCB( UInt16 procId, UInt16 lineId, UInt32  eventId, UArg arg,
        UInt32 payload)
{
    UInt            next;
    Event_Queue*    eventQueue = (Event_Queue *) arg;

    /* ignore no-op events */
    if (payload == APP_CMD_NOP) {
        return;
    }

    /* compute next slot in queue */
    next = (eventQueue->head + 1) % QUEUESIZE;

    if (next == eventQueue->tail) {
        /* queue is full, drop event and set error flag */
        eventQueue->error = APP_E_OVERFLOW;
    }
    else {
        eventQueue->queue[eventQueue->head] = payload;
        
        /* queue head is only written to within this function */
        eventQueue->head = next;
        
        /* signal semaphore (counting) that new event is in queue */
        Semaphore_post(eventQueue->semH);
    }

    return;
}

/*
 *  ======== Server_waitForEvent ========
 * 
 */
static UInt32 Server_waitForEvent(Event_Queue* eventQueue)
{
    UInt32 event;

    if (eventQueue->error >= APP_E_FAILURE) {
        event = eventQueue->error;
    }
    else {
        /* use counting semaphore to wait for next event */
        Semaphore_pend(eventQueue->semH,  BIOS_WAIT_FOREVER);

        /* remove next command from queue */
        event = eventQueue->queue[eventQueue->tail];
        
        /* queue tail is only written to within this function */
        eventQueue->tail = (eventQueue->tail + 1) % QUEUESIZE;
    }

    return (event);
}

