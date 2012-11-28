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
#include <string.h>

/* package header files */
#include <ti/syslink/Std.h>     /* must be first */
#include <ti/syslink/utils/Memory.h>
#include <ti/ipc/Notify.h>
#include <ti/ipc/SharedRegion.h>
#include <ti/ipc/HeapBufMP.h>
#include <ti/ipc/ListMP.h>


/* local header files */
#include "../shared/AppCommon.h"
#include "../shared/SystemCfg.h"
#include "App.h"

/* max number of outstanding commands minus one */
#define QUEUESIZE       8   
#define TOTAL_STRINGS   3
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
    UInt16              lineId;             /* notify line id */
    UInt32              eventId;            /* notify event id */
    HeapBufMP_Handle    HeapBufMP_handle;
    ListMP_Handle       ListMP_localHandle;
    ListMP_Handle       ListMP_remoteHandle;
} App_Module;

/* private functions */
static UInt32 App_waitForEvent(Event_Queue* eventQueue);
static Void App_notifyCB( UInt16 procId, UInt16 lineId, UInt32 eventId, 
        UArg arg, UInt32 payload);
        
/* private data */
static App_Module Module;

/*
 *  ======== App_create ========
 *  1. create sync object
 *  2. register notify callback
 *  3. wait until remote core has also registered notify callback
 *  4. create local HeapBufMP
 *  5. create local ListMP
 *  6. send ListMP created command
 *  7. open remote ListMP
 */

Int App_create(UInt16 remoteProcId)
{
    Int status                      = 0;
    int retStatus                   = 0;
    UInt32              event       = 0;
    HeapBufMP_Params    heapParams;
    ListMP_Params       listParams;
    
    printf("--> App_create:\n");
    
    /* setting default values */
    Module.eventQueue.head      = 0;               
    Module.eventQueue.tail      = 0;              
    Module.eventQueue.error     = 0;       
    Module.lineId               = SystemCfg_LineId;
    Module.eventId              = SystemCfg_EventId;
    Module.remoteProcId         = remoteProcId;
    Module.HeapBufMP_handle     = 0;
    Module.ListMP_localHandle   = 0;
    Module.ListMP_remoteHandle  = 0;
    
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
    HeapBufMP_Params_init(&heapParams);
    heapParams.name         = GPP_HEAPBUFMP_NAME;
    heapParams.regionId     = SHARED_REGION_1;
    heapParams.align        = ALIGN_SIZE;
    heapParams.blockSize    = BLOCK_SIZE;
    heapParams.numBlocks    = NUM_BLOCKS;
    heapParams.gate         = NULL;                 /* use default gate */

    Module.HeapBufMP_handle = HeapBufMP_create(&heapParams);

    if (Module.HeapBufMP_handle == NULL) {
        printf("App_create: Creating HeapBufMP failed\n");
        status = -1;
        goto leave;
    }

    /* 5. create local ListMP */
    ListMP_Params_init(&listParams);
    listParams.name     = GPP_LISTMP_NAME;
    listParams.regionId = SHARED_REGION_1;

    Module.ListMP_localHandle = ListMP_create (&listParams);

    if (Module.ListMP_localHandle == NULL) {
        printf("App_create: Creating ListMP has failed\n");
        status = -1;
        goto leave;
    }

    /* 6. send ListMP created command */
    status = Notify_sendEvent(Module.remoteProcId, Module.lineId, 
            Module.eventId, APP_CMD_LIST_CREATED, TRUE);

    if (status < 0 )  {
        printf("App_create: Failed to send ListMP created command\n");
        goto leave;
    }    

    /* wait for ListMP created command */
    event = App_waitForEvent(&Module.eventQueue);

    if (event >= APP_E_FAILURE) {
        printf("App_create: Received queue error: %d\n",event);
        status = -1;
        goto leave;
    }

    /* 7. open remote ListMP */
    status = ListMP_open(SLAVE_LISTMP_NAME,&Module.ListMP_remoteHandle);
    
    if (status < 0) {
        printf("App_create: Failed to open remote ListMP\n");
        goto leave;
    }

    printf("App_create: Host is ready\n");

leave: 
    printf("<-- App_create:\n");
    return (status);
}


/*
 *  ======== App_exec ========
 *  1. allocate node for ListMP
 *  2. add node to the beginning of ListMP
 *  3. send process ListMP command
 *  4. grab node from the tail of the remote ListMP
 *  5. delete node since it has been removed from the remote ListMP
 */
Int App_exec()
{
    Int                 status                              = 0;
    UInt32              event                               = 0;
    UInt16              x                                   = 0;
    ListMP_Node*        node                                = 0;
    Char                lowercaseStr[TOTAL_STRINGS][100];

    printf("--> App_exec:\n");
    
    strcpy(lowercaseStr[0],"texas instruments");
    strcpy(lowercaseStr[1],"host");
    strcpy(lowercaseStr[2],"syslink");

    for(x = 0;x<TOTAL_STRINGS;x++) {

        /* 1. allocate node for ListMP */
        node = (ListMP_Node *) HeapBufMP_alloc(Module.HeapBufMP_handle, 
            sizeof(ListMP_Node), 0);

        if (node == NULL) {
            printf("App_exec: Failed to allocate node\n");
            status = -1;
            goto leave;
        }

        printf("App_exec: Adding string \"%s\" to the head of the ListMP\n",
        lowercaseStr[x]);

        /* copy string to node's buffer */
        strcpy(node->buffer,lowercaseStr[x]);

        /* 2. add node to the beginning of ListMP */
        status = ListMP_putHead(Module.ListMP_localHandle,(ListMP_Elem *) node);

        if(status < 0) {
            printf("App_exec: Failed to add node to the head of the local "
                    "ListMP\n");
            goto leave;
        }
    }

    printf("App_exec: Command the remote core to convert the lowercase strings to "
            "uppercase\n");

    /* 3. send process ListMP command */
    status = Notify_sendEvent(Module.remoteProcId, Module.lineId, 
            Module.eventId, APP_CMD_PROCESS_LIST, TRUE);

    if (status < 0) {
        printf("App_exec: Failed to send process ListMP command\n");
        goto leave;
    }


    /* wait for operation complete command*/
    event = App_waitForEvent(&Module.eventQueue);

    if (event >= APP_E_FAILURE) {
        printf("App_exec: Received queue error: %d\n",event);
        status = -1;
        goto leave;
    }

    printf("App_exec: Received-> Operation complete\n");

    /* grab all nodes from the remote ListMP */
    while (ListMP_empty(Module.ListMP_remoteHandle) == FALSE) {
        
        /* 4. grab node from the tail of the remote ListMP */
        node = (ListMP_Node*)ListMP_getTail(Module.ListMP_remoteHandle);

        printf("App_exec: Transformed string: \"%s\"\n",node->buffer);

        /* 5. delete node since it has been removed from the remote ListMP */
        HeapBufMP_free(Module.HeapBufMP_handle,node,sizeof(ListMP_Node));
    } 

leave:
    printf("<-- App_exec:\n");
    return (status);
}

/*
 *  ======== App_delete ========
 *  1. sending shutdown command
 *  2. close remote instance of ListMP
 *  3. send ListMP cleanup command
 *  4. delete local ListMP
 *  5. delete HeapBufMP
 *  6. unregister notify callback
 *  7. delete sync object
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
    
    /* 2. close remote instance of ListMP */
    status = ListMP_close(&Module.ListMP_remoteHandle);
     
    if( status < 0) {
        printf("App_delete: Failed to close remote ListMP\n");
        goto leave;
    }

    /* 3. send ListMP cleanup command */
    status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
            Module.eventId, APP_CMD_CLEANUP_LIST, TRUE);

    if (status < 0) {
        printf("App_delete: Error sending cleanup ListMP command\n");
        goto leave;
    }

    /* wait for ListMP cleanup command */
    event = App_waitForEvent(&Module.eventQueue);

    if (event >= APP_E_FAILURE) {
        printf("App_delete: Received queue error: %d\n",event);
        status = -1;
        goto leave;
    }

    /* 4. delete local ListMP */ 
    status = ListMP_delete(&Module.ListMP_localHandle);

    if (status < 0) {
        printf("App_delete: Failed to delete ListMP\n");
        goto leave;
    }

    /* 5. delete HeapBufMP */
    HeapBufMP_delete(&Module.HeapBufMP_handle);

    /* 6. unregister notify callback */
    status = Notify_unregisterEvent(Module.remoteProcId, Module.lineId, 
            Module.eventId, App_notifyCB, (UArg) &Module.eventQueue);

    if (status < 0) {
        printf("App_delete: Unregistering notify callback failed\n"); 
        goto leave;
    }

    /* 7. delete sync object */
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
