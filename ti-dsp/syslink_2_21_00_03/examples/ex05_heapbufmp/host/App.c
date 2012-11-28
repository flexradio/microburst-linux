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
#include <ti/ipc/HeapBufMP.h>

/* local header files */
#include "../shared/AppCommon.h"
#include "../shared/SystemCfg.h"
#include "App.h"

/* max number of outstanding commands minus one */
#define QUEUESIZE   8   
#define TI_STR      "texas instruments"

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
    Event_Queue         eventQueue;
    UInt16              remoteProcId;
    UInt16              lineId;                 /* notify line id */
    UInt32              eventId;                /* notify event id */
    Char*               local_bufferPtr;        /* local string buffer */
    HeapBufMP_Handle    HeapBufMP_localHandle;  /* handle to local HeapBufMP */
} App_Module;

/* private functions */
static UInt32 App_waitForEvent(Event_Queue* eventQueue);
static Void App_notifyCB( UInt16 procId, UInt16 lineId, UInt32 eventId, 
        UArg arg, UInt32 payload);
        
/* private data */
static App_Module Module;

/*sends
 *  ======== App_create ========
 *  1. create sync object
 *  2. register notify callback
 *  3. wait until remote core has also registered notify callback
 *  4. create local HeapBufMP
 *  5. allocate block of memory from local HeapBufMP
 *  6. send heap created command
 *  7. wait for heap created acknowledge command
 */

Int App_create(UInt16 remoteProcId)
{
    Int                 status;
    int                 retStatus;
    UInt32              event;
    HeapBufMP_Params    heapParams;
    
    printf("--> App_create:\n");
    
    /* setting default values */
    Module.eventQueue.head          = 0;
    Module.eventQueue.tail          = 0;
    Module.eventQueue.error         = 0;
    Module.lineId                   = SystemCfg_LineId;
    Module.eventId                  = SystemCfg_EventId;
    Module.remoteProcId             = remoteProcId;
    Module.local_bufferPtr          = NULL;
    Module.HeapBufMP_localHandle    = NULL;
    
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

    /* 3. wait until remote core has also registered notify callback */
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

    /* 4. create local HeapBufMP */
    HeapBufMP_Params_init (&heapParams);

    heapParams.name         =   GPP_HEAPBUFMP_NAME;
    heapParams.regionId     =   SHARED_REGION_1;
    heapParams.align        =   ALIGN_SIZE;
    heapParams.blockSize    =   BLOCK_SIZE;
    heapParams.numBlocks    =   NUM_BLOCKS;
    heapParams.gate         =   NULL;                   /* use default gate */

    Module.HeapBufMP_localHandle = HeapBufMP_create(&heapParams);

    if(Module.HeapBufMP_localHandle == NULL) {
        printf("App_create: Creating local HeapBufMP has failed\n");
        status = -1;
        goto leave;
    }

    /* 5. allocate block of memory from local HeapBufMP */
    Module.local_bufferPtr = (Char*) HeapBufMP_alloc ( Module.HeapBufMP_localHandle,
            BLOCK_SIZE, 0);

    if(Module.local_bufferPtr == NULL) {
        printf("App_create: Failed to allocate memory in HeapBufMP\n");
        status = -1;
        goto leave;
    }
    
    /* 6. send heap created command */
    status = Notify_sendEvent(Module.remoteProcId,Module.lineId,
            Module.eventId, APP_CMD_HEAP_CREATED, TRUE);

    if (status < 0 ) {
        printf("App_create: Error sending heap created command\n");
        goto leave;
    }

    /* wait for heap created acknowledge command */
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
 *  4. wait for operation complete command
 *  5. wait for shared region pointer address lower two bytes
 *  6. save only the payload which represents the shared region pointer address
 *     lower two bytes
 *  7. wait for shared region pointer address upper two bytes
 *  8. join shared region pointer address upper and lower two bytes
 *  9. translate shared region pointer to local address space pointer
 */
Int App_exec()
{
    Int                 status              = 0;
    SharedRegion_SRPtr  sharedBufferPtr     = 0;
    UInt32              event               = 0;
    UInt32              command             = 0;
    Char*               remote_bufferPtr    = 0;

    printf("--> App_exec:\n");

    printf("App_exec: Writing string \"%s\" to buffer\n",TI_STR);
    sprintf(Module.local_bufferPtr,"%s",TI_STR);

    printf("App_exec: Command the remote core to convert the lowercase string"
            " to uppercase\n");

    /* 1. convert local address space pointer to generic shared region pointer */
    sharedBufferPtr = SharedRegion_getSRPtr(Module.local_bufferPtr,
            SHARED_REGION_1);

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

    /* 4. wait for operation complete command */
    event = App_waitForEvent(&Module.eventQueue);

    if (event >= APP_E_FAILURE) {
        printf("App_exec: Received queue error: %d\n",event); 
        status = -1;       
        goto leave;
    }        

    /* 5. wait for shared region pointer address lower two bytes */
    event = App_waitForEvent(&Module.eventQueue);

    if (event >= APP_E_FAILURE) {
        printf("App_exec: Received queue error: %d\n",event);
        status = -1;
        goto leave;
    }

    /* 6. save only the payload which represents the shared region pointer
     *    address lower two bytes
     */
    sharedBufferPtr = event & APP_SPTR_MASK;

    /* send shared region pointer address acknowledge */
    status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
            Module.eventId, APP_SPTR_ADDR_ACK, TRUE);

    if (status < 0 ) {
        printf("App_exec: Error sending shared region pointer address "
                "address acknowledge\n");
        goto leave;
    }

    /* 7. wait for shared region pointer address upper two bytes */
    event = App_waitForEvent(&Module.eventQueue);

    if (event >= APP_E_FAILURE) {
        printf("App_exec: Received queue error: %d\n",event);
        status = -1;
        goto leave;
    }

    /* 8. join shared region pointer address upper and lower two bytes */
    sharedBufferPtr = ((event & APP_SPTR_MASK) << 16) | sharedBufferPtr;

    /* send shared region pointer address acknowledge */
    status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
            Module.eventId, APP_SPTR_ADDR_ACK, TRUE);

    if (status < 0 ) {
        printf("App_exec: Error sending shared region pointer address "
                "address acknowledge\n");
        goto leave;
    }

    /* 9. translate shared region pointer to local address space pointer */
    remote_bufferPtr = (Char *) SharedRegion_getPtr(sharedBufferPtr);

    printf("App_exec: Transformed string: %s\n",remote_bufferPtr);

leave:
    printf("<-- App_exec:\n");
    return (status);
}

/*
 *  ======== App_delete ========
 *  1. sending shutdown command
 *  2. wait for cleanup heap command
 *  3. free string buffer memory from HeapBufMP
 *  4. delete local HeapBufMP
 *  5. unregister notify callback
 *  6. delete sync object
 */
Int App_delete() 
{
    Int             status      = 0;
    UInt32          event       = 0;

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

    /* 2. wait for cleanup heap command */
    event = App_waitForEvent(&Module.eventQueue);

    if (event >= APP_E_FAILURE) {
        printf("App_delete: Received queue error: %d\n",event);
        status = -1;
        goto leave;
    }
    
    /* 3. free string buffer memory from HeapBufMP */
    HeapBufMP_free(Module.HeapBufMP_localHandle,Module.local_bufferPtr
        ,BLOCK_SIZE);

    /* 4. delete local HeapBufMP */
    HeapBufMP_delete(&Module.HeapBufMP_localHandle);

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
