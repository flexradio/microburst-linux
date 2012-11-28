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
 *  ======== AppCommon.h ========
 *
 */

#ifndef AppCommon__include
#define AppCommon__include
#if defined (__cplusplus)
extern "C" {
#endif


/*
 *  ======== Application Configuration ========
 */

typedef struct ListMP_Node_Tag {
    ListMP_Elem elem;               /* the first variable in your data structure
                                     * must be of type ListMP_Elem 
                                     */
    Char        buffer[100];        
} ListMP_Node;

/* notify commands 00 - FF */
#define APP_CMD_MASK            0xFF000000

/* used by host and remote device to synchronize states with each other*/
#define APP_CMD_NOP             0x10000000  /* cc------ */
#define APP_CMD_LIST_CREATED    0x20000000  /* cc------ */
#define APP_CMD_PROCESS_LIST    0x30000000  /* cc------ */
#define APP_CMD_OP_COMPLETE     0x31000000  /* cc------ */
#define APP_CMD_CLEANUP_LIST    0x40000000  /* cc------ */
#define APP_CMD_SHUTDOWN_ACK    0x41000000  /* cc------ */
#define APP_CMD_SHUTDOWN        0x42000000  /* cc------ */

/* program's shared region number */
#define SHARED_REGION_1         0x1

/* HeapBufMP parameters */
#define ALIGN_SIZE              128
#define BLOCK_SIZE              256
#define NUM_BLOCKS              6 
#define GPP_HEAPBUFMP_NAME      "GPP_HMP"

/* ListMP parameters */
#define GPP_LISTMP_NAME         "GPP_LMP"
#define SLAVE_LISTMP_NAME       "SLAV_LMP"

/* queue error message*/
#define APP_E_FAILURE           0xF0000000
#define APP_E_OVERFLOW          0xF1000000

#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */
#endif /* AppCommon__include */
