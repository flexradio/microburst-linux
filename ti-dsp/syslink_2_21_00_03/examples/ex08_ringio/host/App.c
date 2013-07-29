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
#include <ti/syslink/RingIO.h>
#include <ti/syslink/RingIOShm.h>
#include <ti/ipc/Notify.h>

/* local header files */
#include "../shared/AppCommon.h"
#include "../shared/SystemCfg.h"
#include "App.h"

/* max number of outstanding commands minus one */
#define QUEUESIZE       8   
#define ACQUIRED_SIZE   12

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
    RingIO_Handle   rio_Handle;
    RingIO_Handle   rio_WriterHandle;
} App_Module;

/* private functions */
static UInt32 App_waitForEvent(Event_Queue* eventQueue);
static Void App_notifyCB( UInt16 procId, UInt16 lineId, UInt32 eventId, 
        UArg arg, UInt32 payload);

/* private data */
static  App_Module  Module;

const   Char        file_string [] = "Welcome to Texas Instrument. A leading "
                                     "semiconductor company based in Dallas, "
                                     "Texas. You are currently running the "
                                     "SysLink examples";
/*
 *  ======== App_create ========
 *  1. create sync object
 *  2. wait until remote core has also registered notify callback
 *  3. create RingIO component
 *  4. open RingIO for writing
 *  5. notify slave that the RingIO has been created
 */
Int App_create(UInt16 remoteProcId)
{
    Int                 status              = 0;
    int                 retStatus           = 0;
    UInt16              event               = 0;
    RingIOShm_Params    rioParams;
    RingIO_openParams   rio_OpenParams;

    printf("--> App_create:\n");
    
    /* setting default values */
    Module.eventQueue.head      = 0;               
    Module.eventQueue.tail      = 0;              
    Module.eventQueue.error     = 0;       
    Module.lineId               = SystemCfg_LineId;
    Module.eventId              = SystemCfg_EventId;
    Module.remoteProcId         = remoteProcId;

    /* 1. create sync object */
    retStatus = sem_init(&Module.eventQueue.semH, 0, 0);
    if(retStatus == -1) {
        printf("App_create: Could not initialize a semaphore\n");
        status = -1;
        goto leave;
    }

    status = Notify_registerEvent(Module.remoteProcId, Module.lineId, 
            Module.eventId, App_notifyCB, (UArg) &Module.eventQueue);

    if (status < 0) {
        printf("App_create: Host failed to register event\n");
        goto leave;
    }

    /* 2. wait until remote core has also registered notify callback */
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

    /* 3. create RingIO component */
    RingIOShm_Params_init (&rioParams);
    rioParams.commonParams.name     = GPP_RINGIO_NAME;
    rioParams.ctrlRegionId          = SHARED_REGION_1;
    rioParams.dataRegionId          = SHARED_REGION_1;
    rioParams.attrRegionId          = SHARED_REGION_1;
    rioParams.attrSharedAddrSize    = 16;
    rioParams.dataSharedAddrSize    = 40;     
    rioParams.remoteProcId          = Module.remoteProcId;
    rioParams.gateHandle            = NULL;                     /* use default system gate */

    Module.rio_Handle              = RingIO_create(&rioParams);

    if(Module.rio_Handle == NULL) {
        printf("App_create: Failed to create RingIO_1\n");
        status = -1;
        goto leave;
    }

    /* 4. open RingIO for writing */
    rio_OpenParams.openMode         = RingIO_MODE_WRITER;
    rio_OpenParams.flags            = 0;

    status = RingIO_open (GPP_RINGIO_NAME,&rio_OpenParams,NULL,
            &Module.rio_WriterHandle);

    if (status < 0) {
        printf("App_create: Failed to open RingIO\n");
        goto leave;
    }

    /* 5. notify slave that the RingIO has been created */
    status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
            Module.eventId, APP_CMD_RINGIO_CREATED, TRUE);

    if (status < 0) {
        printf("App_create: Sending command failed\n");
        goto leave;
    }

    /* wait for RingIO  acknowledgement */
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
 *  1. loop until the entire string has been transfered
 *  2. acquire a portion of the RingIO data buffer
 *  3. only copy the amount of bytes based on the actual size of the buffer that
 *     was acquired
 *  4. release the acquired buffer
 */
Int App_exec()
{
    Int     status          = 0;
    UInt16  fileIndex       = 0;
    Char*   buffer          = 0;
    UInt32  acquiredSize    = 0;
    UInt16  strLen          = 0;

    printf("--> App_exec:\n");

    /* get the length of the file string including the null termination character */
    strLen =  strlen(file_string) + 1;

    /* 1. loop until the entire string has been transfered */
    while (fileIndex < strLen) {

        /* must be 4 bytes aligned */
        acquiredSize = ACQUIRED_SIZE;

        /* make sure we aren't acquiring more data then we need */
        if ((fileIndex + acquiredSize) >= strLen) {

            /* only acquire enough data to send the remaining parts
             * of the string
             */ 
            acquiredSize = strLen - fileIndex;
        }

        /* 2. acquire a portion of the RingIO data buffer */
        status = RingIO_acquire(Module.rio_WriterHandle,(RingIO_BufPtr*)&buffer,
                &acquiredSize);

        switch (status) {

            case RingIO_E_BUFFULL:  /* acquired less then what was requested 
                                     * or acquired nothing
                                     */

                /* check to see if RingIO buffer is completely full therefore
                 * no space has been acquired 
                 */

                if (acquiredSize == 0) {

                    /* sleep to allow the reader to catchup which will free
                     * up some space
                     */
                    usleep(10);
                    continue;
                }

            case RingIO_S_SUCCESS:  /* the requested acquired size was returned */
            case RingIO_E_BUFWRAP:  /* acquired less then what was requested */ 

                /* check to if we are acquiring space for the first time */
                if( fileIndex == 0) {

                    /* set file header attribute and also include the file size 
                     * as a payload
                     */
                    status = RingIO_setAttribute(Module.rio_WriterHandle,
                            ATTRIBUTE_FILE_HEADER,strLen, FALSE);

                    if( status < 0 ) {
                        printf("App_exec: Failed to set file header attribute"
                                "\n");
                        goto leave;
                    }

                    printf("App_exec: Added file header attribute to acquired "
                            "data buffer\n");

                }
                /* check to see if this will be our last time acquiring data */
                else if((fileIndex + acquiredSize) == strLen) {

                    /* set end of file attribute */
                    status = RingIO_setAttribute(Module.rio_WriterHandle,
                            ATTRIBUTE_EOF,0, FALSE);

                    if( status < 0 ) {
                        printf("App_exec: Failed to set EOF attribute\n");
                        goto leave;
                    }

                    printf("App_exec: Added end of file attribute to acquired "
                            "data buffer\n");
                }

                /* 3. only copy the amount of bytes based on the actual size of the
                 *    buffer that was acquired 
                 */
                memcpy(buffer,&file_string[fileIndex],acquiredSize);

                /* move the file index past the characters we already sent */
                fileIndex += acquiredSize;

                /* 4. release the acquired buffer */
                status = RingIO_release(Module.rio_WriterHandle,
                        acquiredSize);

                if(status < 0) {
                    printf("App_exec: Failed to release acquired buffer\n");
                    goto leave;
                }

                printf("App_exec: Acquired and wrote %d of data to the RingIO "
                        "buffer\n", acquiredSize);

                break;
            
            default:
                printf("App_exec: Received unexpected RingIO_acquire status %d\n"
                        ,status);
                goto leave;
                break;
        }
    }

    printf("App_exec: File transfer complete\n");
    

leave:
    printf("<-- App_exec:\n");
    return (status);
}

/*
 *  ======== App_delete ========
 *  1. send shutdown event
 *  2. wait for shutdown acknowledge
 *  3. wait for RingIO closed command
 *  4. close writer RingIO handle
 *  5. delete RingIO handle
 *  6. unregister notify callback
 */
Int App_delete() 
{
    Int status = 0;
    UInt32 event = 0; 

    printf("--> App_delete:\n");
    
    printf("App_delete: Sending stop command\n");

    /* 1. send shutdown event */
    status = Notify_sendEvent(Module.remoteProcId, Module.lineId,
            Module.eventId,APP_CMD_SHUTDOWN, TRUE);

    /* communication error has occurred */
    if (status < 0) {
        printf("App_delete: Sending stop command failed\n");
        goto leave;
    } 

    /* 2. wait for shutdown acknowledge */
    event = App_waitForEvent(&Module.eventQueue);

    if (event >= APP_E_FAILURE) {
        printf("App_delete: Received queue error: %d\n",event);       
        status = -1;     
        goto leave;
    }

    printf("App_delete: Received---> Shutdown has been acknowledged\n");

    /* 3. wait for RingIO closed command */
    event = App_waitForEvent(&Module.eventQueue);

    if (event >= APP_E_FAILURE) {
        printf("App_delete: Received queue error: %d\n",event);       
        status = -1;     
        goto leave;
    }

    /* 4. close writer RingIO handle */
    status = RingIO_close(&Module.rio_WriterHandle);

    if( status < 0 ) {
        printf("App_delete: Failed to close RingIO writer handle\n");
        goto leave;
    }

    /* 5. delete RingIO handle */
    status = RingIO_delete(&Module.rio_Handle);

    if( status < 0 ) {
        printf("App_delete: Failed to delete RingIO\n");
        goto leave;
    }

    /* 6. unregister notify callback */
    status = Notify_unregisterEvent(Module.remoteProcId, Module.lineId, 
            Module.eventId, App_notifyCB, (UArg) &Module.eventQueue);

    if (status < 0) {
        printf("App_delete: Unregistering notify callback failed\n"); 
        goto leave;
    }

    /* delete sync object */
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
