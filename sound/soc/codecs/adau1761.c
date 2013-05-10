/*
 * Driver for ADAU1761/ADAU1461/ADAU1761/ADAU1961 codec
 *
 * Copyright 2011 Analog Devices Inc.
 * Author: Lars-Peter Clausen <lars@metafoo.de>
 *
 * Licensed under the GPL-2 or later.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <sound/adau17x1.h>

#include "adau17x1.h"

#include "microburst-sigmadsp.h"

#define ADAU1761_DIGMIC_JACKDETECT	0x4008
#define ADAU1761_REC_MIXER_LEFT0	0x400a
#define ADAU1761_REC_MIXER_LEFT1	0x400b
#define ADAU1761_REC_MIXER_RIGHT0	0x400c
#define ADAU1761_REC_MIXER_RIGHT1	0x400d
#define ADAU1761_LEFT_DIFF_INPUT_VOL	0x400e
#define ADAU1761_RIGHT_DIFF_INPUT_VOL	0x400f
#define ADAU1761_ALC_CONTROL0		0x4011
#define ADAU1761_ALC_CONTROL1		0x4012
#define ADAU1761_ALC_CONTROL2		0x4013
#define ADAU1761_ALC_CONTROL3		0x4014
#define ADAU1761_PLAY_LR_MIXER_LEFT	0x4020
#define ADAU1761_PLAY_MIXER_LEFT0	0x401c
#define ADAU1761_PLAY_MIXER_LEFT1	0x401d
#define ADAU1761_PLAY_MIXER_RIGHT0	0x401e
#define ADAU1761_PLAY_MIXER_RIGHT1	0x401f
#define ADAU1761_PLAY_LR_MIXER_RIGHT	0x4021
#define ADAU1761_PLAY_MIXER_MONO	0x4022
#define ADAU1761_PLAY_HP_LEFT_VOL	0x4023
#define ADAU1761_PLAY_HP_RIGHT_VOL	0x4024
#define ADAU1761_PLAY_LINE_LEFT_VOL	0x4025
#define ADAU1761_PLAY_LINE_RIGHT_VOL	0x4026
#define ADAU1761_PLAY_MONO_OUTPUT_VOL	0x4027
#define ADAU1761_POP_CLICK_SUPPRESS	0x4028
#define ADAU1761_JACK_DETECT_PIN	0x4031
#define ADAU1761_DEJITTER		0x4036
#define ADAU1761_CLK_ENABLE0		0x40f9
#define ADAU1761_CLK_ENABLE1		0x40fa

#define ADAU1761_DIGMIC_JACKDETECT_ACTIVE_LOW	BIT(0)
#define ADAU1761_DIGMIC_JACKDETECT_DIGMIC	BIT(5)

#define ADAU1761_DIFF_INPUT_VOL_LDEN		0x01

#define ADAU1761_FIRMWARE "adau1761.bin"

/* Safeload Registers for SigmaDSP Firmware */

#define ADAU1761_SAFELOAD_DATA(x) (0x1 + (x))
#define ADAU1761_SAFELOAD_ADDR		0x6
#define ADAU1761_SAFELOAD_SIZE		0x7

/********************************************/

static struct reg_default adau1761_reg_defaults[] = {
	{ ADAU1761_DEJITTER,			0x03 },
	{ ADAU1761_DIGMIC_JACKDETECT,		0x00 },
	{ ADAU1761_REC_MIXER_LEFT0,		0x00 },
	{ ADAU1761_REC_MIXER_LEFT1,		0x00 },
	{ ADAU1761_REC_MIXER_RIGHT0,		0x00 },
	{ ADAU1761_REC_MIXER_RIGHT1,		0x00 },
	{ ADAU1761_LEFT_DIFF_INPUT_VOL,		0x00 },
	{ ADAU1761_RIGHT_DIFF_INPUT_VOL,	0x00 },
	{ ADAU1761_ALC_CONTROL0,		0x00 },
	{ ADAU1761_ALC_CONTROL1,		0x00 },
	{ ADAU1761_ALC_CONTROL2,		0x00 },
	{ ADAU1761_ALC_CONTROL3,		0x00 },
	{ ADAU1761_PLAY_LR_MIXER_LEFT,		0x00 },
	{ ADAU1761_PLAY_MIXER_LEFT0,		0x00 },
	{ ADAU1761_PLAY_MIXER_LEFT1,		0x00 },
	{ ADAU1761_PLAY_MIXER_RIGHT0,		0x00 },
	{ ADAU1761_PLAY_MIXER_RIGHT1,		0x00 },
	{ ADAU1761_PLAY_LR_MIXER_RIGHT,		0x00 },
	{ ADAU1761_PLAY_MIXER_MONO,		0x00 },
	{ ADAU1761_PLAY_HP_LEFT_VOL,		0x00 },
	{ ADAU1761_PLAY_HP_RIGHT_VOL,		0x00 },
	{ ADAU1761_PLAY_LINE_LEFT_VOL,		0x00 },
	{ ADAU1761_PLAY_LINE_RIGHT_VOL,		0x00 },
	{ ADAU1761_PLAY_MONO_OUTPUT_VOL,	0x00 },
	{ ADAU1761_POP_CLICK_SUPPRESS,		0x00 },
	{ ADAU1761_JACK_DETECT_PIN,		0x00 },
	{ ADAU1761_CLK_ENABLE0,			0x00 },
	{ ADAU1761_CLK_ENABLE1,			0x00 },
	{ ADAU17X1_CLOCK_CONTROL,		0x00 },
	{ ADAU17X1_PLL_CONTROL,			0x00 },
	{ ADAU17X1_REC_POWER_MGMT,		0x00 },
	{ ADAU17X1_MICBIAS,			0x00 },
	{ ADAU17X1_SERIAL_PORT0,		0x00 },
	{ ADAU17X1_SERIAL_PORT1,		0x00 },
	{ ADAU17X1_CONVERTER0,			0x04 },
	{ ADAU17X1_CONVERTER1,			0x00 },
	{ ADAU17X1_LEFT_INPUT_DIGITAL_VOL,	0x00 },
	{ ADAU17X1_RIGHT_INPUT_DIGITAL_VOL,	0x00 },
	{ ADAU17X1_ADC_CONTROL,			0x00 },
	{ ADAU17X1_PLAY_POWER_MGMT,		0x00 },
	{ ADAU17X1_DAC_CONTROL0,		0x00 },
	{ ADAU17X1_DAC_CONTROL1,		0x00 },
	{ ADAU17X1_DAC_CONTROL2,		0x00 },
	{ ADAU17X1_SERIAL_PORT_PAD,		0x00 },
	{ ADAU17X1_CONTROL_PORT_PAD0,		0x00 },
	{ ADAU17X1_CONTROL_PORT_PAD1,		0x00 },
	{ ADAU17X1_DSP_SAMPLING_RATE,		0x03 },
	{ ADAU17X1_SERIAL_INPUT_ROUTE,		0x04 },
	{ ADAU17X1_SERIAL_OUTPUT_ROUTE,		0x04 },
	{ ADAU17X1_DSP_ENABLE,			0x00 },
	{ ADAU17X1_DSP_RUN,			0x00 },
	{ ADAU17X1_SERIAL_SAMPLING_RATE,	0x04 },
};

/* Lookup tables for SigmaDSP Firmware parameters */

/* ONE and ZERO definitions */

uint32_t MICROBURST_SIGMADSP_FIXPT_ZERO	= 0x00000000;
uint32_t MICROBURST_SIGMADSP_FIXPT_ONE = 0x00800000;

/* Level control lookup tables */

uint32_t MICROBURST_SIGMADSP_FIXPT_LEVEL_LOOKUP_64_STEP_MINUS_96_TO_ZERO[64] = { 0x0000009E, 0x000000BC,
		0x000000DF, 0x00000109, 0x0000013B, 0x00000177, 0x000001BD, 0x00000211, 0x00000275, 0x000002EC,
		0x00000379, 0x00000420, 0x000004E7, 0x000005D4, 0x000006ED, 0x0000083B, 0x000009C8, 0x00000BA0,
		0x00000DD1, 0x0000106C, 0x00001385, 0x00001733, 0x00001B92, 0x000020C5, 0x000026F2, 0x00002E49,
		0x00003703, 0x00004161, 0x00004DB5, 0x00005C5A, 0x00006DC3, 0x00008274, 0x00009B0B, 0x0000B845,
		0x0000DB01, 0x00010449, 0x0001355A, 0x00016FAA, 0x0001B4F8, 0x00020756, 0x0002693C, 0x0002DD96,
		0x000367DE, 0x00040C37, 0x0004CF8B, 0x0005B7B1, 0x0006CB9A, 0x00081385, 0x00099941, 0x000B6873,
		0x000D8EF6, 0x00101D3F, 0x001326DD, 0x0016C311, 0x001B0D7B, 0x002026F3, 0x00263680, 0x002D6A86,
		0x0035FA27, 0x004026E7, 0x004C3EA8, 0x005A9DF8, 0x006BB2D6, 0x00800000 };  //table for 64 1.5 dB steps

uint32_t MICROBURST_SIGMADSP_FIXPT_LEVEL_LOOKUP_32_STEP_MINUS_96_TO_ZERO[32] = { 0x000000BC,
		0x00000109, 0x00000177, 0x00000211, 0x000002EC,
		0x00000420, 0x000005D4, 0x0000083B, 0x00000BA0,
		0x0000106C, 0x00001733, 0x000020C5, 0x00002E49,
		0x00004161, 0x00005C5A, 0x00008274, 0x0000B845,
		0x00010449, 0x00016FAA, 0x00020756, 0x0002DD96,
		0x00040C37, 0x0005B7B1, 0x00081385, 0x000B6873,
		0x00101D3F, 0x0016C311, 0x002026F3, 0x002D6A86,
		0x004026E7, 0x005A9DF8, 0x00800000 };   //table for 32 3 dB steps

/* TX_FILTER Low Pass Parameters */

uint32_t MICROBURST_SIGMADSP_FIXPT_TX_LPF_10000HZ[8] = { 0x0068464F, 0x00D08C9F, 0x0068464F, 0xFFA87724, 0xFF319B32,
		0x000002B8, 0x0000028C, 0x00000001 };
uint32_t MICROBURST_SIGMADSP_FIXPT_TX_LPF_8000HZ[8] = { 0x0053954A, 0x00A72A95, 0x0053954A, 0xFFC5E3AA, 0xFF67E819,
		0x000002B8, 0x0000028C, 0x00000001 };
uint32_t MICROBURST_SIGMADSP_FIXPT_TX_LPF_4000HZ[8] = { 0x00283B63, 0x005076C5, 0x00283B63, 0xFFE4EBF3, 0xFFF84977,
		0x000002B8, 0x0000028C, 0x00000001 };
uint32_t MICROBURST_SIGMADSP_FIXPT_TX_LPF_3000HZ[8] = { 0x001C3366, 0x003866CC, 0x001C3366, 0xFFE29A52, 0x002B49B3,
		0x000002B8, 0x0000028C, 0x00000001 };
uint32_t MICROBURST_SIGMADSP_FIXPT_TX_LPF_2500HZ[8] = { 0x001617AF, 0x002C2F5F, 0x001616AF, 0xFFDE41F5, 0x00485958,
		0x000002B8, 0x0000028C, 0x00000001 };
uint32_t MICROBURST_SIGMADSP_FIXPT_TX_LPF_2000HZ[8] = { 0x001011B5, 0x0020236A, 0x001011B5, 0xFFD6DAD1, 0x00681FD1,
		0x000002B8, 0x0000028C, 0x00000001 };

/* TX_FILTER High Pass Parameters */

uint32_t MICROBURST_SIGMADSP_FIXPT_TX_HPF_60HZ[8] = { 0x007DD334, 0xFF045997, 0x007DD334, 0xFF816E67, 0x00FE8F2C,
		0x000002C2, 0x00000294, 0x00000001 };
uint32_t MICROBURST_SIGMADSP_FIXPT_TX_HPF_100HZ[8] = { 0x007D5A87, 0xFF054AF2, 0x007D5A87, 0xFF826068, 0x00FD98E1,
		0x000002C2, 0x00000294, 0x00000001 };
uint32_t MICROBURST_SIGMADSP_FIXPT_TX_HPF_200HZ[8] = { 0x007C2E69, 0xFF07A32D, 0x007C2E69, 0xFF84B5A3, 0x00FB2FBF,
		0x000002C2, 0x00000294, 0x00000001 };
uint32_t MICROBURST_SIGMADSP_FIXPT_TX_HPF_400HZ[8] = { 0x0079DCAC, 0xFF0C46A7, 0x0079DCAC, 0xFF893FDD, 0x00F65785,
		0x000002C2, 0x00000294, 0x00000001 };
uint32_t MICROBURST_SIGMADSP_FIXPT_TX_HPF_450HZ[8] = { 0x0079498A, 0xFF0D6CEC, 0x0079498A, 0xFF8A5BED, 0x00F5203A,
		0x000002C2, 0x00000294, 0x00000001 };
uint32_t MICROBURST_SIGMADSP_FIXPT_TX_HPF_500HZ[8] = { 0x0078B6E8, 0xFF0E9231, 0x0078B6E8, 0xFF8B7578, 0x00F3E871,
		0x000002C2, 0x00000294, 0x00000001 };


/* Safeload write function for SigmaDSP Firmware parameters */

static int adau1761_safeload_write(struct adau *adau, uint32_t addr, uint32_t *data,
          unsigned int size)
{
          unsigned int i;
          int ret;

          if (size > 5)
                    return -EINVAL;

          for (i = 0; i < size; i++) {
                    ret = regmap_write(adau->regmap, ADAU1761_SAFELOAD_DATA(i), htonl(data[i]));
                    if (ret)
                              return ret;
          }

          ret = regmap_write(adau->regmap, ADAU1761_SAFELOAD_ADDR, addr - 1);
          if (ret)
                    return ret;
          ret = regmap_write(adau->regmap, ADAU1761_SAFELOAD_SIZE, size);
          if (ret)
                    return ret;

          /* Wait for the operation to finish */
          udelay(30);

          return 0;
}

/* Block write function for SigmaDSP Firmware parameters     */

static int adau1761_block_write(struct adau *adau, uint32_t addr, uint32_t *data,
          unsigned int size)
{
          int ret;
          int i;
          uint32_t data_swapped[(size/4)];
          printk (KERN_DEBUG "MB-sigmadsp: register block write addr %d size %d\n", addr, size);
          for (i = 0; i < size/4; i++)
          {
        	  printk (KERN_DEBUG "MB-sigmadsp: %d: %08X\n", addr+i, data[i]);
        	  data_swapped[i] = htonl(data[i]);
          }
          ret = regmap_raw_write(adau->regmap, addr, data_swapped, size);
		  return ret;
};

/* Utility functions to convert SigmaDSP Firmware 5.23 paramters between decimal and hex values */
/*
float sigma523_to_float(uint32_t param_value)
{
	return (float)(param_value / 8388608.0);
}

uint32_t float_to_sigma523(float param_value)
{
	return (uint32_t)(param_value * 8388608);
}
*/

/* Microburst SigmaDSP kcontrol definitions */

static const unsigned int microburst_sigmadsp_input_source_values[] = {
		0, 1, 2,
};

static const char * const microburst_sigmadsp_input_source_text[] = {
		"Signal Generator", "Balanced Input", "Microphone",
};

static const char * const microburst_sigmadsp_monitor_voice_cw_text[] = {
		"Monitor Voice", "Monitor CW",
};

/* Microburst SigmaDSP kcontrol functions */

static int microburst_sigmadsp_cw_key_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	printk (KERN_DEBUG "MB-codecdsp: microburst_sigmadsp_cw_key_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_cw_key_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int key_state;
	uint32_t cw_key_addr = MOD_CW_KEY_ISON_ADDR;
	uint32_t key_down = MICROBURST_SIGMADSP_FIXPT_ONE;
	uint32_t key_up = MICROBURST_SIGMADSP_FIXPT_ZERO;
	key_state = ucontrol->value.integer.value[0];

	printk (KERN_DEBUG "MB-codecdsp: microburst_sigmadsp_cw_key_put called.  Key state: %d\n", key_state);
	if (key_state) {
		//printk ("MB-codecdsp: setting key down state\n");
		adau1761_block_write(adau, cw_key_addr, &key_down ,sizeof(key_down));
	}
	else  {
		//printk ("MB-codecdsp: setting key up state\n");
		adau1761_block_write(adau, cw_key_addr, &key_up, sizeof(key_up));
	}
	return 0;
};

static int microburst_sigmadsp_monitor_voice_cw_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	printk (KERN_DEBUG "MB-sigmadsp: monitor_voice_cw_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_monitor_voice_cw_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	printk (KERN_DEBUG "MB-sigmadsp: monitor_voice_cw_put called\n");
	uint32_t buf[2];
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int mon_cw;		//monitor CW is state 1, monitor Voice is state 0
	uint32_t mux_addr = MOD_MONITOR_VOICE_CW_ALG0_STAGE0_MONOSWITCHNOSLEW_ADDR;
	mon_cw = ucontrol->value.integer.value[0];
	if (mon_cw)
	{
		buf[0] = MICROBURST_SIGMADSP_FIXPT_ZERO;
		buf[1] = MICROBURST_SIGMADSP_FIXPT_ONE;
	}
	else
	{
		buf[0] = MICROBURST_SIGMADSP_FIXPT_ONE;
		buf[1] = MICROBURST_SIGMADSP_FIXPT_ZERO;
	}

	printk (KERN_DEBUG "MB-sigmadsp: monitor voice cw setting to %d\n", mon_cw);
	adau1761_block_write(adau, mux_addr, &buf, 8);

	return 0;
};

static int microburst_sigmadsp_compander_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	printk (KERN_DEBUG "MB-sigmadsp: compander_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_compander_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	printk (KERN_DEBUG "MB-sigmadsp: compander_put called\n");
	uint32_t buf[2];
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int compander_enable;
	uint32_t mux_addr = MOD_COMPANDER_ENABLE_ALG0_STAGE0_MONOSWITCHNOSLEW_ADDR;
	compander_enable = ucontrol->value.integer.value[0];
	if (compander_enable)
	{
		buf[0] = MICROBURST_SIGMADSP_FIXPT_ZERO;
		buf[1] = MICROBURST_SIGMADSP_FIXPT_ONE;
	}
	else
	{
		buf[0] = MICROBURST_SIGMADSP_FIXPT_ONE;
		buf[1] = MICROBURST_SIGMADSP_FIXPT_ZERO;
	}

	printk (KERN_DEBUG "MB-sigmadsp: compander setting to %d\n", compander_enable);
	adau1761_block_write(adau, mux_addr, &buf, 8);

	return 0;
};

static int microburst_sigmadsp_tx_eq_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	printk (KERN_DEBUG "MB-sigmadsp: tx_eq_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_tx_eq_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	printk (KERN_DEBUG "MB-sigmadsp: tx_eq_put called\n");
	uint32_t buf[2];
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int tx_eq_enable;
	uint32_t mux_addr = MOD_TX_EQ_ENABLE_ALG0_STAGE0_MONOSWITCHNOSLEW_ADDR;
	tx_eq_enable = ucontrol->value.integer.value[0];
	if (tx_eq_enable)
	{
		buf[0] = MICROBURST_SIGMADSP_FIXPT_ZERO;
		buf[1] = MICROBURST_SIGMADSP_FIXPT_ONE;
	}
	else
	{
		buf[0] = MICROBURST_SIGMADSP_FIXPT_ONE;
		buf[1] = MICROBURST_SIGMADSP_FIXPT_ZERO;
	}

	printk (KERN_DEBUG "MB-sigmadsp: tx_eq setting to %d\n", tx_eq_enable);
	adau1761_block_write(adau, mux_addr, &buf, 8);

	return 0;
};

static int microburst_sigmadsp_rx_eq_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	printk (KERN_DEBUG "MB-sigmadsp: rx_eq_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_rx_eq_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	printk (KERN_DEBUG "MB-sigmadsp: rx_eq_put called\n");
	uint32_t buf[2];
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int rx_eq_enable;
	uint32_t mux_addr = MOD_RX_EQ_ENABLE_ALG0_STAGE0_STEREOSWITCHNOSLEW_ADDR;
	rx_eq_enable = ucontrol->value.integer.value[0];
	if (rx_eq_enable)
	{
		buf[0] = MICROBURST_SIGMADSP_FIXPT_ZERO;
		buf[1] = MICROBURST_SIGMADSP_FIXPT_ONE;
	}
	else
	{
		buf[0] = MICROBURST_SIGMADSP_FIXPT_ONE;
		buf[1] = MICROBURST_SIGMADSP_FIXPT_ZERO;
	}

	printk (KERN_DEBUG "MB-sigmadsp: rx_eq setting to %d\n", rx_eq_enable);
	adau1761_block_write(adau, mux_addr, &buf, 8);

	return 0;
};

static int microburst_sigmadsp_meter_select_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	printk (KERN_DEBUG "MB-sigmadsp: meter_select_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_meter_select_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	printk (KERN_DEBUG "MB-sigmadsp: meter_select_put called\n");
	uint32_t buf[2];
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int meter_select;		// 0 is meter input   1 is meter output
	uint32_t mux_addr = MOD_METER_SELECT_ALG0_STAGE0_MONOSWITCHNOSLEW_ADDR;
	meter_select = ucontrol->value.integer.value[0];
	if (meter_select)
	{
		buf[0] = MICROBURST_SIGMADSP_FIXPT_ZERO;
		buf[1] = MICROBURST_SIGMADSP_FIXPT_ONE;
	}
	else
	{
		buf[0] = MICROBURST_SIGMADSP_FIXPT_ONE;
		buf[1] = MICROBURST_SIGMADSP_FIXPT_ZERO;
	}

	printk (KERN_DEBUG "MB-sigmadsp: meter_select setting to %d\n", meter_select);
	adau1761_block_write(adau, mux_addr, &buf, 8);

	return 0;
};

static int microburst_sigmadsp_echo_cancel_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	printk (KERN_DEBUG "MB-sigmadsp: echo_cancel_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_echo_cancel_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	printk (KERN_DEBUG "MB-sigmadsp: echo_cancel_put called\n");
	uint32_t buf[2];
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int echo_cancel;
	uint32_t mux_addr = MOD_ECHO_CANCEL_ENABLE_ALG0_STAGE0_MONOSWITCHNOSLEW_ADDR;
	echo_cancel = ucontrol->value.integer.value[0];
	if (echo_cancel)
	{
		buf[0] = MICROBURST_SIGMADSP_FIXPT_ZERO;
		buf[1] = MICROBURST_SIGMADSP_FIXPT_ONE;
	}
	else
	{
		buf[0] = MICROBURST_SIGMADSP_FIXPT_ONE;
		buf[1] = MICROBURST_SIGMADSP_FIXPT_ZERO;
	}

	printk (KERN_DEBUG "MB-sigmadsp: echo_cancel setting to %d\n", echo_cancel);
	adau1761_block_write(adau, mux_addr, &buf, 8);

	return 0;
};

static int microburst_sigmadsp_input_source_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	printk (KERN_DEBUG "MB-sigmadsp: input_source_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_input_source_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	printk (KERN_DEBUG "MB-sigmadsp: input_source_put called\n");
	uint32_t buf[3];
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int input_source;		//0 is Sig Gen  1 is Rear Panel Mic  2 is Front Panel Mic
	uint32_t mux_addr = MOD_INPUT_SOURCE_ALG0_STAGE0_MONOSWITCHNOSLEW_ADDR;
	input_source = ucontrol->value.integer.value[0];
	switch (input_source) {

	case 0:	{
			buf[0] = MICROBURST_SIGMADSP_FIXPT_ONE;
			buf[1] = MICROBURST_SIGMADSP_FIXPT_ZERO;
			buf[2] = MICROBURST_SIGMADSP_FIXPT_ZERO;
		};
	break;
	case 1:	{
			buf[0] = MICROBURST_SIGMADSP_FIXPT_ZERO;
			buf[1] = MICROBURST_SIGMADSP_FIXPT_ONE;
			buf[2] = MICROBURST_SIGMADSP_FIXPT_ZERO;
		};
	break;
	default: {
			buf[0] = MICROBURST_SIGMADSP_FIXPT_ZERO;
			buf[1] = MICROBURST_SIGMADSP_FIXPT_ZERO;
			buf[2] = MICROBURST_SIGMADSP_FIXPT_ONE;
		};
	break;
	};

	printk (KERN_DEBUG "MB-sigmadsp: input_source setting to %d\n", input_source);
	adau1761_block_write(adau, mux_addr, &buf, 12);

	return 0;
};

static int microburst_sigmadsp_sig_gen_select_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	printk (KERN_DEBUG "MB-sigmadsp: sig_gen_select_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_sig_gen_select_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	printk (KERN_DEBUG "MB-sigmadsp: sig_gen_select_put called\n");
	uint32_t buf[4];
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int sig_gen_select;		//0 is Sine signle or dual  1 is square   2 is triangle   3 is white noise
	uint32_t mux_addr = MOD_SIGGEN_SIG_GEN_SELECT_ALG0_STAGE0_MONOSWITCHNOSLEW_ADDR;
	sig_gen_select = ucontrol->value.integer.value[0];
	switch (sig_gen_select) {

	case 1:	{
			buf[0] = MICROBURST_SIGMADSP_FIXPT_ZERO;
			buf[1] = MICROBURST_SIGMADSP_FIXPT_ONE;
			buf[2] = MICROBURST_SIGMADSP_FIXPT_ZERO;
			buf[3] = MICROBURST_SIGMADSP_FIXPT_ZERO;
		};
	break;
	case 2: {
			buf[0] = MICROBURST_SIGMADSP_FIXPT_ZERO;
			buf[1] = MICROBURST_SIGMADSP_FIXPT_ZERO;
			buf[2] = MICROBURST_SIGMADSP_FIXPT_ONE;
			buf[3] = MICROBURST_SIGMADSP_FIXPT_ZERO;
		};
	break;
	case 3: {
				buf[0] = MICROBURST_SIGMADSP_FIXPT_ZERO;
				buf[1] = MICROBURST_SIGMADSP_FIXPT_ZERO;
				buf[2] = MICROBURST_SIGMADSP_FIXPT_ZERO;
				buf[3] = MICROBURST_SIGMADSP_FIXPT_ONE;
			};
	break;
	default:	{
			buf[0] = MICROBURST_SIGMADSP_FIXPT_ONE;
			buf[1] = MICROBURST_SIGMADSP_FIXPT_ZERO;
			buf[2] = MICROBURST_SIGMADSP_FIXPT_ZERO;
			buf[3] = MICROBURST_SIGMADSP_FIXPT_ZERO;
		};
	break;
	};

	printk (KERN_DEBUG "MB-sigmadsp: sig_gen_select setting to %d\n", sig_gen_select);
	adau1761_block_write(adau, mux_addr, &buf, 16);

	return 0;
};

static int microburst_sigmadsp_monitor_level_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	printk (KERN_DEBUG "MB-sigmadsp: monitor_level_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_monitor_level_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	printk (KERN_DEBUG "MB-sigmadsp: monitor_level_put called\n");
	uint32_t buf;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int monitor_level;		//0 is Sine signle or dual  1 is square   2 is triangle   3 is white noise
	uint32_t mux_addr = MOD_MONITOR_LEVEL_GAIN1940ALGNS2_ADDR;
	monitor_level = ucontrol->value.integer.value[0];
	buf = MICROBURST_SIGMADSP_FIXPT_LEVEL_LOOKUP_64_STEP_MINUS_96_TO_ZERO[monitor_level];
	printk (KERN_DEBUG "MB-sigmadsp: monitor_level setting to %d\n", monitor_level);
	adau1761_block_write(adau, mux_addr, &buf, 4);

	return 0;
};

static int microburst_sigmadsp_sig_gen_level_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	printk (KERN_DEBUG "MB-sigmadsp: sig_gen_level_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_sig_gen_level_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	printk (KERN_DEBUG "MB-sigmadsp: sig_gen_level_put called\n");
	uint32_t buf;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int sig_gen_level;
	uint32_t mux_addr = MOD_SIGGEN_SIG_GEN_LEVEL_GAIN1940ALGNS3_ADDR;
	sig_gen_level = ucontrol->value.integer.value[0];
	buf = MICROBURST_SIGMADSP_FIXPT_LEVEL_LOOKUP_64_STEP_MINUS_96_TO_ZERO[sig_gen_level];
	printk (KERN_DEBUG "MB-sigmadsp: sig_gen_level setting to %d\n", sig_gen_level);
	adau1761_block_write(adau, mux_addr, &buf, 4);

	return 0;
};

static int microburst_sigmadsp_cw_sidetone_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	printk (KERN_DEBUG "MB-sigmadsp: cw_sidetone_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_cw_sidetone_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	printk (KERN_DEBUG "MB-sigmadsp: cw_sidetone_put called\n");
	uint32_t buf[3];
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int sidetone_frequency;
	uint32_t mux_addr = MOD_STATIC_CW_SIDETONE_ALG0_MASK_ADDR;
	sidetone_frequency = ucontrol->value.integer.value[0];
	buf[0] = 0x000000FF;
	buf[1] = (uint32_t)(sidetone_frequency * 699);
	buf[2] = MICROBURST_SIGMADSP_FIXPT_ONE;
	printk (KERN_DEBUG "MB-sigmadsp: sidetone_frequency setting to %d\n", sidetone_frequency);
	adau1761_block_write(adau, mux_addr, &buf, 12);

	return 0;
};

static int microburst_sigmadsp_tx_filter_bw_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	printk (KERN_DEBUG "MB-sigmadsp: tx_filter_bw_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_tx_filter_bw_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	printk (KERN_DEBUG "MB-sigmadsp: tx_filter_bw_put called\n");
	uint32_t *buf_lp;
	uint32_t *buf_hp;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int bandwidth_select;
	uint32_t lp_addr = MOD_TX_FILTER_TX_LPF_ALG0_STAGE0_B2_ADDR;
	uint32_t hp_addr = MOD_TX_FILTER_TX_HPF_ALG0_STAGE0_B2_ADDR;
	bandwidth_select = ucontrol->value.integer.value[0];
	switch (bandwidth_select) {

	case 2:	{
			buf_lp = MICROBURST_SIGMADSP_FIXPT_TX_LPF_10000HZ;
			buf_hp = MICROBURST_SIGMADSP_FIXPT_TX_HPF_60HZ;
			};
	break;
	case 1: {
			buf_lp = MICROBURST_SIGMADSP_FIXPT_TX_LPF_8000HZ;
			buf_hp = MICROBURST_SIGMADSP_FIXPT_TX_HPF_200HZ;
			};
	break;
	default: {
			buf_lp = MICROBURST_SIGMADSP_FIXPT_TX_LPF_2500HZ;
			buf_hp = MICROBURST_SIGMADSP_FIXPT_TX_HPF_500HZ;
			};
	break;
	};

	printk (KERN_DEBUG "MB-sigmadsp: tx_filter_bw setting to %d\n", bandwidth_select);
	adau1761_block_write(adau, lp_addr, buf_lp, 32);
	adau1761_block_write(adau, hp_addr, buf_hp, 32);

	return 0;
};

/* Microburst SigmaDSP kcontrols */

static const struct snd_kcontrol_new microburst_sigmadsp_controls[] = {
		SOC_SINGLE_BOOL_EXT("Microburst SigmaDSP CW Key", 0, microburst_sigmadsp_cw_key_get,
				microburst_sigmadsp_cw_key_put),
		SOC_SINGLE_BOOL_EXT("Microburst SigmaDSP Monitor Voice CW", 1, microburst_sigmadsp_monitor_voice_cw_get,
				microburst_sigmadsp_monitor_voice_cw_put),
		SOC_SINGLE_BOOL_EXT("Microburst SigmaDSP Compander", 1, microburst_sigmadsp_compander_get,
				microburst_sigmadsp_compander_put),
		SOC_SINGLE_BOOL_EXT("Microburst SigmaDSP TX EQ", 0, microburst_sigmadsp_tx_eq_get,
				microburst_sigmadsp_tx_eq_put),
		SOC_SINGLE_BOOL_EXT("Microburst SigmaDSP RX EQ", 0, microburst_sigmadsp_rx_eq_get,
				microburst_sigmadsp_rx_eq_put),
		SOC_SINGLE_BOOL_EXT("Microburst SigmaDSP Meter Select", 0, microburst_sigmadsp_meter_select_get,
				microburst_sigmadsp_meter_select_put),
		SOC_SINGLE_BOOL_EXT("Microburst SigmaDSP Echo Cancel", 0, microburst_sigmadsp_echo_cancel_get,
				microburst_sigmadsp_echo_cancel_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP Input Source", 2, microburst_sigmadsp_input_source_get,
				microburst_sigmadsp_input_source_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP Sig Gen Select", 3, microburst_sigmadsp_sig_gen_select_get,
				microburst_sigmadsp_sig_gen_select_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP Monitor Level", 63, microburst_sigmadsp_monitor_level_get,
				microburst_sigmadsp_monitor_level_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP Sig Gen Level", 63, microburst_sigmadsp_sig_gen_level_get,
				microburst_sigmadsp_sig_gen_level_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP CW Sidetone", 10000, microburst_sigmadsp_cw_sidetone_get,
				microburst_sigmadsp_cw_sidetone_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP TX Filter Bandwidth", 2, microburst_sigmadsp_tx_filter_bw_get,
				microburst_sigmadsp_tx_filter_bw_put),
};

static const DECLARE_TLV_DB_SCALE(adau1761_sing_in_tlv, -1500, 300, 1);
static const DECLARE_TLV_DB_SCALE(adau1761_diff_in_tlv, -1200, 75, 0);
static const DECLARE_TLV_DB_SCALE(adau1761_out_tlv, -5700, 100, 0);
static const DECLARE_TLV_DB_SCALE(adau1761_sidetone_tlv, -1800, 300, 1);
static const DECLARE_TLV_DB_SCALE(adau1761_boost_tlv, -600, 600, 1);
static const DECLARE_TLV_DB_SCALE(adau1761_pga_boost_tlv, -2000, 2000, 1);

static const unsigned int adau1761_bias_select_values[] = {
	0, 2, 3,
};

static const char * const adau1761_bias_select_text[] = {
	"Normal operation", "Enhanced performance", "Power saving",
};

static const char * const adau1761_bias_select_extreme_text[] = {
	"Normal operation", "Extreme power saving", "Enhanced performance",
	"Power saving",
};

static const unsigned int adau1761_mono_out_gain_values[] = {
	0, 1, 2,
};

static const char * const adau1761_mono_out_gain_text[] = {
	"Common Mode", "0 dB Output", "6 dB Output",
};

static const unsigned int adau1761_alc_select_values[] = {
	0, 1, 2, 3, 4,
};

static const char * const adau1761_alc_select_text[] = {
	"Off", "Right Only", "Left Only", "Stereo", "DSP Control",
};

static const unsigned int adau1761_alc_noise_gate_type_values[] = {
	0, 1, 2, 3,
};

static const char * const adau1761_alc_noise_gate_type_text[] = {
	"Hold", "Mute", "Fade Min", "Fade Mute",
};

static const SOC_ENUM_SINGLE_DECL(adau1761_adc_bias_enum,
		ADAU17X1_REC_POWER_MGMT, 3, adau1761_bias_select_extreme_text);
static const SOC_ENUM_SINGLE_DECL(adau1761_hp_bias_enum,
		ADAU17X1_PLAY_POWER_MGMT, 6, adau1761_bias_select_extreme_text);
static const SOC_ENUM_SINGLE_DECL(adau1761_dac_bias_enum,
		ADAU17X1_PLAY_POWER_MGMT, 4, adau1761_bias_select_extreme_text);
static const SOC_VALUE_ENUM_SINGLE_DECL(adau1761_playback_bias_enum,
		ADAU17X1_PLAY_POWER_MGMT, 2, 0x3, adau1761_bias_select_text,
		adau1761_bias_select_values);
static const SOC_VALUE_ENUM_SINGLE_DECL(adau1761_capture_bias_enum,
		ADAU17X1_REC_POWER_MGMT, 1, 0x3, adau1761_bias_select_text,
		adau1761_bias_select_values);

static const SOC_VALUE_ENUM_SINGLE_DECL(adau1761_mono_out_gain_enum,
		ADAU1761_PLAY_MIXER_MONO, 1, 0x3, adau1761_mono_out_gain_text,
		adau1761_mono_out_gain_values);

static const SOC_VALUE_ENUM_SINGLE_DECL(adau1761_alc_select_enum,
		ADAU1761_ALC_CONTROL0, 0, 0x7, adau1761_alc_select_text, 
		adau1761_alc_select_values);
static const SOC_VALUE_ENUM_SINGLE_DECL(adau1761_alc_noise_gate_type_enum,
		ADAU1761_ALC_CONTROL3, 6, 0x3, adau1761_alc_noise_gate_type_text, 
		adau1761_alc_noise_gate_type_values);

static const struct snd_kcontrol_new adau1761_jack_detect_controls[] = {
	SOC_SINGLE("Jack Detect Switch", ADAU1761_DIGMIC_JACKDETECT, 4, 1, 0),
};

static const struct snd_kcontrol_new adau1761_differential_mode_controls[] = {
	SOC_DOUBLE_R_TLV("Capture Volume", ADAU1761_LEFT_DIFF_INPUT_VOL,
		ADAU1761_RIGHT_DIFF_INPUT_VOL, 2, 0x3f, 0, adau1761_diff_in_tlv),
	SOC_DOUBLE_R("Capture Switch", ADAU1761_LEFT_DIFF_INPUT_VOL,
		ADAU1761_RIGHT_DIFF_INPUT_VOL, 1, 1, 0),

	SOC_DOUBLE_R_TLV("PGA Boost Capture Volume", ADAU1761_REC_MIXER_LEFT1,
		ADAU1761_REC_MIXER_RIGHT1, 3, 2, 0, adau1761_pga_boost_tlv),
};

static const struct snd_kcontrol_new adau1761_alc_controls[] = {
	SOC_VALUE_ENUM("ALC Select", adau1761_alc_select_enum),
	SOC_SINGLE("ALC Max Gain", ADAU1761_ALC_CONTROL0, 3, 7, 0),
	SOC_SINGLE("ALC PGA Slew", ADAU1761_ALC_CONTROL0, 6, 3, 0),

	SOC_SINGLE("ALC Target Level", ADAU1761_ALC_CONTROL1, 0, 0xf, 0),
	SOC_SINGLE("ALC Hold Time", ADAU1761_ALC_CONTROL1, 4, 0xf, 0),

	SOC_SINGLE("ALC Decay", ADAU1761_ALC_CONTROL2, 0, 0xf, 0),
	SOC_SINGLE("ALC Attack", ADAU1761_ALC_CONTROL2, 4, 0xf, 0),

	SOC_VALUE_ENUM("ALC Noise Gate Type", adau1761_alc_noise_gate_type_enum),
	SOC_SINGLE("ALC Noise Gate Enable", ADAU1761_ALC_CONTROL3, 5, 1, 0),
	SOC_SINGLE("ALC Noise Gate Threshold",
		ADAU1761_ALC_CONTROL3, 0, 0x1f,0),

};

static const struct snd_kcontrol_new adau1761_single_mode_controls[] = {
	SOC_SINGLE_TLV("Input 1 Capture Volume", ADAU1761_REC_MIXER_LEFT0,
		4, 7, 0, adau1761_sing_in_tlv),
	SOC_SINGLE_TLV("Input 2 Capture Volume", ADAU1761_REC_MIXER_LEFT0,
		1, 7, 0, adau1761_sing_in_tlv),
	SOC_SINGLE_TLV("Input 3 Capture Volume", ADAU1761_REC_MIXER_RIGHT0,
		4, 7, 0, adau1761_sing_in_tlv),
	SOC_SINGLE_TLV("Input 4 Capture Volume", ADAU1761_REC_MIXER_RIGHT0,
		1, 7, 0, adau1761_sing_in_tlv),
};

static const struct snd_kcontrol_new adau1761_controls[] = {
	SOC_DOUBLE_R_TLV("Aux Capture Volume", ADAU1761_REC_MIXER_LEFT1,
		ADAU1761_REC_MIXER_RIGHT1, 0, 7, 0, adau1761_sing_in_tlv),

	SOC_DOUBLE_R_TLV("Headphone Playback Volume", ADAU1761_PLAY_HP_LEFT_VOL,
		ADAU1761_PLAY_HP_RIGHT_VOL, 2, 0x3f, 0, adau1761_out_tlv),
	SOC_DOUBLE_R("Headphone Playback Switch", ADAU1761_PLAY_HP_LEFT_VOL,
		ADAU1761_PLAY_HP_RIGHT_VOL, 1, 1, 0),
	SOC_DOUBLE_R_TLV("Lineout Playback Volume", ADAU1761_PLAY_LINE_LEFT_VOL,
		ADAU1761_PLAY_LINE_RIGHT_VOL, 2, 0x3f, 0, adau1761_out_tlv),
	SOC_DOUBLE_R("Lineout Playback Switch", ADAU1761_PLAY_LINE_LEFT_VOL,
		ADAU1761_PLAY_LINE_RIGHT_VOL, 1, 1, 0),

	SOC_SINGLE("Mic Bias", ADAU17X1_MICBIAS, 0, 1, 0),
	SOC_ENUM("ADC Bias", adau1761_adc_bias_enum),
	SOC_ENUM("DAC Bias", adau1761_dac_bias_enum),
	SOC_VALUE_ENUM("Capture Bias", adau1761_capture_bias_enum),
	SOC_VALUE_ENUM("Playback Bias", adau1761_playback_bias_enum),
	SOC_ENUM("Headphone Bias", adau1761_hp_bias_enum),
};

static const struct snd_kcontrol_new adau1761_mono_controls[] = {
	SOC_SINGLE_TLV("Mono Playback Volume", ADAU1761_PLAY_MONO_OUTPUT_VOL,
		2, 0x3f, 0, adau1761_out_tlv),
	SOC_SINGLE("Mono Playback Switch", ADAU1761_PLAY_MONO_OUTPUT_VOL,
		1, 1, 0),
	SOC_VALUE_ENUM("Mono Playback Gain", adau1761_mono_out_gain_enum),
};

static const struct snd_kcontrol_new adau1761_left_mixer_controls[] = {
	SOC_DAPM_SINGLE("Left DAC Switch", ADAU1761_PLAY_MIXER_LEFT0, 5, 1, 0),
	SOC_DAPM_SINGLE("Right DAC Switch", ADAU1761_PLAY_MIXER_LEFT0, 6, 1, 0),
	SOC_DAPM_SINGLE_TLV("Aux Bypass Volume",
		ADAU1761_PLAY_MIXER_LEFT0, 1, 8, 0, adau1761_sidetone_tlv),
	SOC_DAPM_SINGLE_TLV("Right Bypass Volume",
		ADAU1761_PLAY_MIXER_LEFT1, 4, 8, 0, adau1761_sidetone_tlv),
	SOC_DAPM_SINGLE_TLV("Left Bypass Volume",
		ADAU1761_PLAY_MIXER_LEFT1, 0, 8, 0, adau1761_sidetone_tlv),
};

static const struct snd_kcontrol_new adau1761_right_mixer_controls[] = {
	SOC_DAPM_SINGLE("Left DAC Switch", ADAU1761_PLAY_MIXER_RIGHT0, 5, 1, 0),
	SOC_DAPM_SINGLE("Right DAC Switch", ADAU1761_PLAY_MIXER_RIGHT0, 6, 1, 0),
	SOC_DAPM_SINGLE_TLV("Aux Bypass Volume",
		ADAU1761_PLAY_MIXER_RIGHT0, 1, 8, 0, adau1761_sidetone_tlv),
	SOC_DAPM_SINGLE_TLV("Right Bypass Volume",
		ADAU1761_PLAY_MIXER_RIGHT1, 4, 8, 0, adau1761_sidetone_tlv),
	SOC_DAPM_SINGLE_TLV("Left Bypass Volume",
		ADAU1761_PLAY_MIXER_RIGHT1, 0, 8, 0, adau1761_sidetone_tlv),
};

static const struct snd_kcontrol_new adau1761_left_lr_mixer_controls[] = {
	SOC_DAPM_SINGLE_TLV("Left Volume",
		ADAU1761_PLAY_LR_MIXER_LEFT, 1, 2, 0, adau1761_boost_tlv),
	SOC_DAPM_SINGLE_TLV("Right Volume",
		ADAU1761_PLAY_LR_MIXER_LEFT, 3, 2, 0, adau1761_boost_tlv),
};

static const struct snd_kcontrol_new adau1761_right_lr_mixer_controls[] = {
	SOC_DAPM_SINGLE_TLV("Left Volume",
		ADAU1761_PLAY_LR_MIXER_RIGHT, 1, 2, 0, adau1761_boost_tlv),
	SOC_DAPM_SINGLE_TLV("Right Volume",
		ADAU1761_PLAY_LR_MIXER_RIGHT, 3, 2, 0, adau1761_boost_tlv),
};

static const char * const adau1761_input_mux_text[] = {
	"ADC", "DMIC",
};

static const SOC_ENUM_SINGLE_DECL(adau1761_input_mux_enum,
	ADAU17X1_ADC_CONTROL, 2, adau1761_input_mux_text);

static const struct snd_kcontrol_new adau1761_input_mux_control =
	SOC_DAPM_ENUM("Input Select", adau1761_input_mux_enum);

static int adau1761_dejitter_fixup(struct snd_soc_dapm_widget *w,
	struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = w->codec;
	struct adau *adau = snd_soc_codec_get_drvdata(codec);

	/* After any power changes have been made the dejitter circuit
	 * has to be reinitialized. */
	snd_soc_write(codec, ADAU1761_DEJITTER, 0);
	if (!adau->master)
		snd_soc_write(codec, ADAU1761_DEJITTER, 3);

	return 0;
}

static const struct snd_soc_dapm_widget adau1x61_dapm_widgets[] = {
	SND_SOC_DAPM_MIXER("Left Input Mixer", ADAU1761_REC_MIXER_LEFT0, 0, 0,
		NULL, 0),
	SND_SOC_DAPM_MIXER("Right Input Mixer", ADAU1761_REC_MIXER_RIGHT0, 0, 0,
		NULL, 0),

	SOC_MIXER_ARRAY("Left Playback Mixer", ADAU1761_PLAY_MIXER_LEFT0,
		0, 0, adau1761_left_mixer_controls),
	SOC_MIXER_ARRAY("Right Playback Mixer", ADAU1761_PLAY_MIXER_RIGHT0,
		0, 0, adau1761_right_mixer_controls),
	SOC_MIXER_ARRAY("Left LR Playback Mixer", ADAU1761_PLAY_LR_MIXER_LEFT,
		0, 0, adau1761_left_lr_mixer_controls),
	SOC_MIXER_ARRAY("Right LR Playback Mixer", ADAU1761_PLAY_LR_MIXER_RIGHT,
		0, 0, adau1761_right_lr_mixer_controls),

	SND_SOC_DAPM_SUPPLY("Headphone", ADAU1761_PLAY_HP_LEFT_VOL,
		0, 0, NULL, 0),

	SND_SOC_DAPM_SUPPLY("SYSCLK", SND_SOC_NOPM, 0, 0, NULL, 0),

	SND_SOC_DAPM_POST("Dejitter fixup", adau1761_dejitter_fixup),

	SND_SOC_DAPM_INPUT("LAUX"),
	SND_SOC_DAPM_INPUT("RAUX"),
	SND_SOC_DAPM_INPUT("LINP"),
	SND_SOC_DAPM_INPUT("LINN"),
	SND_SOC_DAPM_INPUT("RINP"),
	SND_SOC_DAPM_INPUT("RINN"),

	SND_SOC_DAPM_OUTPUT("LOUT"),
	SND_SOC_DAPM_OUTPUT("ROUT"),
	SND_SOC_DAPM_OUTPUT("LHP"),
	SND_SOC_DAPM_OUTPUT("RHP"),
};

static const struct snd_soc_dapm_widget adau1761_mono_dapm_widgets[] = {
	SND_SOC_DAPM_MIXER("Mono Playback Mixer", ADAU1761_PLAY_MIXER_MONO,
		0, 0, NULL, 0),

	SND_SOC_DAPM_OUTPUT("MONOOUT"),
};

static const struct snd_soc_dapm_widget adau1761_capless_dapm_widgets[] = {
	SND_SOC_DAPM_SUPPLY("Headphone VGND", ADAU1761_PLAY_MIXER_MONO,
		0, 0, NULL, 0),
};

static const struct snd_soc_dapm_widget adau1761_dmic_widgets[] = {
	SND_SOC_DAPM_MUX("Input Select", SND_SOC_NOPM, 0, 0,
		&adau1761_input_mux_control),

	SND_SOC_DAPM_INPUT("DMIC"),
};

static const struct snd_soc_dapm_route adau1x61_dapm_routes[] = {
	{ "Left Input Mixer", NULL, "LINP" },
	{ "Left Input Mixer", NULL, "LINN" },
	{ "Left Input Mixer", NULL, "LAUX" },

	{ "Right Input Mixer", NULL, "RINP" },
	{ "Right Input Mixer", NULL, "RINN" },
	{ "Right Input Mixer", NULL, "RAUX" },

	{ "Left Playback Mixer", NULL, "Left Playback Enable"},
	{ "Right Playback Mixer", NULL, "Right Playback Enable"},
	{ "Left LR Playback Mixer", NULL, "Left Playback Enable"},
	{ "Right LR Playback Mixer", NULL, "Right Playback Enable"},

	{ "Left Playback Mixer", "Left DAC Switch", "Left DAC" },
	{ "Left Playback Mixer", "Right DAC Switch", "Right DAC" },

	{ "Right Playback Mixer", "Left DAC Switch", "Left DAC" },
	{ "Right Playback Mixer", "Right DAC Switch", "Right DAC" },

	{ "Left LR Playback Mixer", "Left Volume", "Left Playback Mixer" },
	{ "Left LR Playback Mixer", "Right Volume", "Right Playback Mixer" },

	{ "Right LR Playback Mixer", "Left Volume", "Left Playback Mixer" },
	{ "Right LR Playback Mixer", "Right Volume", "Right Playback Mixer" },

	{ "Left ADC", NULL, "Left Input Mixer" },
	{ "Right ADC", NULL, "Right Input Mixer" },

	{ "LHP", NULL, "Left Playback Mixer" },
	{ "RHP", NULL, "Right Playback Mixer" },

	{ "LHP", NULL, "Headphone" },
	{ "RHP", NULL, "Headphone" },

	{ "LOUT", NULL, "Left LR Playback Mixer" },
	{ "ROUT", NULL, "Right LR Playback Mixer" },

	{ "Left Playback Mixer", "Aux Bypass Volume", "LAUX" },
	{ "Left Playback Mixer", "Left Bypass Volume", "Left Input Mixer" },
	{ "Left Playback Mixer", "Right Bypass Volume", "Right Input Mixer" },
	{ "Right Playback Mixer", "Aux Bypass Volume", "RAUX" },
	{ "Right Playback Mixer", "Left Bypass Volume", "Left Input Mixer" },
	{ "Right Playback Mixer", "Right Bypass Volume", "Right Input Mixer" },
};

static const struct snd_soc_dapm_route adau1761_mono_dapm_routes[] = {
	{ "Mono Playback Mixer", NULL, "Left Playback Mixer" },
	{ "Mono Playback Mixer", NULL, "Right Playback Mixer" },

	{ "MONOOUT", NULL, "Mono Playback Mixer" },
};

static const struct snd_soc_dapm_route adau1761_capless_dapm_routes[] = {
	{ "Headphone", NULL, "Headphone VGND" },
};

static const struct snd_soc_dapm_route adau1761_dmic_routes[] = {
	{ "Input Select", "ADC", "Left ADC" },
	{ "Input Select", "ADC", "Right ADC" },
	{ "Input Select", "DMIC", "DMIC" },
	{ "AIFOUT", NULL, "Input Select" },
};

static const struct snd_soc_dapm_route adau1761_no_dmic_routes[] = {
	{ "AIFOUT", NULL, "Left ADC" },
	{ "AIFOUT", NULL, "Right ADC" },
};

static const struct snd_soc_dapm_widget adau1761_dapm_widgets[] = {
	SND_SOC_DAPM_SUPPLY("Serial Port Clock", ADAU1761_CLK_ENABLE0,
		0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("Serial Input Routing Clock", ADAU1761_CLK_ENABLE0,
		1, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("Serial Output Routing Clock", ADAU1761_CLK_ENABLE0,
		3, 0, NULL, 0),

	SND_SOC_DAPM_SUPPLY("Decimator Resync Clock", ADAU1761_CLK_ENABLE0,
		4, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("Interpolator Resync Clock", ADAU1761_CLK_ENABLE0,
		2, 0, NULL, 0),

	SND_SOC_DAPM_SUPPLY("Slew Clock", ADAU1761_CLK_ENABLE0, 6, 0, NULL, 0),

	SND_SOC_DAPM_SUPPLY("ALC Clock", ADAU1761_CLK_ENABLE0, 5, 0, NULL, 0),

	SND_SOC_DAPM_SUPPLY("Digital Clock 0", ADAU1761_CLK_ENABLE1,
		0, 0, NULL, 0),
	SND_SOC_DAPM_SUPPLY("Digital Clock 1", ADAU1761_CLK_ENABLE1,
		1, 0, NULL, 0),
};

static int adau1761_is_slave_mode(struct snd_soc_dapm_widget *source,
	struct snd_soc_dapm_widget *sink)
{
	struct adau *adau = snd_soc_codec_get_drvdata(source->codec);

	return !adau->master;
}

static const struct snd_soc_dapm_route adau1761_dapm_routes[] = {
	{ "Left ADC", NULL, "Digital Clock 0", },
	{ "Right ADC", NULL, "Digital Clock 0", },
	{ "Left DAC", NULL, "Digital Clock 0", },
	{ "Right DAC", NULL, "Digital Clock 0", },

	{ "AIFCLK", NULL, "Digital Clock 1" },

	{ "AIFIN", NULL, "Serial Port Clock" },
	{ "AIFOUT", NULL, "Serial Port Clock" },
	{ "AIFIN", NULL, "Serial Input Routing Clock" },
	{ "AIFOUT", NULL, "Serial Output Routing Clock" },

	{ "AIFOUT", NULL, "ALC Clock" },

	{ "AIFIN", NULL, "Decimator Resync Clock" },
	{ "AIFOUT", NULL, "Interpolator Resync Clock" },

	{ "DSP", NULL, "Decimator Resync Clock" },
	{ "DSP", NULL, "Interpolator Resync Clock" },
	{ "DSP", NULL, "Digital Clock 0" },

	{ "Slew Clock", NULL, "Digital Clock 0" },
	{ "Right Playback Mixer", NULL, "Slew Clock" },
	{ "Left Playback Mixer", NULL, "Slew Clock" },

	{ "Digital Clock 0", NULL, "SYSCLK" },
	{ "Digital Clock 1", NULL, "SYSCLK" },

	{ "AIFOUT", NULL, "Decimator Resync Clock" },
};

static int adau1761_set_bias_level(struct snd_soc_codec *codec,
				 enum snd_soc_bias_level level)
{
	switch (level) {
	case SND_SOC_BIAS_ON:
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		snd_soc_update_bits(codec, ADAU17X1_CLOCK_CONTROL,
			ADAU17X1_CLOCK_CONTROL_SYSCLK_EN,
			ADAU17X1_CLOCK_CONTROL_SYSCLK_EN);
		break;
	case SND_SOC_BIAS_OFF:
		snd_soc_update_bits(codec, ADAU17X1_CLOCK_CONTROL,
			ADAU17X1_CLOCK_CONTROL_SYSCLK_EN, 0);
		break;

	}
	codec->dapm.bias_level = level;
	return 0;
}


static enum adau1761_output_mode adau1761_get_lineout_mode(
	struct snd_soc_codec *codec)
{
	struct adau1761_platform_data *pdata = codec->dev->platform_data;

	if (pdata)
		return pdata->lineout_mode;

	return ADAU1761_OUTPUT_MODE_LINE;
}

static int adau1761_setup_digmic_jackdetect(struct snd_soc_codec *codec)
{
	struct adau1761_platform_data *pdata = codec->dev->platform_data;
	enum adau1761_digmic_jackdet_pin_mode mode;
	unsigned int val = 0;
	int ret;

	if (pdata)
		mode = pdata->digmic_jackdetect_pin_mode;
	else
		mode = ADAU1761_DIGMIC_JACKDET_PIN_MODE_NONE;

	switch (mode) {
	case ADAU1761_DIGMIC_JACKDET_PIN_MODE_JACKDETECT:
		switch (pdata->jackdetect_debounce_time) {
		case ADAU1761_JACKDETECT_DEBOUNCE_5MS:
		case ADAU1761_JACKDETECT_DEBOUNCE_10MS:
		case ADAU1761_JACKDETECT_DEBOUNCE_20MS:
		case ADAU1761_JACKDETECT_DEBOUNCE_40MS:
			val |= pdata->jackdetect_debounce_time << 6;
			break;
		default:
			return -EINVAL;
		}
		if (pdata->jackdetect_active_low)
			val |= ADAU1761_DIGMIC_JACKDETECT_ACTIVE_LOW;

		ret = snd_soc_add_controls(codec,
			adau1761_jack_detect_controls,
			ARRAY_SIZE(adau1761_jack_detect_controls));
		if (ret)
			return ret;
	case ADAU1761_DIGMIC_JACKDET_PIN_MODE_NONE: /* fallthrough */
		ret = snd_soc_dapm_add_routes(&codec->dapm,
			adau1761_no_dmic_routes,
			ARRAY_SIZE(adau1761_no_dmic_routes));
		if (ret)
			return ret;
		break;
	case ADAU1761_DIGMIC_JACKDET_PIN_MODE_DIGMIC:
		ret = snd_soc_dapm_new_controls(&codec->dapm,
			adau1761_dmic_widgets,
			ARRAY_SIZE(adau1761_dmic_widgets));
		if (ret)
			return ret;
		ret = snd_soc_dapm_add_routes(&codec->dapm,
			adau1761_dmic_routes,
			ARRAY_SIZE(adau1761_dmic_routes));
		if (ret)
			return ret;
		val |= ADAU1761_DIGMIC_JACKDETECT_DIGMIC;
		break;
	default:
		return -EINVAL;
	}

	snd_soc_write(codec, ADAU1761_DIGMIC_JACKDETECT, val);

	return 0;
}

static int adau1761_setup_headphone_mode(struct snd_soc_codec *codec)
{
	struct adau1761_platform_data *pdata = codec->dev->platform_data;
	enum adau1761_output_mode mode;
	int ret;

	if (pdata)
		mode = pdata->headphone_mode;
	else
		mode = ADAU1761_OUTPUT_MODE_HEADPHONE;

	switch (mode) {
	case ADAU1761_OUTPUT_MODE_LINE:
		break;
	case ADAU1761_OUTPUT_MODE_HEADPHONE_CAPLESS:
		snd_soc_update_bits(codec, ADAU1761_PLAY_MONO_OUTPUT_VOL, 3, 3);
	case ADAU1761_OUTPUT_MODE_HEADPHONE: /* fallthrough */
		snd_soc_update_bits(codec, ADAU1761_PLAY_HP_RIGHT_VOL, 1, 1);
		snd_soc_add_controls(codec, adau1761_mono_controls,ARRAY_SIZE(adau1761_mono_controls));
		break;
	default:
		return -EINVAL;
	}

	if (mode == ADAU1761_OUTPUT_MODE_HEADPHONE_CAPLESS) {
		ret = snd_soc_dapm_new_controls(&codec->dapm,
			adau1761_capless_dapm_widgets,
			ARRAY_SIZE(adau1761_capless_dapm_widgets));
		if (ret)
			return ret;
		ret = snd_soc_dapm_add_routes(&codec->dapm,
			adau1761_capless_dapm_routes,
			ARRAY_SIZE(adau1761_capless_dapm_routes));
	} else {
		ret = snd_soc_dapm_new_controls(&codec->dapm,
			adau1761_mono_dapm_widgets,
			ARRAY_SIZE(adau1761_mono_dapm_widgets));
		if (ret)
			return ret;
		ret = snd_soc_dapm_add_routes(&codec->dapm,
			adau1761_mono_dapm_routes,
			ARRAY_SIZE(adau1761_mono_dapm_routes));
	}

	return ret;
}

static bool adau1761_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case ADAU1761_DIGMIC_JACKDETECT:
	case ADAU1761_REC_MIXER_LEFT0:
	case ADAU1761_REC_MIXER_LEFT1:
	case ADAU1761_REC_MIXER_RIGHT0:
	case ADAU1761_REC_MIXER_RIGHT1:
	case ADAU1761_LEFT_DIFF_INPUT_VOL:
	case ADAU1761_RIGHT_DIFF_INPUT_VOL:
	case ADAU1761_ALC_CONTROL0:
	case ADAU1761_ALC_CONTROL1:
	case ADAU1761_ALC_CONTROL2:
	case ADAU1761_ALC_CONTROL3:
	case ADAU1761_PLAY_LR_MIXER_LEFT:
	case ADAU1761_PLAY_MIXER_LEFT0:
	case ADAU1761_PLAY_MIXER_LEFT1:
	case ADAU1761_PLAY_MIXER_RIGHT0:
	case ADAU1761_PLAY_MIXER_RIGHT1:
	case ADAU1761_PLAY_LR_MIXER_RIGHT:
	case ADAU1761_PLAY_MIXER_MONO:
	case ADAU1761_PLAY_HP_LEFT_VOL:
	case ADAU1761_PLAY_HP_RIGHT_VOL:
	case ADAU1761_PLAY_LINE_LEFT_VOL:
	case ADAU1761_PLAY_LINE_RIGHT_VOL:
	case ADAU1761_PLAY_MONO_OUTPUT_VOL:
	case ADAU1761_POP_CLICK_SUPPRESS:
	case ADAU1761_JACK_DETECT_PIN:
	case ADAU1761_DEJITTER:
	case ADAU1761_CLK_ENABLE0:
	case ADAU1761_CLK_ENABLE1:
		return true;
	default:
		break;
	}

	return adau17x1_readable_register(dev, reg);
}

static int adau1761_probe(struct snd_soc_codec *codec)
{
	struct adau1761_platform_data *pdata = codec->dev->platform_data;
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int ret;
	ret = adau17x1_probe(codec);
	if (ret < 0)
		return ret;

	adau1761_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	if (pdata && pdata->input_differential) {
		snd_soc_update_bits(codec, ADAU1761_LEFT_DIFF_INPUT_VOL,
			ADAU1761_DIFF_INPUT_VOL_LDEN,
			ADAU1761_DIFF_INPUT_VOL_LDEN);
		snd_soc_update_bits(codec, ADAU1761_RIGHT_DIFF_INPUT_VOL,
			ADAU1761_DIFF_INPUT_VOL_LDEN,
			ADAU1761_DIFF_INPUT_VOL_LDEN);
		ret = snd_soc_add_controls(codec,
			adau1761_differential_mode_controls,
			ARRAY_SIZE(adau1761_differential_mode_controls));
		if (ret)
			return ret;
		ret = snd_soc_add_controls(codec,
			adau1761_alc_controls,
			ARRAY_SIZE(adau1761_alc_controls));
		if (ret)
			return ret;
		} 
		else {
		ret = snd_soc_add_controls(codec,
			adau1761_single_mode_controls,
			ARRAY_SIZE(adau1761_single_mode_controls));
		if (ret)
			return ret;
	}

	switch (adau1761_get_lineout_mode(codec)) {
	case ADAU1761_OUTPUT_MODE_LINE:
		break;
	case ADAU1761_OUTPUT_MODE_HEADPHONE:
		snd_soc_update_bits(codec, ADAU1761_PLAY_LINE_LEFT_VOL, 1, 1);
		snd_soc_update_bits(codec, ADAU1761_PLAY_LINE_RIGHT_VOL, 1, 1);
		break;
	default:
		return -EINVAL;
	}

	ret = adau1761_setup_headphone_mode(codec);
	if (ret)
		return ret;

	ret = adau1761_setup_digmic_jackdetect(codec);
	if (ret)
		return ret;

	if (adau->type == ADAU1761) {
		ret = snd_soc_dapm_new_controls(&codec->dapm,
			adau1761_dapm_widgets,
			ARRAY_SIZE(adau1761_dapm_widgets));
		if (ret)
			return ret;

		ret = snd_soc_dapm_add_routes(&codec->dapm,
			adau1761_dapm_routes,
			ARRAY_SIZE(adau1761_dapm_routes));
		if (ret)
			return ret;

		ret = adau17x1_load_firmware(codec, ADAU1761_FIRMWARE);
		if (ret) { 
			dev_warn(codec->dev, "Failed to load SigmaDSP firmware\n");
	}
		else {
			ret = snd_soc_add_controls(codec, microburst_sigmadsp_controls, ARRAY_SIZE(microburst_sigmadsp_controls));
			printk("MB-codecdsp: adding kcontrols for dsp module\n");

		if (ret)
			return ret;
		}
	}
	return 0;
}

static struct snd_soc_codec_driver adau1761_codec_driver = {
	.probe			= adau1761_probe,
	.remove			= adau17x1_remove,
	.suspend		= adau17x1_suspend,
	.resume			= adau17x1_resume,
	.set_bias_level		= adau1761_set_bias_level,

	.controls		= adau1761_controls,
	.num_controls		= ARRAY_SIZE(adau1761_controls),
	.dapm_widgets		= adau1x61_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(adau1x61_dapm_widgets),
	.dapm_routes		= adau1x61_dapm_routes,
	.num_dapm_routes	= ARRAY_SIZE(adau1x61_dapm_routes),
};

#define ADAU1761_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE | \
	SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver adau1361_dai_driver = {
	.name = "adau-hifi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 4,
		.rates = SNDRV_PCM_RATE_8000_96000,
		.formats = ADAU1761_FORMATS,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 2,
		.channels_max = 4,
		.rates = SNDRV_PCM_RATE_8000_96000,
		.formats = ADAU1761_FORMATS,
	},
	.ops = &adau17x1_dai_ops,
};

static struct snd_soc_dai_driver adau1761_dai_driver = {
	.name = "adau-hifi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 8,
		.rates = SNDRV_PCM_RATE_8000_96000,
		.formats = ADAU1761_FORMATS,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 2,
		.channels_max = 8,
		.rates = SNDRV_PCM_RATE_8000_96000,
		.formats = ADAU1761_FORMATS,
	},
	.ops = &adau17x1_dai_ops,
};

#if defined(CONFIG_SPI_MASTER)

static const struct regmap_config adau1761_spi_regmap_config = {
	.val_bits		= 8,
	.reg_bits		= 24,
	.read_flag_mask		= 0x01,
	.max_register		= 0x40fa,
	.reg_defaults		= adau1761_reg_defaults,
	.num_reg_defaults	= ARRAY_SIZE(adau1761_reg_defaults),
	.readable_reg		= adau1761_readable_register,
	.volatile_reg		= adau17x1_volatile_register,
	.cache_type			= REGCACHE_RBTREE,
};

static int __devinit adau1761_spi_probe(struct spi_device *spi)
{
	enum adau17x1_type type = spi_get_device_id(spi)->driver_data;
	struct snd_soc_dai_driver *dai_drv;
	struct regmap *regmap;
	int ret;

	regmap = regmap_init_spi(spi, &adau1761_spi_regmap_config);

	ret = adau17x1_bus_probe(&spi->dev, regmap, type, SND_SOC_SPI);
	if (ret)
		return ret;

	if (type == ADAU1361)
		dai_drv = &adau1361_dai_driver;
	else
		dai_drv = &adau1761_dai_driver;

	ret = snd_soc_register_codec(&spi->dev, &adau1761_codec_driver,
			dai_drv, 1);

	if (ret)
		goto err_remove;

	return 0;

err_remove:
	adau17x1_bus_remove(&spi->dev);
	return ret;
}

static int __devexit adau1761_spi_remove(struct spi_device *spi)
{
	snd_soc_unregister_codec(&spi->dev);
	adau17x1_bus_remove(&spi->dev);
	return 0;
}

static const struct spi_device_id adau1761_spi_id[] = {
	{ "adau1361", ADAU1361 },
	{ "adau1461", ADAU1761 },
	{ "adau1761", ADAU1761 },
	{ "adau1961", ADAU1361 },
	{ }
};
MODULE_DEVICE_TABLE(spi, adau1761_spi_id);

static struct spi_driver adau1761_spi_driver = {
	.driver = {
		.name	= "adau1761",
		.owner	= THIS_MODULE,
	},
	.probe		= adau1761_spi_probe,
	.remove		= __devexit_p(adau1761_spi_remove),
	.id_table	= adau1761_spi_id,
};
#endif

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)

static const struct regmap_config adau1761_i2c_regmap_config = {
	.val_bits		= 8,
	.reg_bits		= 16,
	.max_register		= 0x40fa,
	.reg_defaults		= adau1761_reg_defaults,
	.num_reg_defaults	= ARRAY_SIZE(adau1761_reg_defaults),
	.readable_reg		= adau1761_readable_register,
	.volatile_reg		= adau17x1_volatile_register,
	.cache_type			= REGCACHE_RBTREE,
};

static int __devinit adau1761_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	enum adau17x1_type type = id->driver_data;
	struct snd_soc_dai_driver *dai_drv;
	struct regmap *regmap;
	int ret;

	regmap = regmap_init_i2c(client, &adau1761_i2c_regmap_config);

	ret = adau17x1_bus_probe(&client->dev, regmap, type, SND_SOC_I2C);
	if (ret)
		return ret;

	if (type == ADAU1361)
		dai_drv = &adau1361_dai_driver;
	else
		dai_drv = &adau1761_dai_driver;

	ret = snd_soc_register_codec(&client->dev, &adau1761_codec_driver,
			dai_drv, 1);

	if (ret)
		goto err_remove;

	return 0;

err_remove:
	adau17x1_bus_remove(&client->dev);
	return ret;
}

static int __devexit adau1761_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	adau17x1_bus_remove(&client->dev);
	return 0;
}

static const struct i2c_device_id adau1761_i2c_id[] = {
	{ "adau1361", ADAU1361 },
	{ "adau1461", ADAU1761 },
	{ "adau1761", ADAU1761 },
	{ "adau1961", ADAU1361 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, adau1761_i2c_id);

static struct i2c_driver adau1761_i2c_driver = {
	.driver = {
		.name = "adau1761",
		.owner = THIS_MODULE,
	},
	.probe = adau1761_i2c_probe,
	.remove = __devexit_p(adau1761_i2c_remove),
	.id_table = adau1761_i2c_id,
};
#endif

static int __init adau1761_init(void)
{
	int ret = 0;

#if defined(CONFIG_SPI_MASTER)
	ret = spi_register_driver(&adau1761_spi_driver);
	if (ret)
		return ret;
#endif

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	ret = i2c_add_driver(&adau1761_i2c_driver);
#endif

	return ret;
}
module_init(adau1761_init);

static void __exit adau1761_exit(void)
{
#if defined(CONFIG_SPI_MASTER)
	spi_unregister_driver(&adau1761_spi_driver);
#endif

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	i2c_del_driver(&adau1761_i2c_driver);
#endif
}
module_exit(adau1761_exit);

MODULE_DESCRIPTION("ASoC ADAU1361/ADAU1461/ADAU1761/ADAU1961 CODEC driver");
MODULE_AUTHOR("Lars-Peter Clausen <lars@metafoo.de>");
MODULE_LICENSE("GPL");
