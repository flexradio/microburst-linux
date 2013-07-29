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
#include <stdlib.h>
#include <semaphore.h>

/* package header files */
#include <ti/syslink/Std.h>     /* must be first */
#include <ti/ipc/Notify.h>

/* local header files */
#include "../shared/AppCommon.h"
#include "../shared/SystemCfg.h"
#include "App.h"

/* max number of outstanding commands minus one */
#define QUEUESIZE   8   
#define SEED        13
#define LOOP_MAX    5


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
    UInt16          lineId;     /* notify line id */
    UInt32          eventId;    /* notify event id */
} App_Module;

/* private functions */
static UInt32 App_waitForEvent(Event_Queue* eventQueue);
static Void App_notifyCB( UInt16 procId, UInt16 lineId, UInt32 eventId, 
        UArg arg, UInt32 payload);
        
/* private data */
static App_Module Module;

/*
 *  ======== App_create ========
 *
 *  1. create sync object
 *  2. register notify callback
 *  3. wait until remote core has also registered notify callback
 */

Int App_create(UInt16 remoteProcId)
{
    Int status;
    int retStatus;
    
    printf("--> App_create:\n");
    
    /* setting default values */
    Module.eventQueue.head      = 0;               
    Module.eventQueue.tail      = 0;              
    Module.eventQueue.error     = 0;       
    Module.lineId               = SystemCfg_LineId;
    Module.eventId              = SystemCfg_EventId;
    Module.remoteProcId         = remoteProcId;
    
    srand (SEED);
    
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

    printf("App_create: Host is ready\n");

leave: 
    printf("<-- App_create:\n");
    return (status);
}


/*
 *  ======== App_exec ========
 */
Int App_exec()
{
    Int     status  = 0;
    UInt32  event   = 0;    
    UInt32  x;
    UInt16  num     = 0;
    UInt32  command = 0;
    
    printf("--> App_exec:\n");

    for (x=0; x<LOOP_MAX; x++) {
    
        if (num == 0 || rand() % 2 == 0) {
            printf("App_exec: Sending INC command with payload: %d\n",num);
            command = APP_CMD_INC_PAYLOAD|num;
        }
        else {
            printf("App_exec: Sending DEC command with payload: %d\n",num);
            command = APP_CMD_DEC_PAYLOAD|num;
        }

        status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
                Module.eventId,command, TRUE);

        if (status < 0) {
            printf("App_exec: Sending command failed\n");
            goto leave;
        }

        event = App_waitForEvent(&Module.eventQueue);
        if (event >= APP_E_FAILURE) {
            printf("App_exec: Received queue error: %d\n",event);       
            status = -1;     
            goto leave;
        }

        num = (UInt16) (event & APP_PAYLOAD_MASK);
        printf("App_exec: Received---> Operation complete with payload: %d\n"
                ,num);
    }

leave:
    printf("<-- App_exec:\n");
    return (status);
}

/*
 *  ======== App_delete ========
 *
 *  1. send shutdown event
 *  2. wait for shutdown acknowledgement event
 *  3. unregister notify callback
 *  4. delete sync object
 */
Int App_delete() 
{
    Int status = 0;
    UInt32 event = 0; 

    printf("--> App_delete:\n");
    
    printf("App_delete: Sending stop command\n");

    /* 1. send shutdown event */
    status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
            Module.eventId, APP_CMD_SHUTDOWN, TRUE);

    /* communication error has occurred */
    if (status < 0) {
        printf("App_delete: Sending stop command failed\n");
        goto leave;
    } 

    /* 2. wait for stop acknowledgement event */
    event = App_waitForEvent(&Module.eventQueue);

    if (event >= APP_E_FAILURE) {
        printf("App_delete: Received Queue error: %d\n",event);    
        status = -1;  
        goto leave;    
    }

    printf("App_delete: Received---> Stop has been acknowledged\n"); 

    /* 3. unregister notify callback */
    status = Notify_unregisterEvent(Module.remoteProcId, Module.lineId, 
            Module.eventId, App_notifyCB, (UArg) &Module.eventQueue);

    if (status < 0) {
        printf("App_delete: Unregistering notify callback failed\n"); 
        goto leave;
    }

    /* 4. delete sync object */
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
