/*
 *  Copyright (c) 2008-2012, Texas Instruments Incorporated
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  *  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  *  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  *  Neither the name of Texas Instruments Incorporated nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 *  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 *  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 *  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 *  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 *  OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 *  EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 *  @file   ti/syslink/family/common/ti81xx/ti81xxdsp/Dm8168DspProc.c
 *
 *  @brief      Processor implementation for DM8168DSP.
 *
 *              This module is responsible for taking care of device-specific
 *              operations for the processor. This module can be used
 *              stand-alone or as part of ProcMgr.
 *              The implementation is specific to DM8168DSP.
 */

#if defined(SYSLINK_BUILD_RTOS)
#include <xdc/std.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/Memory.h>
#include <xdc/runtime/System.h>
#include <xdc/runtime/IGateProvider.h>
#include <ti/sysbios/gates/GateMutex.h>
#endif /* #if defined(SYSLINK_BUILD_RTOS) */

#if defined(SYSLINK_BUILD_HLOS)
#include <ti/syslink/Std.h>
/* OSAL & Utils headers */
#include <ti/syslink/utils/String.h>
#include <ti/syslink/utils/IGateProvider.h>
#include <ti/syslink/utils/GateMutex.h>
#include <ti/syslink/utils/Memory.h>
#endif /* #if defined(SYSLINK_BUILD_HLOS) */

#if defined(SYSLINK_BUILDOS_LINUX)
#include <linux/string.h>
#else
#include <string.h>
#endif

#include <ti/syslink/utils/Cfg.h>
#include <ti/syslink/utils/Trace.h>
/* Module level headers */
#include <ti/syslink/inc/knl/ProcDefs.h>
#include <ti/syslink/inc/knl/Processor.h>
#include <ti/syslink/inc/knl/Dm8168DspProc.h>
#include <ti/syslink/inc/knl/_Dm8168DspProc.h>
#include <ti/syslink/inc/knl/Dm8168DspHal.h>
#include <ti/syslink/inc/knl/Dm8168DspHalReset.h>
#include <ti/syslink/inc/knl/Dm8168DspHalBoot.h>
#include <ti/ipc/MultiProc.h>
#include <ti/syslink/inc/_MultiProc.h>


/* =============================================================================
 *  Macros and types
 * =============================================================================
 */

/*!
 *  @brief  Number of static entries in address translation table.
 */
#define AddrTable_STATIC_COUNT 5

/*!
 *  @brief  max entries in translation table.
 */
#define AddrTable_SIZE 32

/* config param for dsp mmu */
#define PARAMS_mmuEnable "ProcMgr.proc[DSP].mmuEnable="


/*!
 *  @brief  DM8168DSPPROC Module state object
 */
typedef struct DM8168DSPPROC_ModuleObject_tag {
    UInt32              configSize;
    /*!< Size of configuration structure */
    DM8168DSPPROC_Config cfg;
    /*!< DM8168DSPPROC configuration structure */
    DM8168DSPPROC_Config defCfg;
    /*!< Default module configuration */
    DM8168DSPPROC_Params      defInstParams;
    /*!< Default parameters for the DM8168DSPPROC instances */
    Bool                isSetup;
    /*!< Indicates whether the DM8168DSPPROC module is setup. */
    DM8168DSPPROC_Handle procHandles [MultiProc_MAXPROCESSORS];
    /*!< Processor handle array. */
    IGateProvider_Handle         gateHandle;
    /*!< Handle of gate to be used for local thread safety */
} DM8168DSPPROC_ModuleObject;

/*!
 *  @brief  DM8168DSPPROC instance object.
 */
typedef struct DM8168DSPPROC_Object_tag {
    DM8168DSPPROC_Params params;
    /*!< Instance parameters (configuration values) */
    Ptr                 halObject;
    /*!< Pointer to the Hardware Abstraction Layer object. */
    ProcMgr_Handle      pmHandle;
    /*!< Handle to proc manager */
} DM8168DSPPROC_Object;

/* Default memory regions */
static UInt32 AddrTable_count = AddrTable_STATIC_COUNT;

/* static memory regions
 * CAUTION: AddrTable_STATIC_COUNT must match number of entries below.
 */
static ProcMgr_AddrInfo AddrTable[AddrTable_SIZE] =
    {
        { /* L2 RAM */
            .addr     = { -1u, -1u, 0x40800000, 0x10800000, 0x10800000 },
            .size     = 0x40000,
            .isCached = FALSE,
            .isMapped = FALSE,
            .mapMask  = ProcMgr_MASTERKNLVIRT
        },
        { /* L1P RAM */
            .addr     = { -1u, -1u, 0x40E00000, 0x10E00000, 0x10E00000 },
            .size     = 0x8000,
            .isCached = FALSE,
            .isMapped = FALSE,
            .mapMask  = ProcMgr_MASTERKNLVIRT
        },
        { /* L1D RAM */
            .addr     = { -1u, -1u, 0x40F00000, 0x10F00000, 0x10F00000 },
            .size     = 0x8000,
            .isCached = FALSE,
            .isMapped = FALSE,
            .mapMask  = ProcMgr_MASTERKNLVIRT
        },
        { /* OCMC0, 256 KB */
            .addr     = { -1u, -1u, 0x40300000, 0x40300000, 0x40300000 },
            .size     = 0x40000,
            .isCached = FALSE,
            .isMapped = FALSE,
            .mapMask  = (ProcMgr_MASTERKNLVIRT | ProcMgr_SLAVEVIRT)
        },
        { /* OCMC1, 256 KB */
            .addr     = { -1u, -1u, 0x40400000, 0x40400000, 0x40400000 },
            .size     = 0x40000,
            .isCached = FALSE,
            .isMapped = FALSE,
            .mapMask  = (ProcMgr_MASTERKNLVIRT | ProcMgr_SLAVEVIRT)
        }
    };

/* =============================================================================
 *  Globals
 * =============================================================================
 */

/*!
 *  @var    DM8168DSPPROC_state
 *
 *  @brief  DM8168DSPPROC state object variable
 */
#if !defined(SYSLINK_BUILD_DEBUG)
static
#endif /* if !defined(SYSLINK_BUILD_DEBUG) */
DM8168DSPPROC_ModuleObject DM8168DSPPROC_state =
{
    .isSetup = FALSE,
    .configSize = sizeof(DM8168DSPPROC_Config),
    .gateHandle = NULL,
    .defInstParams.mmuEnable = FALSE,
    .defInstParams.numMemEntries = AddrTable_STATIC_COUNT
};

/* config override specified in SysLinkCfg.c, defined in ProcMgr.c */
extern String ProcMgr_sysLinkCfgParams;

/* =============================================================================
 * APIs directly called by applications
 * =============================================================================
 */
/*!
 *  @brief      Function to get the default configuration for the DM8168DSPPROC
 *              module.
 *
 *              This function can be called by the application to get their
 *              configuration parameter to DM8168DSPPROC_setup filled in by the
 *              DM8168DSPPROC module with the default parameters. If the user
 *              does not wish to make any change in the default parameters, this
 *              API is not required to be called.
 *
 *  @param      cfg        Pointer to the DM8168DSPPROC module configuration
 *                         structure in which the default config is to be
 *                         returned.
 *
 *  @sa         DM8168DSPPROC_setup
 */
Void
DM8168DSPPROC_getConfig (DM8168DSPPROC_Config * cfg)
{
    GT_1trace (curTrace, GT_ENTER, "DM8168DSPPROC_getConfig", cfg);

    GT_assert (curTrace, (cfg != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (cfg == NULL) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DSPPROC_getConfig",
                             PROCESSOR_E_INVALIDARG,
                             "Argument of type (DM8168DSPPROC_Config *) passed "
                             "is null!");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        memcpy(cfg, &(DM8168DSPPROC_state.defCfg),
            sizeof(DM8168DSPPROC_Config));
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */

    GT_0trace(curTrace, GT_LEAVE, "DM8168DSPPROC_getConfig");
}


/*!
 *  @brief      Function to setup the DM8168DSPPROC module.
 *
 *              This function sets up the DM8168DSPPROC module. This function
 *              must be called before any other instance-level APIs can be
 *              invoked.
 *              Module-level configuration needs to be provided to this
 *              function. If the user wishes to change some specific config
 *              parameters, then DM8168DSPPROC_getConfig can be called to get the
 *              configuration filled with the default values. After this, only
 *              the required configuration values can be changed. If the user
 *              does not wish to make any change in the default parameters, the
 *              application can simply call DM8168DSPPROC_setup with NULL
 *              parameters. The default parameters would get automatically used.
 *
 *  @param      cfg   Optional DM8168DSPPROC module configuration. If provided as
 *                    NULL, default configuration is used.
 *
 *  @sa         DM8168DSPPROC_destroy
 *              GateMutex_create
 */
Int
DM8168DSPPROC_setup (DM8168DSPPROC_Config * cfg)
{
    Int                  status = PROCESSOR_SUCCESS;
    DM8168DSPPROC_Config tmpCfg;
    Error_Block          eb;

    Error_init(&eb);

    GT_1trace (curTrace, GT_ENTER, "DM8168DSPPROC_setup", cfg);

    if (cfg == NULL) {
        DM8168DSPPROC_getConfig (&tmpCfg);
        cfg = &tmpCfg;
    }

    /* Create a default gate handle for local module protection. */
    DM8168DSPPROC_state.gateHandle = (IGateProvider_Handle)
                               GateMutex_create((GateMutex_Params *)NULL, &eb);
    if (DM8168DSPPROC_state.gateHandle == NULL) {
        /*! @retval PROCESSOR_E_FAIL Failed to create GateMutex! */
        status = PROCESSOR_E_FAIL;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DSPPROC_setup",
                             status,
                             "Failed to create GateMutex!");
    }
    else {
        /* Copy the user provided values into the state object. */
        memcpy (&DM8168DSPPROC_state.cfg,
                     cfg,
                     sizeof (DM8168DSPPROC_Config));

        /* Initialize the name to handles mapping array. */
        memset (&DM8168DSPPROC_state.procHandles,
                    0,
                    (sizeof (DM8168DSPPROC_Handle) * MultiProc_MAXPROCESSORS));
        DM8168DSPPROC_state.isSetup = TRUE;
    }


    GT_1trace (curTrace, GT_LEAVE, "DM8168DSPPROC_setup", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return (status);
}


/*!
 *  @brief      Function to destroy the DM8168DSPPROC module.
 *
 *              Once this function is called, other DM8168DSPPROC module APIs,
 *              except for the DM8168DSPPROC_getConfig API cannot be called
 *              anymore.
 *
 *  @sa         DM8168DSPPROC_setup
 *              GateMutex_delete
 */
Int
DM8168DSPPROC_destroy (Void)
{
    Int    status = PROCESSOR_SUCCESS;
    UInt16 i;

    GT_0trace (curTrace, GT_ENTER, "DM8168DSPPROC_destroy");

    /* Check if any DM8168DSPPROC instances have not been deleted so far. If not,
     * delete them.
     */
    for (i = 0 ; i < MultiProc_MAXPROCESSORS ; i++) {
        GT_assert (curTrace, (DM8168DSPPROC_state.procHandles [i] == NULL));
        if (DM8168DSPPROC_state.procHandles [i] != NULL) {
            DM8168DSPPROC_delete (&(DM8168DSPPROC_state.procHandles [i]));
        }
    }

    if (DM8168DSPPROC_state.gateHandle != NULL) {
        GateMutex_delete ((GateMutex_Handle *)
                                &(DM8168DSPPROC_state.gateHandle));
    }

    DM8168DSPPROC_state.isSetup = FALSE;

    GT_1trace (curTrace, GT_LEAVE, "DM8168DSPPROC_destroy", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return (status);
}


/*!
 *  @brief      Function to initialize the parameters for this Processor
 *              instance.
 *
 *  @param      params  Configuration parameters to be returned
 *
 *  @sa         DM8168DSPPROC_create
 */
Void
DM8168DSPPROC_Params_init(
        DM8168DSPPROC_Handle    handle,
        DM8168DSPPROC_Params *  params)
{
    DM8168DSPPROC_Object * procObject = (DM8168DSPPROC_Object *) handle;
    Int                    i          = 0;
    ProcMgr_AddrInfo *     ai         = NULL;

    GT_2trace (curTrace, GT_ENTER, "DM8168DSPPROC_Params_init", handle, params);

    GT_assert (curTrace, (params != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (params == NULL) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DSPPROC_Params_init",
                             PROCESSOR_E_INVALIDARG,
                             "Argument of type (DM8168DSPPROC_Params *) "
                             "passed is null!");
    }
    else {
#endif
        if (handle == NULL) {

            /* check for instance params override */
            Cfg_propBool(PARAMS_mmuEnable, ProcMgr_sysLinkCfgParams,
                &(DM8168DSPPROC_state.defInstParams.mmuEnable));

            memcpy(params, &(DM8168DSPPROC_state.defInstParams),
                sizeof(DM8168DSPPROC_Params));

            /* initialize the translation table */
            for (i = AddrTable_STATIC_COUNT; i < AddrTable_SIZE; i++) {
                ai = &AddrTable[i];
                ai->addr[ProcMgr_AddrType_MasterKnlVirt] = -1u;
                ai->addr[ProcMgr_AddrType_MasterUsrVirt] = -1u;
                ai->addr[ProcMgr_AddrType_MasterPhys] = -1u;
                ai->addr[ProcMgr_AddrType_SlaveVirt] = -1u;
                ai->addr[ProcMgr_AddrType_SlavePhys] = -1u;
                ai->size = 0u;
                ai->isCached = FALSE;
                ai->mapMask = 0u;
                ai->isMapped = FALSE;
            }

            /* initialize refCount for all entries - both static and dynamic */
            for(i = 0; i < AddrTable_SIZE; i++) {
                AddrTable[i].refCount = 0u;
            }
            memcpy((Ptr)params->memEntries, AddrTable, sizeof(AddrTable));
        }
        else {
            /* return updated DM8168DSPPROC instance specific parameters */
            memcpy(params, &(procObject->params), sizeof(DM8168DSPPROC_Params));
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif

    GT_0trace (curTrace, GT_LEAVE, "DM8168DSPPROC_Params_init");
}

/*!
 *  @brief      Function to create an instance of this Processor.
 *
 *  @param      name    Name of the Processor instance.
 *  @param      params  Configuration parameters.
 *
 *  @sa         DM8168DSPPROC_delete
 */
DM8168DSPPROC_Handle
DM8168DSPPROC_create (      UInt16                procId,
                     const DM8168DSPPROC_Params * params)
{
    Int                   status    = PROCESSOR_SUCCESS;
    Processor_Object *    handle    = NULL;
    DM8168DSPPROC_Object * object    = NULL;
    IArg                  key;

    GT_2trace (curTrace, GT_ENTER, "DM8168DSPPROC_create", procId, params);

    GT_assert (curTrace, IS_VALID_PROCID (procId));
    GT_assert (curTrace, (params != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (!IS_VALID_PROCID (procId)) {
        /* Not setting status here since this function does not return status.*/
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DSPPROC_create",
                             PROCESSOR_E_INVALIDARG,
                             "Invalid procId specified");
    }
    else if (params == NULL) {
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DSPPROC_create",
                             PROCESSOR_E_INVALIDARG,
                             "params passed is NULL!");
    }
    else {
#endif
        /* Enter critical section protection. */
        key = IGateProvider_enter (DM8168DSPPROC_state.gateHandle);
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
        /* Check if the Processor already exists for specified procId. */
        if (DM8168DSPPROC_state.procHandles [procId] != NULL) {
            status = PROCESSOR_E_ALREADYEXIST;
            GT_setFailureReason (curTrace,
                              GT_4CLASS,
                              "DM8168DSPPROC_create",
                              status,
                              "Processor already exists for specified procId!");
        }
        else {
#endif
            /* Allocate memory for the handle */
            handle = (Processor_Object *) Memory_calloc (NULL,
                                                      sizeof (Processor_Object),
                                                      0,
                                                      NULL);
            if (handle == NULL) {
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "DM8168DSPPROC_create",
                                     PROCESSOR_E_MEMORY,
                                     "Memory allocation failed for handle!");
            }
            else {
                /* Populate the handle fields */
                handle->procFxnTable.attach        = &DM8168DSPPROC_attach;
                handle->procFxnTable.detach        = &DM8168DSPPROC_detach;
                handle->procFxnTable.start         = &DM8168DSPPROC_start;
                handle->procFxnTable.stop          = &DM8168DSPPROC_stop;
                handle->procFxnTable.read          = &DM8168DSPPROC_read;
                handle->procFxnTable.write         = &DM8168DSPPROC_write;
                handle->procFxnTable.control       = &DM8168DSPPROC_control;
                handle->procFxnTable.map           = &DM8168DSPPROC_map;
                handle->procFxnTable.unmap         = &DM8168DSPPROC_unmap;
                handle->procFxnTable.translateAddr = &DM8168DSPPROC_translate;
                handle->state = ProcMgr_State_Unknown;

                /* Allocate memory for the DM8168DSPPROC handle */
                handle->object = Memory_calloc (NULL,
                                                sizeof (DM8168DSPPROC_Object),
                                                0,
                                                NULL);
                if (handle->object == NULL) {
                    status = PROCESSOR_E_MEMORY;
                    GT_setFailureReason (curTrace,
                                GT_4CLASS,
                                "DM8168DSPPROC_create",
                                status,
                                "Memory allocation failed for handle->object!");
                }
                else {
                    handle->procId = procId;
                    object = (DM8168DSPPROC_Object *) handle->object;
                    object->halObject = NULL;
                    /* Copy params into instance object. */
                    memcpy (&(object->params),
                                 (Ptr) params,
                                 sizeof (DM8168DSPPROC_Params));
                    /* Set the handle in the state object. */
                    DM8168DSPPROC_state.procHandles [procId] =
                                                   (DM8168DSPPROC_Handle) handle;
                }
            }
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
        }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */

        /* Leave critical section protection. */
        IGateProvider_leave (DM8168DSPPROC_state.gateHandle, key);
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */

    if (status < 0) {
        if (handle !=  NULL) {
            if (handle->object != NULL) {
                Memory_free (NULL,
                             handle->object,
                             sizeof (DM8168DSPPROC_Object));
            }
            Memory_free (NULL, handle, sizeof (Processor_Object));
        }
        /*! @retval NULL Function failed */
        handle = NULL;
    }
    GT_1trace (curTrace, GT_LEAVE, "DM8168DSPPROC_create", handle);

    /*! @retval Valid-Handle Operation successful */
    return (DM8168DSPPROC_Handle) handle;
}


/*!
 *  @brief      Function to delete an instance of this Processor.
 *
 *              The user provided pointer to the handle is reset after
 *              successful completion of this function.
 *
 *  @param      handlePtr  Pointer to Handle to the Processor instance
 *
 *  @sa         DM8168DSPPROC_create
 */
Int
DM8168DSPPROC_delete (DM8168DSPPROC_Handle * handlePtr)
{
    Int                   status = PROCESSOR_SUCCESS;
    DM8168DSPPROC_Object * object = NULL;
    Processor_Object *    handle;
    IArg                  key;

    GT_1trace (curTrace, GT_ENTER, "DM8168DSPPROC_delete", handlePtr);

    GT_assert (curTrace, (handlePtr != NULL));
    GT_assert (curTrace, ((handlePtr != NULL) && (*handlePtr != NULL)));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (handlePtr == NULL) {
        /*! @retval PROCESSOR_E_INVALIDARG Invalid NULL handlePtr pointer
                                         specified*/
        status = PROCESSOR_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DSPPROC_delete",
                             status,
                             "Invalid NULL handlePtr pointer specified");
    }
    else if (*handlePtr == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid NULL *handlePtr specified */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DSPPROC_delete",
                             status,
                             "Invalid NULL *handlePtr specified");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        handle = (Processor_Object *) (*handlePtr);
        /* Enter critical section protection. */
        key = IGateProvider_enter (DM8168DSPPROC_state.gateHandle);

        /* Reset handle in PwrMgr handle array. */
        GT_assert (curTrace, IS_VALID_PROCID (handle->procId));
        DM8168DSPPROC_state.procHandles [handle->procId] = NULL;

        /* Free memory used for the DM8168DSPPROC object. */
        if (handle->object != NULL) {
            object = (DM8168DSPPROC_Object *) handle->object;
            Memory_free (NULL,
                         object,
                         sizeof (DM8168DSPPROC_Object));
            handle->object = NULL;
        }

        /* Free memory used for the Processor object. */
        Memory_free (NULL, handle, sizeof (Processor_Object));
        *handlePtr = NULL;

        /* Leave critical section protection. */
        IGateProvider_leave (DM8168DSPPROC_state.gateHandle, key);
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */

    GT_1trace (curTrace, GT_LEAVE, "DM8168DSPPROC_delete", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return (status);
}


/*!
 *  @brief      Function to open a handle to an instance of this Processor. This
 *              function is called when access to the Processor is required from
 *              a different process.
 *
 *  @param      handlePtr   Handle to the Processor instance
 *  @param      procId      Processor ID addressed by this Processor instance.
 *
 *  @sa         DM8168DSPPROC_close
 */
Int
DM8168DSPPROC_open (DM8168DSPPROC_Handle * handlePtr, UInt16 procId)
{
    Int status = PROCESSOR_SUCCESS;

    GT_2trace (curTrace, GT_ENTER, "DM8168DSPPROC_open", handlePtr, procId);

    GT_assert (curTrace, (handlePtr != NULL));
    GT_assert (curTrace, IS_VALID_PROCID (procId));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (handlePtr == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid NULL handlePtr specified */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DSPPROC_open",
                             status,
                             "Invalid NULL handlePtr specified");
    }
    else if (!IS_VALID_PROCID (procId)) {
        /*! @retval PROCESSOR_E_INVALIDARG Invalid procId specified */
        status = PROCESSOR_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DSPPROC_open",
                             status,
                             "Invalid procId specified");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        /* Initialize return parameter handle. */
        *handlePtr = NULL;

        /* Check if the PwrMgr exists and return the handle if found. */
        if (DM8168DSPPROC_state.procHandles [procId] == NULL) {
            /*! @retval PROCESSOR_E_NOTFOUND Specified instance not found */
            status = PROCESSOR_E_NOTFOUND;
            GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DSPPROC_open",
                             status,
                             "Specified DM8168DSPPROC instance does not exist!");
        }
        else {
            *handlePtr = DM8168DSPPROC_state.procHandles [procId];
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */

    GT_1trace (curTrace, GT_LEAVE, "DM8168DSPPROC_open", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to close a handle to an instance of this Processor.
 *
 *  @param      handlePtr  Pointer to Handle to the Processor instance
 *
 *  @sa         DM8168DSPPROC_open
 */
Int
DM8168DSPPROC_close (DM8168DSPPROC_Handle * handlePtr)
{
    Int status = PROCESSOR_SUCCESS;

    GT_1trace (curTrace, GT_ENTER, "DM8168DSPPROC_close", handlePtr);

    GT_assert (curTrace, (handlePtr != NULL));
    GT_assert (curTrace, ((handlePtr != NULL) && (*handlePtr != NULL)));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (handlePtr == NULL) {
        /*! @retval PROCESSOR_E_INVALIDARG Invalid NULL handlePtr pointer
                                         specified*/
        status = PROCESSOR_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DSPPROC_close",
                             status,
                             "Invalid NULL handlePtr pointer specified");
    }
    else if (*handlePtr == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid NULL *handlePtr specified */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DSPPROC_close",
                             status,
                             "Invalid NULL *handlePtr specified");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        /* Nothing to be done for close. */
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */

    GT_1trace (curTrace, GT_LEAVE, "DM8168DSPPROC_close", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/* =============================================================================
 * APIs called by Processor module (part of function table interface)
 * =============================================================================
 */
/*!
 *  @brief      Function to initialize the slave processor
 *
 *  @param      handle  Handle to the Processor instance
 *  @param      params  Attach parameters
 *
 *  @sa         DM8168DSPPROC_detach
 */
Int
DM8168DSPPROC_attach(
        Processor_Handle            handle,
        Processor_AttachParams *    params)
{

    Int                         status = PROCESSOR_SUCCESS;
    Processor_Object *          procHandle = (Processor_Object *)handle;
    DM8168DSPPROC_Object *      object = NULL;
    UInt32                      i = 0;
    UInt32                      j = 0;
    UInt32                      index = 0;
    ProcMgr_AddrInfo *          me;
    SysLink_MemEntry *          entry;
    SysLink_MemEntry_Block *    memBlock = NULL;
//  DM8168DSP_HalMmuCtrlArgs_Enable mmuEnableArgs;

    GT_2trace(curTrace, GT_ENTER, "DM8168DSPPROC_attach", handle, params);
    GT_assert (curTrace, (handle != NULL));
    GT_assert (curTrace, (params != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (handle == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid argument */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DSPPROC_attach",
                             status,
                             "Invalid handle specified");
    }
    else if (params == NULL) {
            /*! @retval PROCESSOR_E_INVALIDARG Invalid argument */
            status = PROCESSOR_E_INVALIDARG;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "DM8168DSPPROC_attach",
                                 status,
                                 "Invalid params specified");
    }
    else {
#endif
        object = (DM8168DSPPROC_Object *) procHandle->object;
        GT_assert (curTrace, (object != NULL));

        params->procArch = Processor_ProcArch_C64x;

        /* check for instance params override */
        Cfg_propBool(PARAMS_mmuEnable, ProcMgr_sysLinkCfgParams,
            &(object->params.mmuEnable));

        object->pmHandle = params->pmHandle;
        GT_0trace(curTrace, GT_1CLASS,
            "DM8168DSPPROC_attach: Mapping memory regions");

        /* search for dsp memory map */
        for (j = 0; j < params->sysMemMap->numBlocks; j++) {
            if (!strcmp(params->sysMemMap->memBlocks[j].procName, "DSP")) {
                memBlock = &params->sysMemMap->memBlocks[j];
                break;
            }
        }

        /* update translation tables with memory map */
        for (i = 0; (memBlock != NULL) && (i < memBlock->numEntries)
            && (memBlock->memEntries[i].isValid) && (status >= 0); i++) {

            entry = &memBlock->memEntries[i];

            if (entry->map == FALSE) {
                /* update table with entries which don't require mapping */
                if (AddrTable_count != AddrTable_SIZE) {
                    me = &AddrTable[AddrTable_count];

                    me->addr[ProcMgr_AddrType_MasterKnlVirt] = -1u;
                    me->addr[ProcMgr_AddrType_MasterUsrVirt] = -1u;
                    me->addr[ProcMgr_AddrType_MasterPhys] =
                            entry->masterPhysAddr;
                    me->addr[ProcMgr_AddrType_SlaveVirt] = entry->slaveVirtAddr;
                    me->addr[ProcMgr_AddrType_SlavePhys] = -1u;
                    me->size = entry->size;
                    me->isCached = entry->isCached;
                    me->mapMask = entry->mapMask;

                    AddrTable_count++;
                }
                else {
                    status = PROCESSOR_E_FAIL;
                    GT_setFailureReason(curTrace, GT_4CLASS,
                        "DM8168DSPPROC_attach", status,
                        "AddrTable_SIZE reached!");
                }
            }
            else if (entry->map == TRUE) {
                /* send these entries back to ProcMgr for mapping */
                index = object->params.numMemEntries;

                if (index != ProcMgr_MAX_MEMORY_REGIONS) {
                    me = &object->params.memEntries[index];

                    me->addr[ProcMgr_AddrType_MasterKnlVirt] = -1u;
                    me->addr[ProcMgr_AddrType_MasterUsrVirt] = -1u;
                    me->addr[ProcMgr_AddrType_MasterPhys] =
                            entry->masterPhysAddr;
                    me->addr[ProcMgr_AddrType_SlaveVirt] = entry->slaveVirtAddr;
                    me->addr[ProcMgr_AddrType_SlavePhys] = -1u;
                    me->size = entry->size;
                    me->isCached = entry->isCached;
                    me->mapMask = entry->mapMask;

                    object->params.numMemEntries++;
                }
                else {
                    status = PROCESSOR_E_FAIL;
                    GT_setFailureReason(curTrace, GT_4CLASS,
                        "DM8168DSPPROC_attach", status,
                        "ProcMgr_MAX_MEMORY_REGIONS reached!");
                }
            }
            else {
                status = PROCESSOR_E_INVALIDARG;
                GT_setFailureReason(curTrace, GT_4CLASS,
                    "DM8168DSPPROC_attach", status,
                    "Memory map has entry with invalid 'map' value");
            }
        } /* for (...) */

        if (status >= 0) {
            /* populate the return params */
            params->numMemEntries = object->params.numMemEntries;
            memcpy((Ptr)params->memEntries, (Ptr)object->params.memEntries,
                sizeof(ProcMgr_AddrInfo) * params->numMemEntries);

            status = DM8168DSP_halInit(&(object->halObject), NULL);

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
            if (status < 0) {
                GT_setFailureReason(curTrace, GT_4CLASS,
                    "DM8168DSPPROC_attach", status,
                    "DM8168DSP_halInit failed");
            }
            else {
#endif
                if ((procHandle->bootMode == ProcMgr_BootMode_Boot)
                    || (procHandle->bootMode == ProcMgr_BootMode_NoLoad_Pwr)) {

                    /* assert reset, then release global reset to enable L2 */
                    status = Dm8168DspHal_reset(object->halObject,
                        Dm8168DspHal_Reset_Attach);

#if !defined(SYSLINK_BUILD_OPTIMIZE)
                    if (status < 0) {
                        GT_setFailureReason(curTrace, GT_4CLASS,
                            "DM8168DSPPROC_attach", status,
                            "Failed to reset the slave processor");
                    }
                    else {
#endif
                        GT_0trace(curTrace, GT_1CLASS,
                            "DM8168DSPPROC_attach: slave is now in reset");

                        if (object->params.mmuEnable) {
#if 0   /* disabled because of silicon bug */
                           mmuEnableArgs.numMemEntries = 0;
                           status = DM8168DSP_halMmuCtrl(object->halObject,
                               Processor_MmuCtrlCmd_Enable, &mmuEnableArgs);

#if !defined(SYSLINK_BUILD_OPTIMIZE)
                            if (status < 0) {
                                GT_setFailureReason(curTrace, GT_4CLASS,
                                    "DM8168DSPPROC_attach", status,
                                    "Failed to enable the slave MMU");
                            }
#endif
                            GT_0trace(curTrace, GT_2CLASS,
                                "DM8168DSPPROC_attach: Slave MMU "
                                "is configured!");
#endif
                        }
#if !defined(SYSLINK_BUILD_OPTIMIZE)
                    }
#endif
                }
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
            }
#endif
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif

    GT_1trace(curTrace, GT_LEAVE, "DM8168DSPPROC_attach", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to detach from the Processor.
 *
 *  @param      handle  Handle to the Processor instance
 *
 *  @sa         DM8168DSPPROC_attach
 */
Int
DM8168DSPPROC_detach (Processor_Handle handle)
{
    Int                   status       = PROCESSOR_SUCCESS;
    Int                   tmpStatus    = PROCESSOR_SUCCESS;
    Processor_Object *    procHandle   = (Processor_Object *) handle;
    DM8168DSPPROC_Object * object      = NULL;
    Int i                              = 0;
    ProcMgr_AddrInfo *    ai;

    GT_1trace (curTrace, GT_ENTER, "DM8168DSPPROC_detach", handle);

    GT_assert (curTrace, (handle != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (handle == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid argument */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DSPPROC_detach",
                             PROCESSOR_E_HANDLE,
                             "Invalid handle specified");
    }
    else {
#endif
        object = (DM8168DSPPROC_Object *) procHandle->object;
        GT_assert (curTrace, (object != NULL));

        if (    (procHandle->bootMode == ProcMgr_BootMode_Boot)
            ||  (procHandle->bootMode == ProcMgr_BootMode_NoLoad_Pwr)) {

            if (object->params.mmuEnable) {
                GT_0trace(curTrace, GT_2CLASS,
                    "DM8168DSPPROC_detach: Disabling Slave MMU ...");

#if 0   /* disabled because of silicon bug */
               status = DM8168DSP_halMmuCtrl(object->halObject,
                   Processor_MmuCtrlCmd_Disable, NULL);

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
                if (status < 0) {
                    GT_setFailureReason(curTrace, GT_4CLASS,
                        "DM8168VPSSM3PROC_detach", status,
                        "Failed to disable the slave MMU");
                }
#endif
#endif
            }

            /* delete all dynamically added entries */
            for (i = AddrTable_STATIC_COUNT; i < AddrTable_count; i++) {
                ai = &AddrTable[i];
                ai->addr[ProcMgr_AddrType_MasterKnlVirt] = -1u;
                ai->addr[ProcMgr_AddrType_MasterUsrVirt] = -1u;
                ai->addr[ProcMgr_AddrType_MasterPhys] = -1u;
                ai->addr[ProcMgr_AddrType_SlaveVirt] = -1u;
                ai->addr[ProcMgr_AddrType_SlavePhys] = -1u;
                ai->size = 0u;
                ai->isCached = FALSE;
                ai->mapMask = 0u;
                ai->isMapped = FALSE;
                ai->refCount = 0u;
            }
            object->params.numMemEntries = AddrTable_STATIC_COUNT;
            AddrTable_count = AddrTable_STATIC_COUNT;

            /* assert reset on the slave processor */
            tmpStatus = Dm8168DspHal_reset(object->halObject,
                Dm8168DspHal_Reset_Detach);

            GT_0trace(curTrace, GT_2CLASS,
                "DM8168DSPPROC_detach: Slave processor is now in reset");

#if !defined(SYSLINK_BUILD_OPTIMIZE)
            if ((tmpStatus < 0) && (status >= 0)) {
                status = tmpStatus;
                GT_setFailureReason(curTrace,
                                     GT_4CLASS,
                                     "DM8168DSPPROC_detach",
                                     status,
                                     "Failed to reset the slave processor");
            }
#endif
        }

        GT_0trace (curTrace,
                   GT_2CLASS,
                   "    DM8168DSPPROC_detach: Unmapping memory regions\n");

        tmpStatus = DM8168DSP_halExit (object->halObject);
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
        if ((tmpStatus < 0) && (status >= 0)) {
            status = tmpStatus;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "DM8168DSPPROC_detach",
                                 status,
                                 "Failed to finalize HAL object");
        }
    }
#endif

    GT_1trace(curTrace, GT_LEAVE, "DM8168DSPPROC_detach: status=0x%x", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to start the slave processor
 *
 *              Start the slave processor running from its entry point.
 *              Depending on the boot mode, this involves configuring the boot
 *              address and releasing the slave from reset.
 *
 *  @param      handle    Handle to the Processor instance
 *
 *  @sa         DM8168DSPPROC_stop, DM8168DSP_halBootCtrl, Dm8168DspHal_reset
 */
Int
DM8168DSPPROC_start (Processor_Handle        handle,
                    UInt32                  entryPt,
                    Processor_StartParams * params)
{
    Int                   status        = PROCESSOR_SUCCESS ;
    Processor_Object *    procHandle    = (Processor_Object *) handle;
    DM8168DSPPROC_Object * object        = NULL;

    GT_3trace (curTrace, GT_ENTER, "DM8168DSPPROC_start",
               handle, entryPt, params);

    GT_assert (curTrace, (handle != NULL));
    GT_assert (curTrace, (params != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (handle == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid argument */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DSPPROC_start",
                             status,
                             "Invalid handle specified");
    }
    else if (params == NULL) {
            /*! @retval PROCESSOR_E_INVALIDARG Invalid argument */
            status = PROCESSOR_E_INVALIDARG;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "DM8168DSPPROC_start",
                                 status,
                                 "Invalid params specified");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        object = (DM8168DSPPROC_Object *) procHandle->object;
        GT_assert (curTrace, (object != NULL));
        if (    (procHandle->bootMode == ProcMgr_BootMode_Boot)
            ||  (procHandle->bootMode == ProcMgr_BootMode_NoLoad_Pwr)
            ||  (procHandle->bootMode == ProcMgr_BootMode_NoLoad_NoPwr)) {
            /* Slave is to be started only for Boot mode and NoLoad mode. */
            /* Specify the DSP boot address in the boot config register */
            status = DM8168DSP_halBootCtrl (object->halObject,
                                        Processor_BootCtrlCmd_SetEntryPoint,
                                        (Ptr) entryPt);
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
            if (status < 0) {
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "DM8168DSPPROC_start",
                                     status,
                                     "Failed to set slave boot entry point");
            }
            else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
                /* release the slave cpu from reset */
                status = Dm8168DspHal_reset(object->halObject,
                    Dm8168DspHal_Reset_Start);
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
                if (status < 0) {
                    GT_setFailureReason (curTrace,
                                         GT_4CLASS,
                                         "DM8168DSPPROC_start",
                                         status,
                                         "Failed to release slave from reset");
                }
            }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        }

        /* For NoBoot mode, send an interrupt to the slave.
         * TBD: How should Dm8168DspProc interface with Notify for this?
         */
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */

    if (status >= 0) {
        GT_0trace (curTrace,
                   GT_1CLASS,
                   "    DM8168DSPPROC_start: Slave successfully started!\n");
    }
    else {
        GT_0trace (curTrace,
                   GT_1CLASS,
                   "    DM8168DSPPROC_start: Slave could not be started!\n");
    }

    GT_1trace (curTrace, GT_LEAVE, "DM8168DSPPROC_start", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to stop the slave processor
 *
 *              Stop the execution of the slave processor. Depending on the boot
 *              mode, this may result in placing the slave processor in reset.
 *
 *  @param      handle    Handle to the Processor instance
 *
 *  @sa         DM8168DSPPROC_start, Dm8168DspHal_reset
 */
Int
DM8168DSPPROC_stop (Processor_Handle handle)
{
    Int                   status       = PROCESSOR_SUCCESS ;
    Processor_Object *    procHandle   = (Processor_Object *) handle;
    DM8168DSPPROC_Object * object       = NULL;

    GT_1trace (curTrace, GT_ENTER, "DM8168DSPPROC_stop", handle);

    GT_assert (curTrace, (handle != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (handle == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid argument */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DSPPROC_stop",
                             status,
                             "Invalid handle specified");
    }
    else {
#endif
        object = (DM8168DSPPROC_Object *) procHandle->object;
        GT_assert (curTrace, (object != NULL));

        /* Slave is to be stopped only for Boot mode and NoLoad mode. */
        if (    (procHandle->bootMode == ProcMgr_BootMode_Boot)
            ||  (procHandle->bootMode == ProcMgr_BootMode_NoLoad_Pwr)
            ||  (procHandle->bootMode == ProcMgr_BootMode_NoLoad_NoPwr)) {
            status = Dm8168DspHal_reset(object->halObject,
                Dm8168DspHal_Reset_Stop);

            GT_0trace (curTrace,
                       GT_1CLASS,
                       "    DM8168DSPPROC_stop: Slave is now in reset!\n");
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
            if (status < 0) {
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "DM8168DSPPROC_stop",
                                     status,
                                     "Failed to place slave in reset");
            }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
    GT_1trace (curTrace, GT_LEAVE, "DM8168DSPPROC_stop", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to read from the slave processor's memory.
 *
 *              Read from the slave processor's memory and copy into the
 *              provided buffer.
 *
 *  @param      handle     Handle to the Processor instance
 *  @param      procAddr   Address in host processor's address space of the
 *                         memory region to read from.
 *  @param      numBytes   IN/OUT parameter. As an IN-parameter, it takes in the
 *                         number of bytes to be read. When the function
 *                         returns, this parameter contains the number of bytes
 *                         actually read.
 *  @param      buffer     User-provided buffer in which the slave processor's
 *                         memory contents are to be copied.
 *
 *  @sa         DM8168DSPPROC_write
 */
Int
DM8168DSPPROC_read (Processor_Handle   handle,
                   UInt32             procAddr,
                   UInt32 *           numBytes,
                   Ptr                buffer)
{
    Int       status   = PROCESSOR_SUCCESS ;
    UInt8  *  procPtr8 = NULL;

    GT_4trace (curTrace, GT_ENTER, "DM8168DSPPROC_read",
               handle, procAddr, numBytes, buffer);

    GT_assert (curTrace, (handle   != NULL));
    GT_assert (curTrace, (numBytes != NULL));
    GT_assert (curTrace, (buffer   != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (handle == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid argument */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DSPPROC_read",
                             status,
                             "Invalid handle specified");
    }
    else if (numBytes == 0) {
            /*! @retval PROCESSOR_E_INVALIDARG Invalid argument */
            status = PROCESSOR_E_INVALIDARG;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "DM8168DSPPROC_read",
                                 status,
                                 "Invalid numBytes specified");
    }
    else if (buffer == NULL) {
            /*! @retval PROCESSOR_E_INVALIDARG Invalid argument */
            status = PROCESSOR_E_INVALIDARG;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "DM8168DSPPROC_read",
                                 status,
                                 "Invalid buffer specified");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        procPtr8 = (UInt8 *) procAddr ;
        buffer = memcpy (buffer, procPtr8, *numBytes);
        GT_assert (curTrace, (buffer != (UInt32) NULL));
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
        if (buffer == (UInt32) NULL) {
            /*! @retval PROCESSOR_E_FAIL Failed in memcpy */
            status = PROCESSOR_E_FAIL;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "DM8168DSPPROC_read",
                                 status,
                                 "Failed in memcpy");
            *numBytes = 0;
        }
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */

    GT_1trace (curTrace, GT_LEAVE, "DM8168DSPPROC_read",status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to write into the slave processor's memory.
 *
 *              Read from the provided buffer and copy into the slave
 *              processor's memory.
 *
 *  @param      handle     Handle to the Processor object
 *  @param      procAddr   Address in host processor's address space of the
 *                         memory region to write into.
 *  @param      numBytes   IN/OUT parameter. As an IN-parameter, it takes in the
 *                         number of bytes to be written. When the function
 *                         returns, this parameter contains the number of bytes
 *                         actually written.
 *  @param      buffer     User-provided buffer from which the data is to be
 *                         written into the slave processor's memory.
 *
 *  @sa         DM8168DSPPROC_read, DM8168DSPPROC_translateAddr
 */
Int
DM8168DSPPROC_write (Processor_Handle handle,
                    UInt32           procAddr,
                    UInt32 *         numBytes,
                    Ptr              buffer)
{
    Int                   status       = PROCESSOR_SUCCESS ;
    UInt8  *              procPtr8     = NULL;
    UInt8                 temp8_1;
    UInt8                 temp8_2;
    UInt8                 temp8_3;
    UInt8                 temp8_4;
    UInt32                temp;

    GT_4trace (curTrace, GT_ENTER, "DM8168DSPPROC_write",
               handle, procAddr, numBytes, buffer);

    GT_assert (curTrace, (handle   != NULL));
    GT_assert (curTrace, (numBytes != NULL));
    GT_assert (curTrace, (buffer   != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (handle == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid argument */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DSPPROC_write",
                             status,
                             "Invalid handle specified");
    }
    else if (numBytes == 0) {
            /*! @retval PROCESSOR_E_INVALIDARG Invalid argument */
            status = PROCESSOR_E_INVALIDARG;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "DM8168DSPPROC_write",
                                 status,
                                 "Invalid numBytes specified");
    }
    else if (buffer == NULL) {
            /*! @retval PROCESSOR_E_INVALIDARG Invalid argument */
            status = PROCESSOR_E_INVALIDARG;
            GT_setFailureReason (curTrace,
                                 GT_4CLASS,
                                 "DM8168DSPPROC_write",
                                 status,
                                 "Invalid buffer specified");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        if (*numBytes != sizeof (UInt32)) {
            procPtr8 = (UInt8 *) procAddr ;
            procAddr = (UInt32) memcpy (procPtr8,
                                             buffer,
                                             *numBytes);
            GT_assert (curTrace, (procAddr != (UInt32) NULL));
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
            if (procAddr == (UInt32) NULL) {
                /*! @retval PROCESSOR_E_FAIL Failed in memcpy */
                status = PROCESSOR_E_FAIL;
                GT_setFailureReason (curTrace,
                                     GT_4CLASS,
                                     "DM8168DSPPROC_write",
                                     status,
                                     "Failed in memcpy");
                *numBytes = 0;
            }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        }
        else  {
             /* For 4 bytes, directly write as a UInt32 */
            temp8_1 = ((UInt8 *) buffer) [0];
            temp8_2 = ((UInt8 *) buffer) [1];
            temp8_3 = ((UInt8 *) buffer) [2];
            temp8_4 = ((UInt8 *) buffer) [3];
            temp = (UInt32) (    ((UInt32) temp8_4 << 24)
                             |   ((UInt32) temp8_3 << 16)
                             |   ((UInt32) temp8_2 << 8)
                             |   ((UInt32) temp8_1));
            *((UInt32*) procAddr) = temp;
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */

    GT_1trace (curTrace, GT_LEAVE, "DM8168DSPPROC_write",status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to perform device-dependent operations.
 *
 *              Performs device-dependent control operations as exposed by this
 *              implementation of the Processor module.
 *
 *  @param      handle     Handle to the Processor object
 *  @param      cmd        Device specific processor command
 *  @param      arg        Arguments specific to the type of command.
 *
 *  @sa
 */
Int
DM8168DSPPROC_control (Processor_Handle handle, Int32 cmd, Ptr arg)
{
    Int                   status       = PROCESSOR_SUCCESS ;

    GT_3trace (curTrace, GT_ENTER, "DM8168DSPPROC_control", handle, cmd, arg);

    GT_assert (curTrace, (handle   != NULL));
    /* cmd and arg can be 0/NULL, so cannot check for validity. */

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (handle == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid argument */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DSPPROC_control",
                             status,
                             "Invalid handle specified");
    }
    else {
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
        /* No control operations currently implemented. */
        /*! @retval PROCESSOR_E_NOTSUPPORTED No control operations are supported
                                             for this device. */
        status = PROCESSOR_E_NOTSUPPORTED;
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif /* #if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS) */
    GT_1trace (curTrace, GT_LEAVE, "DM8168DSPPROC_control",status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Translate slave virtual address to master physical address.
 *
 *  @param      handle     Handle to the Processor object
 *  @param      dstAddr    Returned: master physical address.
 *  @param      srcAddr    Slave virtual address.
 *
 *  @sa
 */
Int
DM8168DSPPROC_translate(
        Processor_Handle    handle,
        UInt32 *            dstAddr,
        UInt32              srcAddr)
{
    Int                         status = PROCESSOR_SUCCESS;
    Processor_Object *          procHandle= (Processor_Object *)handle;
    DM8168DSPPROC_Object *      object = NULL;
    UInt32                      startAddr;
    UInt32                      endAddr;
    UInt32                      offset;
    UInt32                      i;
    ProcMgr_AddrInfo *          ai;

    GT_3trace(curTrace, GT_ENTER,
        "DM8168DSPPROC_translate: handle=0x%x, dstAddr=0x%x, srcAddr=0x%x",
        handle, dstAddr, srcAddr);

    GT_assert (curTrace, (handle != NULL));
    GT_assert (curTrace, (dstAddr != NULL));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (handle == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid argument */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DSPPROC_translate",
                             status,
                             "Invalid handle specified");
    }
    else if (dstAddr == NULL) {
        /*! @retval PROCESSOR_E_INVALIDARG sglist provided as NULL */
        status = PROCESSOR_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DSPPROC_translate",
                             status,
                             "dstAddr provided as NULL");
    }
    else {
#endif
        object = (DM8168DSPPROC_Object *)procHandle->object;
        GT_assert(curTrace, (object != NULL));
        *dstAddr = -1u;

        for (i = 0; i < AddrTable_SIZE; i++) {
            ai = &AddrTable [i];
            if (ai->addr[ProcMgr_AddrType_SlaveVirt] != -1u) {
                startAddr = ai->addr[ProcMgr_AddrType_SlaveVirt];
                endAddr = startAddr + ai->size;

                if ((startAddr <= srcAddr) && (srcAddr < endAddr)) {
                    offset = srcAddr - startAddr;
                    *dstAddr = ai->addr[ProcMgr_AddrType_MasterPhys] + offset;
                    GT_3trace(curTrace, GT_1CLASS, "DM8168DSPPROC_translate: "
                        "translated [%d] srcAddr=0x%x to dstAddr=0x%x",
                        i, srcAddr, *dstAddr);
                    break;
                }
            }
        }

        if (*dstAddr == -1u) {
            if (!object->params.mmuEnable) {
                /* default to direct mapping (i.e. v=p) */
                *dstAddr = srcAddr;
                GT_2trace(curTrace, GT_1CLASS, "DM8168DSPPROC_translate: "
                    "(default) srcAddr=0x%x to dstAddr=0x%x",
                    srcAddr, *dstAddr);
            }
            else {
                /* srcAddr not found in slave address space */
                status = PROCESSOR_E_INVALIDARG;
                GT_setFailureReason(curTrace, GT_4CLASS,
                    "DM8168DSPPROC_translate", status,
                    "srcAddr not found in slave address space");
            }
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif
    GT_1trace(curTrace, GT_LEAVE,
        "DM8168DSPPROC_translate: status=0x%x", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Map the given address translation into the slave mmu
 *
 *  @param      handle      Handle to the Processor object
 *  @param      dstAddr     Base virtual address
 *  @param      nSegs       Number of given segments
 *  @param      sglist      Segment list
 */
Int
DM8168DSPPROC_map(
        Processor_Handle    handle,
        UInt32 *            dstAddr,
        UInt32              nSegs,
        Memory_SGList *     sglist)
{
    Int                         status = PROCESSOR_SUCCESS;
    Processor_Object *          procHandle = (Processor_Object *)handle;
    DM8168DSPPROC_Object *      object = NULL;
    Bool                        found = FALSE;
    UInt32                      startAddr;
    UInt32                      endAddr;
    UInt32                      i;
    UInt32                      j;
    ProcMgr_AddrInfo *          ai;
//  DM8168DSP_HalMmuCtrlArgs_AddEntry addEntryArgs;

    GT_4trace(curTrace, GT_ENTER, "DM8168DSPPROC_map: "
       "handle=0x%x, dstAddr=0x%x, nSegs=%d, sglist=0x%x",
       handle, dstAddr, nSegs, sglist);

    GT_assert (curTrace, (handle != NULL));
    GT_assert (curTrace, (sglist != NULL));
    GT_assert (curTrace, (nSegs > 0));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (handle == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid argument */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DSPPROC_map",
                             status,
                             "Invalid handle specified");
    }
    else if (sglist == NULL) {
        /*! @retval PROCESSOR_E_INVALIDARG sglist provided as NULL */
        status = PROCESSOR_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DSPPROC_map",
                             status,
                             "sglist provided as NULL");
    }
    else if (nSegs == 0) {
        /*! @retval PROCESSOR_E_INVALIDARG Number of segments provided is 0 */
        status = PROCESSOR_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DSPPROC_map",
                             status,
                             "Number of segments provided is 0");
    }
    else {
#endif
        object = (DM8168DSPPROC_Object *)procHandle->object;
        GT_assert (curTrace, (object != NULL));

        for (i = 0; (i < nSegs) && (status >= 0); i++) {
            /* Update the translation table with entries for which mapping
             * is required. Add the entry only if the range does not exist
             * in the translation table.
             */

            /* check in static entries first */
            for (j = 0; j < AddrTable_STATIC_COUNT; j++) {
                ai = &AddrTable [j];
                startAddr = ai->addr[ProcMgr_AddrType_SlaveVirt];
                endAddr = startAddr + ai->size;

                if ((startAddr <= *dstAddr) && (*dstAddr < endAddr)) {
                    found = TRUE;
                    ai->refCount++;
                    GT_4trace(curTrace, GT_1CLASS, "DM8168DSPPROC_map: "
                        "found static entry: [%d] sva=0x%x, mpa=0x%x size=0x%x",
                        j, ai->addr[ProcMgr_AddrType_SlaveVirt],
                        ai->addr[ProcMgr_AddrType_MasterPhys], ai->size);
                    break;
                 }
            }

            /* if not found in static entries, check in dynamic entries */
            if (!found) {
                for (j = AddrTable_STATIC_COUNT; j < AddrTable_SIZE; j++) {
                    ai = &AddrTable[j];
                    if (ai->addr[ProcMgr_AddrType_SlaveVirt] != -1u) {

                        if (ai->isMapped == TRUE) {
                            startAddr = ai->addr[ProcMgr_AddrType_SlaveVirt];
                            endAddr = startAddr + ai->size;

                            if ((startAddr <= *dstAddr) && (*dstAddr < endAddr)
                                && ((*dstAddr + sglist[i].size) <= endAddr)) {
                                found = TRUE;
                                ai->refCount++;
                                GT_4trace(curTrace, GT_1CLASS,
                                    "DM8168DSPPROC_map: found dynamic entry: "
                                    "[%d] sva=0x%x, mpa=0x%x, size=0x%x",
                                    j, ai->addr[ProcMgr_AddrType_SlaveVirt],
                                    ai->addr[ProcMgr_AddrType_MasterPhys],ai->size);
                                break;
                            }
                        }
                    }
                }
            }

            /* if not found and mmu is enabled, add new entry to table */
            if (!found) {
                if (object->params.mmuEnable) {

                    for (j = AddrTable_STATIC_COUNT; j < AddrTable_SIZE; j++) {
                        ai = &AddrTable[j];
                        if (ai->addr[ProcMgr_AddrType_MasterPhys] == -1u) {

                            ai->addr[ProcMgr_AddrType_MasterKnlVirt] = -1u;
                            ai->addr[ProcMgr_AddrType_MasterUsrVirt] = -1u;
                            ai->addr[ProcMgr_AddrType_MasterPhys] = sglist[i].paddr;
                            ai->addr[ProcMgr_AddrType_SlaveVirt] = *dstAddr;
                            ai->addr[ProcMgr_AddrType_SlavePhys] = -1u;
                            ai->size = sglist[i].size;
                            ai->isCached = sglist[i].isCached;
                            ai->refCount++;

                            GT_4trace(curTrace, GT_1CLASS,
                                "DM8168DSPPROC_map: adding dynamic entry: "
                                "[%d] sva=0x%x, mpa=0x%x, size=0x%x",
                                (j), *dstAddr, sglist[i].paddr,
                                sglist[i].size);
                           break;
                         }
                    }
                    if (j == AddrTable_SIZE) {
                        status = PROCESSOR_E_FAIL;
                        GT_setFailureReason(curTrace, GT_4CLASS,
                            "DM8168DSPPROC_map", status,
                            "AddrTable_SIZE reached!");
                    }
                }
                else {
                    /* if mmu disabled, AddrTable not updated */
                    ai = NULL;
                }
            }

            /* if new entry, map into dsp mmu */
            if ((ai != NULL) && (ai->refCount == 1) && (status >= 0)) {
                ai->isMapped = TRUE;

                if (object->params.mmuEnable) {
                    /* Add entry to Dsp mmu */
#if 0
                    addEntryArgs.masterPhyAddr = sglist [i].paddr;
                    addEntryArgs.size          = sglist [i].size;
                    addEntryArgs.slaveVirtAddr = (UInt32)*dstAddr;
                    /* TBD: elementSize, endianism, mixedSized are
                     * hard coded now, must be configurable later
                     */
                    addEntryArgs.elementSize   = ELEM_SIZE_16BIT;
                    addEntryArgs.endianism     = LITTLE_ENDIAN;
                    addEntryArgs.mixedSize     = MMU_TLBES;
                    status = DM8168DSP_halMmuCtrl(object->halObject,
                        Processor_MmuCtrlCmd_AddEntry, &addEntryArgs);
#endif
#if !defined(SYSLINK_BUILD_OPTIMIZE)
                    if (status < 0) {
                        GT_setFailureReason(curTrace, GT_4CLASS,
                            "DM8168DSPPROC_map", status,
                            "Processor_MmuCtrlCmd_AddEntry failed");
                    }
#endif
                }
            }
#if !defined(SYSLINK_BUILD_OPTIMIZE)
            if (status < 0) {
                GT_setFailureReason(curTrace, GT_4CLASS,
                    "DM8168DSPPROC_map", status,
                    "DSP MMU configuration failed");
            }
#endif
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif
    GT_1trace(curTrace, GT_LEAVE, "DM8168DSPPROC_map: status=0x%x", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}


/*!
 *  @brief      Function to unmap slave address from host address space
 *
 *  @param      handle      Handle to the Processor object
 *  @param      dstAddr     Return parameter: Pointer to receive the mapped
 *                          address.
 *  @param      size        Size of the region to be mapped.
 *
 *  @sa
 */
Int
DM8168DSPPROC_unmap(
        Processor_Handle    handle,
        UInt32              addr,
        UInt32              size)
{
    Int                         status = PROCESSOR_SUCCESS;
    Processor_Object *          procHandle = (Processor_Object *)handle;
    DM8168DSPPROC_Object *      object = NULL;
    ProcMgr_AddrInfo *          ai;
    Int                         i;
    UInt32                      startAddr;
    UInt32                      endAddr;
//  DM8168DSP_HalMmuCtrlArgs_DeleteEntry deleteEntryArgs;

    GT_3trace (curTrace, GT_ENTER, "DM8168DSPPROC_unmap",
               handle, addr, size);

    GT_assert (curTrace, (handle != NULL));
    GT_assert (curTrace, (size   != 0));

#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    if (handle == NULL) {
        /*! @retval PROCESSOR_E_HANDLE Invalid argument */
        status = PROCESSOR_E_HANDLE;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DSPPROC_unmap",
                             status,
                             "Invalid handle specified");
    }
    else if (size == 0) {
        /*! @retval  PROCESSOR_E_INVALIDARG Size provided is zero */
        status = PROCESSOR_E_INVALIDARG;
        GT_setFailureReason (curTrace,
                             GT_4CLASS,
                             "DM8168DSPPROC_unmap",
                             status,
                             "Size provided is zero");
    }
    else {
#endif
        object = (DM8168DSPPROC_Object *) procHandle->object;
        GT_assert (curTrace, (object != NULL));

        /* Delete dynamically added non-default entries from translation
         * table only in last unmap called on that entry
         */
        for (i = AddrTable_STATIC_COUNT; i < AddrTable_SIZE; i++) {
            ai = &AddrTable[i];

            if (ai->addr[ProcMgr_AddrType_SlaveVirt] != -1u) {

                if (!ai->isMapped) {
                    continue;
                }

                startAddr = ai->addr[ProcMgr_AddrType_SlaveVirt];
                endAddr = startAddr + ai->size;

                if ((startAddr <= addr) && (addr < endAddr)) {
                    ai->refCount--;

                    if (ai->refCount == 0) {

                        GT_4trace(curTrace, GT_1CLASS,
                            "DM8168DSPPROC_unmap: removing dynamic entry: "
                            "[%d] sva=0x%x, mpa=0x%x, size=0x%x",
                            (i), ai->addr[ProcMgr_AddrType_SlaveVirt],
                            ai->addr[ProcMgr_AddrType_MasterPhys],
                            ai->size);

                        ai->addr[ProcMgr_AddrType_MasterKnlVirt] = -1u;
                        ai->addr[ProcMgr_AddrType_MasterUsrVirt] = -1u;
                        ai->addr[ProcMgr_AddrType_MasterPhys] = -1u;
                        ai->addr[ProcMgr_AddrType_SlaveVirt] = -1u;
                        ai->addr[ProcMgr_AddrType_SlavePhys] = -1u;
                        ai->size = 0u;
                        ai->isCached = FALSE;
                        ai->mapMask = 0u;
                        ai->isMapped = FALSE;

                        if (object->params.mmuEnable) {
#if 0
                            /* Remove the entry from the DSP MMU also */
                            deleteEntryArgs.size          = size;
                            deleteEntryArgs.slaveVirtAddr = addr;
                            /* TBD: elementSize, endianism, mixedSized are
                             * hard coded now, must be configurable later
                             */
                            deleteEntryArgs.elementSize   = ELEM_SIZE_16BIT;
                            deleteEntryArgs.endianism     = LITTLE_ENDIAN;
                            deleteEntryArgs.mixedSize     = MMU_TLBES;

                            status = DM8168DSP_halMmuCtrl(object->halObject,
                                Processor_MmuCtrlCmd_DeleteEntry,
                                &deleteEntryArgs);
#endif
#if !defined(SYSLINK_BUILD_OPTIMIZE)
                            if (status < 0) {
                                GT_setFailureReason(curTrace, GT_4CLASS,
                                    "DM8168DSPPROC_unmap", status,
                                    "DSP MMU configuration failed");
                            }
#endif
                        }
                    }
                }
            }
        }
#if !defined(SYSLINK_BUILD_OPTIMIZE) && defined (SYSLINK_BUILD_HLOS)
    }
#endif
    GT_1trace(curTrace, GT_LEAVE, "DM8168DSPPROC_unmap: status=0x%x", status);

    /*! @retval PROCESSOR_SUCCESS Operation successful */
    return status;
}
