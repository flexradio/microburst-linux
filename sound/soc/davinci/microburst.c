/*
 * Machine Driver for Microburst board, adapted from:
 *
 * ASoC driver for TI DAVINCI EVM platform
 *
 * Author:      Vladimir Barinov, <vbarinov@embeddedalley.com>
 * Copyright:   (C) 2007 MontaVista Software, Inc., <source@mvista.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/* Platform specific information for Microburst board
 *
 * Adapted for Microburst board for FlexRadio Systems
 * by: Steve Conklin and Jim Reese
 * 
 * The ADAU1761 has device specific parameters that are loaded when
 * the i2c device gets registered with the kernel.  This is done in
 * the file arch/arm/mach-omap2/board-ti8168evm.c
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
//#include <sound/soc-dapm.h>
#include <sound/adau17x1.h>

#include <asm/dma.h>
#include <asm/mach-types.h>

#ifndef CONFIG_ARCH_TI81XX
#include <mach/asp.h>
#include <mach/edma.h>
#include <mach/mux.h>
#else
#include <plat/asp.h>
#include <asm/hardware/edma.h>
#endif

#include "../codecs/adau17x1.h"
#include "davinci-pcm.h"
#include "davinci-i2s.h"
#include "davinci-mcasp.h"

#define CODEC_AUDIO_FORMAT (SND_SOC_DAIFMT_LEFT_J | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM)
#define CPU_AUDIO_FORMAT (SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM)

static int microburst_hw_params(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;
	unsigned sysclk;

  /* This is the MCLK clock coming in directly to the ADAU1761 */
  sysclk = 24576000;

	/* set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, CODEC_AUDIO_FORMAT);
	if (ret < 0)
		return ret;

	/* set cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, CPU_AUDIO_FORMAT);
	if (ret < 0)
		return ret;

  /* Set up direct clock from MCLK */
  ret = snd_soc_dai_set_sysclk(codec_dai, ADAU17X1_CLK_SRC_MCLK, sysclk,
                               SND_SOC_CLOCK_IN);
  if (ret < 0)
    return ret;
	/* Set up for clock input on PLL */
  //	ret = snd_soc_dai_set_sysclk(codec_dai, ADAU17X1_CLK_SRC_PLL, sysclk,
  //			SND_SOC_CLOCK_IN);
  //	if (ret < 0)
  //		return ret;
  /* Not using the PLL */
  //  ret = snd_soc_dai_set_pll(codec_dai, 0, 0, sysclk, 49152000);

	/* Set clock divider, div_id (0) argument ignored */
  //  ret = snd_soc_dai_set_clkdiv(codec_dai, 0, 4);	//This was originally codec_dai, 0, 1

  /* Set clock divider, div_id (0) argument ignored.
     Internal clock freq is 48kHz so we want need to set divider is = MCLK / fs = 512 = 0x1
  */
  ret = snd_soc_dai_set_clkdiv(codec_dai, 0, 2);

	if (ret < 0)
		return ret;

	/* set TDM slot configuration */
        //	ret = snd_soc_dai_set_tdm_slot(codec_dai, 0x03, 0x03, 2, 32);
        // 		return ret;


	return 0;
}

static struct snd_soc_ops microburst_ops = {
	.hw_params = microburst_hw_params,
};

/* microburst dapm widgets */
static const struct snd_soc_dapm_widget microburst_dapm_widgets[] = {
	SND_SOC_DAPM_LINE("MB-Rear-Mic", NULL),
	SND_SOC_DAPM_LINE("MB-Front-Mic", NULL),
	SND_SOC_DAPM_LINE("MB-AuxIn", NULL),

	SND_SOC_DAPM_LINE("MB-AuxOut L", NULL),
	SND_SOC_DAPM_LINE("MB-AuxOut R", NULL),
	SND_SOC_DAPM_LINE("MB-PwrSpk", NULL),
	SND_SOC_DAPM_HP("MB-Headphones", NULL),
	SND_SOC_DAPM_LINE("MB-Speaker", NULL),
};

/* microburst audio_mapnections to the codec */
static const struct snd_soc_dapm_route microburst_audio_map[] = {
	{ "LAUX", NULL, "MB-AuxIn" },
	{ "RAUX", NULL, "MB-AuxIn" },
	{ "LINP", NULL, "MB-Rear-Mic" },
	{ "RINP", NULL, "MB-Front-Mic" },

	{ "MB-Rear-Mic", NULL, "MICBIAS" },
	{ "MB-Front-Mic", NULL, "MICBIAS" },

	{ "MB-AuxOut L", NULL, "LOUT" },
	{ "MB-AuxOut R", NULL, "ROUT" },
	{ "MB-PwrSpk", NULL, "LOUT" },
	{ "MB-PwrSpk", NULL, "ROUT" },
	{ "MB-Headphones", NULL, "LHP" },
	{ "MB-Headphones", NULL, "RHP" },
	{ "MB-Speaker", NULL, "MONOOUT" },
};

/* Logic for the microburst adau1761 as connected on a davinci-evm */
static int evm_adau1761_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;


	/* Add davinci-evm specific widgets */
//	snd_soc_dapm_new_controls(dapm, microburst_dapm_widgets, ARRAY_SIZE(microburst_dapm_widgets));


	/* Set up davinci-evm specific audio path audio_map */
//	snd_soc_dapm_add_routes(dapm, microburst_audio_map, ARRAY_SIZE(microburst_audio_map));

//	snd_soc_dapm_enable_pin(dapm, "Mono Playback Mixer");
//	snd_soc_dapm_enable_pin(dapm, "LINN");
//	snd_soc_dapm_enable_pin(dapm, "RINP");
//	snd_soc_dapm_enable_pin(dapm, "RINN");
//	snd_soc_dapm_enable_pin(dapm, "LAUX");
//	snd_soc_dapm_enable_pin(dapm, "RAUX");
//	snd_soc_dapm_enable_pin(dapm, "HP Out");
//	snd_soc_dapm_enable_pin(dapm, "Stereo Out");
//	snd_soc_dapm_enable_pin(dapm, "HPLCOM");
//	snd_soc_dapm_enable_pin(dapm, "HPRCOM");
//	snd_soc_dapm_enable_pin(dapm, "Mono Out");


	return 0;
}

/* davinci-evm digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link ti81xx_evm_dai[] = {
	{
		.name = "adau1x61-de",
		.stream_name = "adau1x61-de",
		.cpu_dai_name = "davinci-mcasp.0",
		.codec_dai_name = "adau-hifi-de",
		.platform_name = "davinci-pcm-audio",
		.codec_name = "adau1761.1-0038",
		.init = evm_adau1761_init,
		.ops = &microburst_ops,
	},
  {
		.name = "adau1x61-mb",
		.stream_name = "adau1x61-mb",
		.cpu_dai_name = "davinci-mcasp.2",
		.codec_dai_name = "adau-hifi-mb",
		.platform_name = "davinci-pcm-audio",
		.codec_name = "adau1761.1-0038",
		.init = evm_adau1761_init,
		.ops = &microburst_ops,
	}
};

static struct snd_soc_card microburst_snd_soc_card = {
	.name = "TI81XX EVM",
	.dai_link = ti81xx_evm_dai,
	.num_links = ARRAY_SIZE(ti81xx_evm_dai),
};

static struct platform_device *evm_snd_device;
static int __init evm_init(void)
{
	struct snd_soc_card *evm_snd_dev_data;
	int index;
	int ret;

	evm_snd_dev_data = &microburst_snd_soc_card;
	index = 0;

	evm_snd_device = platform_device_alloc("soc-audio", index);
	if (!evm_snd_device)
		return -ENOMEM;

	platform_set_drvdata(evm_snd_device, evm_snd_dev_data);
	ret = platform_device_add(evm_snd_device);
	if (ret)
		platform_device_put(evm_snd_device);

	return ret;
}

static void __exit evm_exit(void)
{
	platform_device_unregister(evm_snd_device);
}

module_init(evm_init);
module_exit(evm_exit);

MODULE_AUTHOR("Steve Conklin");
MODULE_DESCRIPTION("Microburst ASoC driver");
MODULE_LICENSE("GPL");
