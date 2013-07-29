/*
 * Do not modify this file; it is automatically generated from the template
 * linkcmd.xdt in the ti.targets.elf package and will be overwritten.
 */

/*
 * put '"'s around paths because, without this, the linker
 * considers '-' as minus operator, not a file name character.
 */


-l"/src/flex/microburst-linux/ti-dsp/syslink_2_21_00_03/examples/ex04_sharedregion/dsp/configuro/package/cfg/Dsp_pe674.oe674"
-l"/src/flex/microburst-linux/ti-dsp/syslink_2_21_00_03/packages/ti/syslink/ipc/rtos/lib/release/ti.syslink.ipc.rtos.ae674"
-l"/opt/TI/ipc_1_25_00_04/packages/ti/sdo/ipc/lib/ipc/nonInstrumented/ipc.ae674"
-l"/src/flex/microburst-linux/ti-dsp/syslink_2_21_00_03/packages/ti/syslink/utils/rtos/bios6/lib/release/ti.syslink.utils.rtos.bios6.ae674"
-l"/opt/TI/bios_6_34_02_18/packages/ti/sysbios/lib/sysbios/nonInstrumented/sysbios.ae674"
-l"/opt/TI/xdctools_3_24_05_48/packages/ti/targets/rts6000/lib/ti.targets.rts6000.ae674"
-l"/opt/TI/xdctools_3_24_05_48/packages/ti/targets/rts6000/lib/boot.ae674"

--retain="*(xdc.meta)"


--args 0x64
-heap  0x0
-stack 0x1000

MEMORY
{
    OCMC_0 (RWX) : org = 0x40300000, len = 0x40000
    OCMC_1 (RWX) : org = 0x40400000, len = 0x40000
    SR_0 (RWX) : org = 0x8e000000, len = 0x10000
    SR_1 (RWX) : org = 0x8e010000, len = 0xff0000
    DSP_PROG (RWX) : org = 0x8f000000, len = 0x1000000
}

/*
 * Linker command file contributions from all loaded packages:
 */

/* Content from xdc.services.global (null): */

/* Content from xdc (null): */

/* Content from xdc.corevers (null): */

/* Content from xdc.shelf (null): */

/* Content from xdc.services.spec (null): */

/* Content from xdc.services.intern.xsr (null): */

/* Content from xdc.services.intern.gen (null): */

/* Content from xdc.services.intern.cmd (null): */

/* Content from xdc.bld (null): */

/* Content from ti.targets (null): */

/* Content from ti.targets.elf (null): */

/* Content from xdc.rov (null): */

/* Content from xdc.runtime (null): */

/* Content from ti.targets.rts6000 (null): */

/* Content from ti.sysbios.interfaces (null): */

/* Content from ti.sysbios.family (null): */

/* Content from ti.sysbios.hal (null): */

/* Content from ti.sysbios (null): */

/* Content from ti.sysbios.knl (null): */

/* Content from ti.sysbios.gates (null): */

/* Content from ti.sysbios.family.c64p (null): */

/* Content from xdc.services.getset (null): */

/* Content from xdc.runtime.knl (null): */

/* Content from ti.sdo.ipc.family (null): */

/* Content from ti.sdo.utils (null): */

/* Content from ti.sdo.ipc.interfaces (null): */

/* Content from ti.sysbios.syncs (null): */

/* Content from ti.sysbios.heaps (null): */

/* Content from ti.sysbios.xdcruntime (null): */

/* Content from ti.sysbios.family.c62 (null): */

/* Content from ti.sysbios.timers.dmtimer (null): */

/* Content from ti.sysbios.family.c64p.ti81xx (null): */

/* Content from ti.syslink.utils.rtos.bios6 (null): */

/* Content from ti.sysbios.utils (null): */

/* Content from ti.catalog.c6000 (null): */

/* Content from ti.catalog (null): */

/* Content from ti.catalog.peripherals.hdvicp2 (null): */

/* Content from xdc.platform (null): */

/* Content from xdc.cfg (null): */

/* Content from ti.catalog.arm.peripherals.timers (null): */

/* Content from ti.catalog.arm.cortexm3 (null): */

/* Content from ti.catalog.arm.cortexa8 (null): */

/* Content from ti.platforms.evmTI816X (null): */

/* Content from ti.sdo.ipc (ti/sdo/ipc/linkcmd.xdt): */

SECTIONS
{
    ti.sdo.ipc.SharedRegion_0:  { . += 0x10000;} run > 0x8e000000, type = NOLOAD
    ti.sdo.ipc.SharedRegion_1:  { . += 0xff0000;} run > 0x8e010000, type = NOLOAD

    ti_sdo_ipc_init: load > DSP_PROG, type = NOINIT 
}


/* Content from ti.sdo.ipc.family.ti81xx (null): */

/* Content from ti.sdo.ipc.notifyDrivers (null): */

/* Content from ti.sdo.ipc.transports (null): */

/* Content from ti.sdo.ipc.nsremote (null): */

/* Content from ti.sdo.ipc.heaps (null): */

/* Content from ti.syslink.ipc.rtos (ti/syslink/ipc/rtos/linkcmd.xdt): */


    /*
     *  Set entry point to the HWI reset vector 0 to automatically satisfy
     *  any alignment constraints for the boot vector.
     */
    -eti_sysbios_family_c64p_Hwi0

    /*
     * We just modified the entry point, so suppress the "entry point symbol other
     * than _c_int00 specified" warning.
     */
    --diag_suppress=10063

    /* Add the flags needed for SysLink ELF build. */
    --dynamic
    --retain=_Ipc_ResetVector

    /* 
     * Added symbol to ensure SysBios code is retained when entrypoint is
     * something other than _c_int00.  This workaround will be removed once
     * fixed in the SysBios package
     */
    -u _c_int00

/* Content from ti.sdo.ipc.gates (null): */

/* Content from configuro (null): */


/*
 * symbolic aliases for static instance objects
 */
xdc_runtime_Startup__EXECFXN__C = 1;
xdc_runtime_Startup__RESETFXN__C = 1;
TSK_idle = ti_sysbios_knl_Task_Object__table__V + 0;

SECTIONS
{
    .text: load >> DSP_PROG
    .ti.decompress: load > DSP_PROG
    .stack: load > DSP_PROG
    GROUP: load > DSP_PROG
    {
        .bss:
        .neardata:
        .rodata:
    }
    .cinit: load > DSP_PROG
    .pinit: load >> DSP_PROG
    .init_array: load > DSP_PROG
    .const: load >> DSP_PROG
    .data: load >> DSP_PROG
    .fardata: load >> DSP_PROG
    .switch: load >> DSP_PROG
    .sysmem: load > DSP_PROG
    .far: load >> DSP_PROG
    .args: load > DSP_PROG align = 0x4, fill = 0 {_argsize = 0x64; }
    .cio: load >> DSP_PROG
    .ti.handler_table: load > DSP_PROG
    .vecs: load >> DSP_PROG
    xdc.meta: load >> DSP_PROG, type = COPY

}
