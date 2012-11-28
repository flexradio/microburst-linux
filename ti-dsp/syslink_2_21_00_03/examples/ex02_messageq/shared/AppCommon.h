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

/* notify commands 00 - FF */
#define App_CMD_MASK            0xFF000000
#define App_CMD_NOP             0x00000000  /* cc------ */
#define App_CMD_ACK             0x01000000  /* cc------ */
#define App_CMD_SHUTDOWN        0x02000000  /* cc------ */
#define App_CMD_PROCESS         0x03000000  /* cc------ */
#define App_CMD_DONE            0x04000000  /* cc------ */
#define App_CMD_ADDR_HI         0x05000000  /* cc--hhhh */
#define App_CMD_ADDR_LO         0x06000000  /* cc--llll */
#define App_CMD_SIZE            0x07000000  /* cc--ssss */
#define App_CMD_FINISH          0x08000000  /* cc-------*/

#define App_CMD_RESRDY          0x09000000  /* cc-------*/
#define App_CMD_READY           0x0A000000  /* cc-------*/
#define App_CMD_SDACK           0x0B000000  /* cc-------*/
#define App_CMD_CLOSED          0x0C000000  /* cc-------*/
#define App_CMD_DATAREQ         0x0D000000  /* cc-------*/

#define App_CMD_ANY             0xFF000000
#define App_E_OVERFLOW          0xFE000000
#define App_E_FAILURE           0xF0000000


typedef struct {
    MessageQ_MsgHeader  reserved;
    UInt32              cmd;
    SharedRegion_SRPtr  buf;
} App_Msg;

#define App_MsgHeapName         "MsgHeap:1"
#define App_MsgHeapSrId         1
#define App_MsgHeapId           1
#define App_HostMsgQueName      "HOST:MsgQ:01"
#define App_VideoMsgQueName     "VIDEO:MsgQ:01"


#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */
#endif /* AppCommon__include */
