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
#include <ti/syslink/utils/Memory.h>
#include <ti/ipc/Notify.h>
#include <ti/ipc/SharedRegion.h>
#include <ti/ipc/GateMP.h>

/* local header files */
#include "../shared/AppCommon.h"
#include "../shared/SystemCfg.h"
#include "App.h"

/* max number of outstanding commands minus one */
#define QUEUESIZE   8   
#define LOOP_ITR    10

/* queue structure */
typedef struct 
{
    UInt32              queue[QUEUESIZE];   /* command queue */
    UInt                head;               /* queue head pointer */
    UInt                tail;               /* queue tail pointer */
    UInt32              error;              /* error flag */
    sem_t               semH;               /* handle to object above  */
} Event_Queue;   

/* module structure */
typedef struct {
    Event_Queue     eventQueue;
    UInt16          remoteProcId;
    UInt16          lineId;         /* notify line id */
    UInt32          eventId;        /* notify event id */
    Int*            intPtr;         /* Int pointer */
    GateMP_Handle   gateMPHandle; 
} App_Module;

/* private functions */
static UInt32 App_waitForEvent(Event_Queue* eventQueue);
static Void App_notifyCB( UInt16 procId, UInt16 lineId, UInt32 eventId, 
        UArg arg, UInt32 payload);
        
/* private data */
static App_Module Module;

/*
 *  ======== App_create ========
 */

Int App_create(UInt16 remoteProcId)
{
    Int             status;
    int             retStatus;
    IHeap_Handle    heap;
    GateMP_Params   gateParams;
    UInt32          event;
    
    printf("--> App_create:\n");
    
    /* setting default values */
    Module.eventQueue.head      = 0;               
    Module.eventQueue.tail      = 0;              
    Module.eventQueue.error     = 0;       
    Module.lineId               = SystemCfg_LineId;
    Module.eventId              = SystemCfg_EventId;
    Module.remoteProcId         = remoteProcId;
    Module.gateMPHandle         = NULL;
    
    /* 1. create sync object */
    retStatus = sem_init(&Module.eventQueue.semH, 0, 0);
    if(retStatus == -1) {
        printf("App_create: Could not initialize a semaphore\n");
        status = -1;
        goto leave;
    }

    /* 2. register notify callback */
    status = Notify_registerEvent(Module.remoteProcId, Module.lineId, 
            Module.eventId, App_notifyCB, (UArg) &Module.eventQueue);

    if (status < 0) {
        printf("App_create: Host failed to register event\n");
        goto leave;
    }

    /* wait until remote core has also registered notify callback */
    do {
        status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
                Module.eventId,APP_CMD_NOP, TRUE);

        if (status == Notify_E_EVTNOTREGISTERED) {
            usleep(100);
        }
        
    } while (status == Notify_E_EVTNOTREGISTERED);

    if (status != Notify_S_SUCCESS) {
        printf("App_create: Waiting for remote core to register has failed\n");
        status = -1;
        goto leave;
    }

    /* 3. get handle to shared region 1 heap */
    heap = (IHeap_Handle) SharedRegion_getHeap(SHARED_REGION_1);

    if (heap == NULL) {
        status = -1;
        printf("App_create: Shared region heap does not exist\n");
        goto leave;
    }

    /* 4. create UInt from shared region 1 heap */
    Module.intPtr = (Int *) Memory_calloc(heap,sizeof(Int),0,NULL);
    
    if(Module.intPtr == NULL) {
        printf("App_create: Failed to create buffer from shared region 1 "
                "heap\n");
        status = -1;
        goto leave;
    }

    /* 5. create GateMP */
    GateMP_Params_init (&gateParams);

    gateParams.name             = GATEMP_NAME;
    gateParams.regionId         = SHARED_REGION_1;
    gateParams.localProtect     = GateMP_LocalProtect_PROCESS;
    gateParams.remoteProtect    = GateMP_RemoteProtect_SYSTEM;

    Module.gateMPHandle = GateMP_create (&gateParams);

    if (Module.gateMPHandle == NULL) {
        status = -1;
        printf("App_create: Failed to create GateMP\n");
        goto leave;
    }

    /* send GateMP created command */
    status = Notify_sendEvent(Module.remoteProcId, Module.lineId, 
            Module.eventId, APP_CMD_GATECREATED, TRUE);

    if (status < 0 )  {
        printf("App_create: Error sending Gate created command\n");
        goto leave;
    }

    /* wait for GateMP created acknowledgement */
    event = App_waitForEvent(&Module.eventQueue);

    if (event >= APP_E_FAILURE) {
        printf("App_create: Received queue error: %d\n",event);       
        status = -1;     
        goto leave;
    }
    
    printf("App_create: Host is ready\n");

leave: 
    printf("<-- App_create:\n");
    return (status);
}


/*
 *  ======== App_exec ========
 *  1. convert local address space pointer to generic shared region pointer
 *  2. send shared region pointer address lower two bytes
 *  3. send shared region pointer address upper two bytes
 *  4. enter GateMP
 *  5. exit GateMP
 */
Int App_exec()
{
    Int                 status          = 0;
    UInt32              command         = 0;
    SharedRegion_SRPtr  sharedBufferPtr = 0;
    UInt32              event           = 0;
    IArg                gateKey         = 0;
    Int                 num             = 0;
    UInt                x               = 0;
     
    printf("--> App_exec:\n");

    /* set shared variable initial value */
    *Module.intPtr = 500;
            
    /* 1. convert local address space pointer to generic shared region pointer */
    sharedBufferPtr = SharedRegion_getSRPtr(Module.intPtr, SHARED_REGION_1);
    
    /* store only the lower two bytes of the shared region address pointer */
    command = (sharedBufferPtr & 0xFFFF);
    
    /* add shared region pointer address low command to payload */
    command = APP_SPTR_LADDR | command;

    /* 2. send shared region pointer address lower two bytes */
    status = Notify_sendEvent(Module.remoteProcId, Module.lineId, 
            Module.eventId, command, TRUE);

    if (status < 0 )  {
        printf("App_exec: Error sending shared region pointer address lower "
                "two bytes\n");
        goto leave;
    }
    
    /* wait for shared region address acknowledge command */
    event = App_waitForEvent(&Module.eventQueue);

    if (event >= APP_E_FAILURE) {
        printf("App_exec: Received queue error: %d\n",event); 
        status = -1;       
        goto leave;
    }
    
    /* store only the upper two bytes of the shared region address pointer */
    command = ((sharedBufferPtr >> 16) & 0xFFFF);

    /* add shared region pointer address high command to payload */
    command = APP_SPTR_HADDR | command;
    
    /* 3. send shared region pointer address upper two bytes */
    status = Notify_sendEvent(Module.remoteProcId, Module.lineId, 
            Module.eventId, command, TRUE);

    if (status < 0 )  {
        printf("App_exec: Error sending shared region pointer address upper "
                "two bytes\n");
        goto leave;
    }    
       
    /* wait for shared region address acknowledge command */
    event = App_waitForEvent(&Module.eventQueue);

    if (event >= APP_E_FAILURE) {
        printf("App_exec: Received queue error: %d\n",event); 
        status = -1;       
        goto leave;
    } 
   
    printf("App_exec: Reading shared region variable value\n");
 
    for(x = 0; x < LOOP_ITR; x++) {

        /* read the shared variable as long as no one is currently modifying
         * it
         */

        /* 4. enter GateMP */
        gateKey = GateMP_enter (Module.gateMPHandle);

        /* copy shared region value */
        num = *Module.intPtr;

        /* 5. exit GateMP */
        GateMP_leave (Module.gateMPHandle,gateKey);

        printf("App_exec: Current shared region variable value: %d\n",num);
    }

    printf("App_exec: Finished reading shared region variable\n");

leave:
    printf("<-- App_exec:\n");
    return (status);
}

/*
 *  ======== App_delete ========
 *  1. sending shutdown command
 *  2. wait for cleanup GateMP command
 *  3. delete GateMP
 *  4. free shared region variable memory
 *  5. unregister notify callback
 *  6. delete sync object
 */
Int App_delete() 
{
    Int             status      = 0;
    UInt32          event       = 0;
    IHeap_Handle    heap; 

    printf("--> App_delete:\n");
   
    /* 1. sending shutdown command */
    status = Notify_sendEvent(Module.remoteProcId, Module.lineId, 
        Module.eventId, APP_CMD_SHUTDOWN, TRUE);

    if (status < 0 )  {
        printf("App_delete: Error sending shutdown command\n");
        goto leave;
    }    
    
    /* wait for shutdown acknowledge command */                               
    event = App_waitForEvent(&Module.eventQueue);

    if (event >= APP_E_FAILURE) {
        printf("App_delete: Received queue error: %d\n",event); 
        status = -1;       
        goto leave;
    }  
    
    /* 2. wait for cleanup GateMP command */                               
    event = App_waitForEvent(&Module.eventQueue);

    if (event >= APP_E_FAILURE) {
        printf("App_delete: Received queue error: %d\n",event); 
        status = -1;       
        goto leave;
    }    
   
    /* 3. delete GateMP */
    GateMP_delete(&Module.gateMPHandle);
     
    /* 4. free shared region variable memory */
    heap = (IHeap_Handle) SharedRegion_getHeap(SHARED_REGION_1);
    
    Memory_free(heap,Module.intPtr,sizeof(Int));

    /* 5. unregister notify callback */
    status = Notify_unregisterEvent(Module.remoteProcId, Module.lineId, 
            Module.eventId, App_notifyCB, (UArg) &Module.eventQueue);

    if (status < 0) {
        printf("App_delete: Unregistering notify callback failed\n"); 
        goto leave;
    }

    /* 6. delete sync object */
    sem_destroy(&Module.eventQueue.semH);
    
    printf("App_delete: Cleanup complete\n");

leave:
    printf("<-- App_delete:\n");
    return (status);
}

/*
 *  ======== App_notifyCB ========
 */
Void App_notifyCB(UInt16 procId, UInt16 lineId, UInt32 eventId, UArg arg,
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
        /* queue head is only written to within this function */
        eventQueue->queue[eventQueue->head] = payload;
        eventQueue->head = next;
        
        /* signal semaphore (counting) that new event is in queue */
        sem_post(&eventQueue->semH);
    }

    return;
}

/*
 *  ======== App_waitForEvent ========
 */
static UInt32 App_waitForEvent(Event_Queue* eventQueue)
{
    UInt32 event;

    if (eventQueue->error >= APP_E_FAILURE) {
        event = eventQueue->error;
    }
    else {
        /* use counting semaphore to wait for next event */
        sem_wait(&eventQueue->semH);

        /* remove next command from queue */
        event = eventQueue->queue[eventQueue->tail];
        
        /* queue tail is only written to within this function */
        eventQueue->tail = (eventQueue->tail + 1) % QUEUESIZE;
    }

    return (event);
}
