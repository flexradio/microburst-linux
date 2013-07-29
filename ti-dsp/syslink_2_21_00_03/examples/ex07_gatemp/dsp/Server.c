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
#include <stdlib.h>

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
#include <ti/ipc/GateMP.h>
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
#define LOOP_ITR    10
#define SEED        13

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
    Event_Queue     eventQueue;
    UInt16          remoteProcId;
    UInt16          lineId;             /* notify line id */
    UInt32          eventId;            /* notify event id */
    GateMP_Handle   remoteGateMPHandle;
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
 *  3. open remote GateMP
 */
Int Server_create(UInt16 remoteProcId)
{
    Uint32              event       = 0;
    Int                 status      = 0;
    Semaphore_Params    semParams;
    
    Log_print0(Diags_ENTRY, "--> Server_create:");
    
    /* setting default values */
    Module.eventQueue.head      = 0;               
    Module.eventQueue.tail      = 0;              
    Module.eventQueue.error     = 0;       
    Module.eventQueue.semH      = NULL;
    Module.lineId               = SystemCfg_LineId;
    Module.eventId              = SystemCfg_EventId;
    Module.remoteProcId         = remoteProcId;
  
    /* set random number seed */ 
    srand (SEED);
     
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

    /* wait until remote core has also registered notify callback */
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

    /* wait for GateMP created command */
    event = Server_waitForEvent(&Module.eventQueue);
    
    if (event >= APP_E_FAILURE) {
        Log_error1("Server_create: Received queue error: %d",event); 
        status = -1;       
        goto leave;
    } 

    /* 3. open remote GateMP */
    status = GateMP_open(GATEMP_NAME,&Module.remoteGateMPHandle);

    if (status < 0) {
        Log_error0("Server_create: Failed to open remote GateMP");
        goto leave;
    }

    /* send GateMP created acknowledge command */
    status = Notify_sendEvent(Module.remoteProcId, Module.lineId, 
        Module.eventId, APP_CMD_GATECREATED_ACK, TRUE);

    if (status < 0 )  {
        Log_error0("Server_create: Error sending GateMP created acknowledge "
                "command");
        goto leave;
    }   
    
    Log_print0(Diags_INFO, "Server_create: Slave is ready");

leave:
    Log_print1(Diags_EXIT, "<-- Server_create: %d", (IArg)status);
    return (status);
}

/*
 *  ======== Server_exec ========
 *  1. wait for shared region pointer address lower two bytes
 *  2. save only the payload which represents the shared region pointer 
 *     address lower two bytes 
 *  3. wait for shared region pointer address upper two bytes
 *  4. join shared region pointer address upper and lower two bytes
 *  5. translate shared region pointer to local address space pointer
 *  6. enter GateMP
 *  7. leave Gate
 */
Int Server_exec()
{
    Int                 status              = 0;
    SharedRegion_SRPtr  sharedBufferPtr     = 0;
    UInt32              event               = 0;
    Int*                intPtr              = 0;
    Int                 num                 = 0;
    UInt                x                   = 0; 
    IArg                gateKey             = 0;

    Log_print0(Diags_ENTRY | Diags_INFO, "--> Server_exec:");

    /* 1. wait for shared region pointer address lower two bytes */
    event = Server_waitForEvent(&Module.eventQueue);
    
    if (event >= APP_E_FAILURE) {
        Log_error1("Server_exec: Received queue error: %d",event); 
        status = -1;       
        goto leave;
    } 
   
    /* 2. save only the payload which represents the shared region pointer 
     *    address lower two bytes 
     */ 
    sharedBufferPtr = event & APP_SPTR_MASK;
    
    /* send shared region pointer address acknowledge */
    status = Notify_sendEvent(Module.remoteProcId, Module.lineId, 
            Module.eventId, APP_SPTR_ADDR_ACK, TRUE);

    if (status < 0 )  {
        Log_error0("Server_exec: Error sending shared region pointer"
                "address acknowledge");
        goto leave;
    }    
    
    /* 3. wait for shared region pointer address upper two bytes */
    event = Server_waitForEvent(&Module.eventQueue);
    
    if (event >= APP_E_FAILURE) {
        Log_error1("Server_exec: Received queue error: %d",event); 
        status = -1;       
        goto leave;
    } 
    
    /* 4. join shared region pointer address upper and lower two bytes */
    sharedBufferPtr = ((event & APP_SPTR_MASK) << 16) | sharedBufferPtr;
    
    /* send shared region pointer address acknowledge */
    status = Notify_sendEvent(Module.remoteProcId, Module.lineId, 
            Module.eventId, APP_SPTR_ADDR_ACK, TRUE);

    if (status < 0 )  {
        Log_error0("Server_exec: Error sending shared region pointer"
                "address acknowledge");
        goto leave;
    }        
    
    /* 5. translate shared region pointer to local address space pointer */
    intPtr = SharedRegion_getPtr(sharedBufferPtr);
    
    Log_print0(Diags_INFO,"Server_exec: Modifying shared region variable "
            "value");

    for (x = 0;x < LOOP_ITR; x++) {

        /* modify the shared variable as long as no one else is currently
         * accessing it 
         */

        /* 6. enter GateMP */
        gateKey = GateMP_enter (Module.remoteGateMPHandle);

        /* randomly modify the shared region variable */
        if ( rand() % 2) {
            *intPtr -= 1;
        }
        else {
            *intPtr += 1;
        }

        /* copy shared region variable value */
        num = *intPtr;

        /* 7. leave Gate */
        GateMP_leave (Module.remoteGateMPHandle, gateKey);

        Log_print1(Diags_INFO,"Server_exec: Current shared region variable "
                "value %d",num);

        Task_sleep(1);
    }

    Log_print0(Diags_INFO,"Server_exec: Finished modifying shared region "
            "variable");

leave:
    Log_print1(Diags_EXIT, "<-- Server_exec: %d", (IArg)status);
    return (status);
}

/*
 *  ======== Server_delete ========
 *  1. wait for shutdown command 
 *  2. close remote GateMP 
 *  3. unregister notify callback
 *  4. delete sync object 
 */
    
Int Server_delete()
{
    Int     status = 0; 
    UInt32  event;

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

    /* 2. close remote GateMP */
    GateMP_close(&Module.remoteGateMPHandle);

    /* send GateMP cleanup command */
    status = Notify_sendEvent(Module.remoteProcId, Module.lineId, 
            Module.eventId, APP_CMD_GATEMPCLEANUP, TRUE);

    if (status < 0 )  {
        Log_error0("Server_delete: Error sending GateMP cleanup command "
                "acknowledge");
        goto leave;
    }

    /* 3. unregister notify callback */
    status = Notify_unregisterEvent(Module.remoteProcId, Module.lineId, 
            Module.eventId, Server_notifyCB,(UArg) &Module.eventQueue);

    if (status < 0) {
        Log_error0("Server_delete: Unregistering event has failed");
        goto leave;
    }

    /* 4. delete sync object */
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

