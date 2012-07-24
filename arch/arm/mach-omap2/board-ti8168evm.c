/*
 * Code for TI8168 EVM.
 *
 * Copyright (C) 2010 Texas Instruments, Inc. - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/i2c/at24.h>
#include <linux/device.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/device.h>
#include <linux/mtd/nand.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/mtd/physmap.h>
#include <linux/phy.h>
#include <linux/phy_fixed.h>
#include <linux/gpio.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/gpio-regulator.h>

#include <mach/hardware.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>

#include <plat/mcspi.h>
#include <plat/irqs.h>
#include <plat/board.h>
#include <plat/common.h>
#include <plat/asp.h>
#include <plat/usb.h>
#include <plat/mmc.h>
#include <plat/gpmc.h>
//#include <plat/nand.h>
//#include <plat/hdmi_lib.h>

#include "control.h"


#include "clock.h"
#include "mux.h"
#include "hsmmc.h"
#include "board-flash.h"
#include <mach/board-ti816x.h>

static struct omap2_hsmmc_info mmc[] = {
	{
		.mmc		= 1,
		.caps           = MMC_CAP_4_BIT_DATA,
		.gpio_cd	= -EINVAL,/* Dedicated pins for CD and WP */
		.gpio_wp	= -EINVAL,
		.ocr_mask	= MMC_VDD_33_34,
	},
	{}	/* Terminator */
};

static struct mtd_partition ti816x_evm_norflash_partitions[] = {
	/* bootloader (U-Boot, etc) in first 5 sectors */
	{
		.name		= "bootloader",
		.offset		= 0,
		.size		= 2 * SZ_128K,
		.mask_flags	= MTD_WRITEABLE, /* force read-only */
	},
	/* bootloader params in the next 1 sectors */
	{
		.name		= "env",
		.offset		= MTDPART_OFS_APPEND,
		.size		= SZ_128K,
		.mask_flags	= 0,
	},
	/* kernel */
	{
		.name		= "kernel",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 2 * SZ_2M,
		.mask_flags	= 0
	},
	/* file system */
	{
		.name		= "filesystem",
		.offset		= MTDPART_OFS_APPEND,
		.size		= 25 * SZ_2M,
		.mask_flags	= 0
	},
	/* reserved */
	{
		.name		= "reserved",
		.offset		= MTDPART_OFS_APPEND,
		.size		= MTDPART_SIZ_FULL,
		.mask_flags	= 0
	}
};


#define NAND_BLOCK_SIZE					SZ_128K

/* Macro for GPIO voltage regulator */
#define VR_GPIO_INSTANCE	0

static struct mtd_partition ti816x_nand_partitions[] = {
/* All the partition sizes are listed in terms of NAND block size */
	{
		.name           = "U-Boot",
		.offset         = 0,    /* Offset = 0x0 */
		.size           = 19 * NAND_BLOCK_SIZE,
	},
	{
		.name           = "U-Boot Env",
		.offset         = MTDPART_OFS_APPEND,   /* Offset = 0x260000 */
		.size           = 1 * NAND_BLOCK_SIZE,
	},
	{
		.name           = "Kernel",
		.offset         = MTDPART_OFS_APPEND,   /* Offset = 0x280000 */
		.size           = 34 * NAND_BLOCK_SIZE,
	},
	{
		.name           = "File System",
		.offset         = MTDPART_OFS_APPEND,   /* Offset = 0x6C0000 */
		.size           = 1601 * NAND_BLOCK_SIZE,
	},
	{
		.name           = "Reserved",
		.offset         = MTDPART_OFS_APPEND,   /* Offset = 0xCEE0000 */
		.size           = MTDPART_SIZ_FULL,
	},
};

#ifdef CONFIG_REGULATOR_GPIO
static struct regulator_consumer_supply ti816x_gpio_dcdc_supply[] = {
	{
		.supply = "vdd_avs",
	},
};

static struct regulator_init_data gpio_pmic_init_data = {
	.constraints = {
		.min_uV		= 800000,
		.max_uV		= 1025000,
		.valid_ops_mask	= (REGULATOR_CHANGE_VOLTAGE |
			REGULATOR_CHANGE_STATUS),
	},
	.num_consumer_supplies	= ARRAY_SIZE(ti816x_gpio_dcdc_supply),
	.consumer_supplies	= ti816x_gpio_dcdc_supply,
};

/* Supported voltage values for regulators */
static struct gpio_vr_data ti816x_vsel_table[] = {
	{0x0, 800000}, {0x8, 815000}, {0x4, 830000}, {0xC, 845000},
	{0x2, 860000}, {0xA, 875000}, {0x6, 890000}, {0xE, 905000},
	{0x1, 920000}, {0x9, 935000}, {0x5, 950000}, {0xD, 965000},
	{0x3, 980000}, {0xB, 995000}, {0x7, 1010000}, {0xF, 1025000},
};

static struct gpio vcore_gpios[] = {
	{ (VR_GPIO_INSTANCE * 32) + 0, GPIOF_IN, "vgpio 0"},
	{ (VR_GPIO_INSTANCE * 32) + 1, GPIOF_IN, "vgpio 1"},
	{ (VR_GPIO_INSTANCE * 32) + 2, GPIOF_IN, "vgpio 2"},
	{ (VR_GPIO_INSTANCE * 32) + 3, GPIOF_IN, "vgpio 3"},
};

/* GPIO regulator platform data */
static struct gpio_reg_platform_data gpio_vr_init_data = {
	.name			= "VFB",
	.pmic_init_data		= &gpio_pmic_init_data,
	.gpio_vsel_table	= ti816x_vsel_table,
	.num_voltages		= ARRAY_SIZE(ti816x_vsel_table),
	.gpios			= vcore_gpios,
	.gpio_single_bank	= true,
	.gpio_arr_mask		= 0xF,
	.num_gpio_pins		= ARRAY_SIZE(vcore_gpios),
	.pmic_vout		= 600000,
};

/* VCORE for SR regulator init */
static struct platform_device ti816x_gpio_vr_device = {
	.name		= "gpio_vr",
	.id		= -1,
	.dev = {
		.platform_data = &gpio_vr_init_data,
	},
};

static void __init ti816x_gpio_vr_init(void)
{
	if (platform_device_register(&ti816x_gpio_vr_device))
		printk(KERN_ERR "failed to register ti816x_gpio_vr device\n");
	else
		printk(KERN_INFO "registered ti816x_gpio_vr device\n");
}
#else
static inline void ti816x_gpio_vr_init(void) {}
#endif

static struct i2c_board_info __initdata ti816x_i2c_boardinfo0[] = {
	{
	  I2C_BOARD_INFO("adau1761", 0x70), // codec
	},
/*
	{
	  I2C_BOARD_INFO("R5H30211", 0x22), // MCU
	},
	{
	  I2C_BOARD_INFO("M24C64", 0x20), //eeprom
	},
*/
	/* Not present yet
	{
		I2C_BOARD_INFO("CY8C3446AX1", 0x04),
	},
	{
		I2C_BOARD_INFO("CY8C3446AX2", 0x08),
	},
	*/
};

/* For future device expansion
static struct i2c_board_info __initdata ti816x_i2c_boardinfo1[] = {
// PCA9517 hub   N/A
	{
		I2C_BOARD_INFO("xyzzy", 0x39),
	},
};
*/

static int __init ti816x_evm_i2c_init(void)
{
	omap_register_i2c_bus(1, 100, ti816x_i2c_boardinfo0,
		ARRAY_SIZE(ti816x_i2c_boardinfo0));
	//omap_register_i2c_bus(2, 100, ti816x_i2c_boardinfo1,
	//	ARRAY_SIZE(ti816x_i2c_boardinfo1));
	return 0;
}

/* SPI fLash information */
struct mtd_partition ti816x_spi_partitions[] = {
	/* All the partition sizes are listed in terms of NAND block size */
	{
		.name		= "U-Boot",
		.offset		= 0,	/* Offset = 0x0 */
		.size		= 64 * SZ_4K,
		.mask_flags	= MTD_WRITEABLE,	/* force read-only */
	},
	{
		.name		= "U-Boot Env",
		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0x40000 */
		.size		= 2 * SZ_4K,
	},
	{
		.name		= "Kernel",
		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0x42000 */
		.size		= 640 * SZ_4K,
	},
	{
		.name		= "File System",
		.offset		= MTDPART_OFS_APPEND,	/* Offset = 0x2C2000 */
		.size		= MTDPART_SIZ_FULL,	/* size = 1.24 MiB */
	}
};

const struct flash_platform_data ti816x_spi_flash = {
	.name		= "spi_flash",
	.parts		= ti816x_spi_partitions,
	.nr_parts	= ARRAY_SIZE(ti816x_spi_partitions),
};

struct spi_board_info __initdata ti816x_spi_slave_info[] = {
	{
		.modalias	= "m25p80",
		.platform_data	= &ti816x_spi_flash,
		.irq		= -1,
		.max_speed_hz	= 75000000,
		.bus_num	= 1,
		.chip_select	= 0,
	},
};

static void __init ti816x_spi_init(void)
{
	spi_register_board_info(ti816x_spi_slave_info,
				ARRAY_SIZE(ti816x_spi_slave_info));
}

static struct platform_device flex_adau1x61_device = {
	.name = "bfin-eval-adau1x61",
};

static struct platform_device *flex_devices[] __initdata = {
	&flex_adau1x61_device,
};

static void __init ti8168_evm_init_irq(void)
{
	omap2_init_common_infrastructure();
	omap2_init_common_devices(NULL, NULL);
	omap_init_irq();
	gpmc_init();
}

static u8 ti8168_iis_serializer_direction[] = {
	TX_MODE,	RX_MODE,	INACTIVE_MODE,	INACTIVE_MODE,
	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,
	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,
	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,	INACTIVE_MODE,
};

static struct snd_platform_data ti8168_evm_snd_data = {
	.tx_dma_offset	= 0x46800000,
	.rx_dma_offset	= 0x46800000,
	.op_mode	= DAVINCI_MCASP_IIS_MODE,
	.num_serializer = ARRAY_SIZE(ti8168_iis_serializer_direction),
	.tdm_slots	= 2,
	.serial_dir	= ti8168_iis_serializer_direction,
	.asp_chan_q	= EVENTQ_2,
	.version	= MCASP_VERSION_2,
	.txnumevt	= 1,
	.rxnumevt	= 1,
};

static struct omap_musb_board_data musb_board_data = {
	.interface_type		= MUSB_INTERFACE_ULPI,
#ifdef CONFIG_USB_MUSB_OTG
	.mode           = MUSB_OTG,
#elif defined(CONFIG_USB_MUSB_HDRC_HCD)
	.mode           = MUSB_HOST,
#elif defined(CONFIG_USB_GADGET_MUSB_HDRC)
	.mode           = MUSB_PERIPHERAL,
#endif
	.power		= 500,
	.instances	= 1,
};

#ifdef CONFIG_OMAP_MUX
static struct omap_board_mux board_mux[] __initdata = {

	/* PIN mux for non-muxed NOR */
	TI816X_MUX(TIM7_OUT, OMAP_MUX_MODE1),	/* gpmc_a12 */
	TI816X_MUX(UART1_CTSN, OMAP_MUX_MODE1),	/* gpmc_a13 */
	TI816X_MUX(UART1_RTSN, OMAP_MUX_MODE1),	/* gpmc_a14 */
	TI816X_MUX(UART2_RTSN, OMAP_MUX_MODE1),	/* gpmc_a15 */
	/* REVISIT: why 2 lines configured as gpmc_a15 */
	TI816X_MUX(SC1_RST, OMAP_MUX_MODE1),	/* gpmc_a15 */
	TI816X_MUX(UART2_CTSN, OMAP_MUX_MODE1),	/* gpmc_a16 */
	TI816X_MUX(UART0_RIN, OMAP_MUX_MODE1),	/* gpmc_a17 */
	TI816X_MUX(UART0_DCDN, OMAP_MUX_MODE1),	/* gpmc_a18 */
	TI816X_MUX(UART0_DSRN, OMAP_MUX_MODE1),	/* gpmc_a19 */
	TI816X_MUX(UART0_DTRN, OMAP_MUX_MODE1),	/* gpmc_a20 */
	TI816X_MUX(SPI_SCS3, OMAP_MUX_MODE1),	/* gpmc_a21 */
	TI816X_MUX(SPI_SCS2, OMAP_MUX_MODE1),	/* gpmc_a22 */
	TI816X_MUX(GP0_IO6, OMAP_MUX_MODE2),	/* gpmc_a23 */
	TI816X_MUX(TIM6_OUT, OMAP_MUX_MODE1),	/* gpmc-a24 */
	TI816X_MUX(SC0_DATA, OMAP_MUX_MODE1),	/* gpmc_a25 */
	/* for controlling high address */
	TI816X_MUX(GPMC_A27, OMAP_MUX_MODE1),	/* gpio-20 */
	{ .reg_offset = OMAP_MUX_TERMINATOR },
};
#else
#define board_mux	NULL
#endif

int __init ti_ahci_register(u8 num_inst);

static struct fixed_phy_status fixed_phy_status __initdata = {
	.link		= 1,
	.speed		= 100,
	.duplex		= 1,
};

static void __init ti8168_evm_init(void)
{
	int bw; /* bus-width */

	ti81xx_mux_init(board_mux);
	omap_serial_init();
	ti816x_evm_i2c_init();
	ti81xx_register_mcasp(0, &ti8168_evm_snd_data);
	ti816x_spi_init();
	/* initialize usb */
	usb_musb_init(&musb_board_data);
	/* register devices (sound card) */
	platform_add_devices(flex_devices, ARRAY_SIZE(flex_devices));
	/* nand initialisation */
	if (cpu_is_ti81xx()) {
		u32 *control_status = TI81XX_CTRL_REGADDR(0x40);
		if (*control_status & (1<<16))
			bw = 2;	/*16-bit nand if BTMODE BW pin on board is ON*/
		else
			bw = 0;	/*8-bit nand if BTMODE BW pin on board is OFF*/

		board_nand_init(ti816x_nand_partitions,
			ARRAY_SIZE(ti816x_nand_partitions), 0, bw);
	} else
		board_nand_init(ti816x_nand_partitions,
		ARRAY_SIZE(ti816x_nand_partitions), 0, NAND_BUSWIDTH_16);

	omap2_hsmmc_init(mmc);
	board_nor_init(ti816x_evm_norflash_partitions,
		ARRAY_SIZE(ti816x_evm_norflash_partitions), 0);
	ti816x_gpio_vr_init();

	regulator_has_full_constraints();
	regulator_use_dummy_regulator();

	/* Add a fixed phy @ address 0:00 for the FPGA interface */
	fixed_phy_add(PHY_POLL, 0, &fixed_phy_status);
}

static int __init ti8168_evm_gpio_setup(void)
{
	/* GPIO-20 should be low for NOR access beyond 4KiB */
	gpio_request(20, "nor");
	gpio_direction_output(20, 0x0);
	return 0;
}
/* GPIO setup should be as subsys_initcall() as gpio driver
 * is registered in arch_initcall()
 */
subsys_initcall(ti8168_evm_gpio_setup);

static void __init ti8168_evm_map_io(void)
{
	omap2_set_globals_ti816x();
	ti81xx_map_common_io();
}

MACHINE_START(TI8168EVM, "Flex6000Series")
	/* Maintainer: Texas Instruments */
	.boot_params	= 0x80000100,
	.map_io		= ti8168_evm_map_io,
	.reserve         = ti81xx_reserve,
	.init_irq	= ti8168_evm_init_irq,
	.init_machine	= ti8168_evm_init,
	.timer		= &omap_timer,
MACHINE_END
