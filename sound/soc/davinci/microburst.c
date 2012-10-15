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

#define AUDIO_FORMAT (SND_SOC_DAIFMT_LEFT_J |			\
		SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBM_CFM)

static int microburst_hw_params(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;
	unsigned sysclk;

	sysclk = 24576000; // 98304000

	/* set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, AUDIO_FORMAT);
	if (ret < 0)
		return ret;

	/* set cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, AUDIO_FORMAT);
	if (ret < 0)
		return ret;

	/* Set up for clock input on MCLK */
	ret = snd_soc_dai_set_sysclk(codec_dai, ADAU17X1_CLK_SRC_MCLK, sysclk,
			SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	/* Set clock divider, div_id (0) argument ignored */
	ret = snd_soc_dai_set_clkdiv(codec_dai, 0, 1);
	if (ret < 0)
		return ret;

	return 0;
}

static struct snd_soc_ops microburst_ops = {
	.hw_params = microburst_hw_params,
};

/* davinci-evm machine dapm widgets */
static const struct snd_soc_dapm_widget adau_dapm_widgets[] = {
	SND_SOC_DAPM_LINE("In 1", NULL),
	SND_SOC_DAPM_LINE("In 2", NULL),
	SND_SOC_DAPM_LINE("In 3-4", NULL),

	SND_SOC_DAPM_LINE("Diff Out L", NULL),
	SND_SOC_DAPM_LINE("Diff Out R", NULL),
	SND_SOC_DAPM_LINE("Stereo Out", NULL),
	SND_SOC_DAPM_HP("Capless HP Out", NULL),
};

/* davinci-evm machine audio_mapnections to the codec pins */
static const struct snd_soc_dapm_route audio_map[] = {
	{ "LAUX", NULL, "In 3-4" },
	{ "RAUX", NULL, "In 3-4" },
	{ "LINP", NULL, "In 1" },
	{ "LINN", NULL, "In 1"},
	{ "RINP", NULL, "In 2" },
	{ "RINN", NULL, "In 2" },

	{ "In 1", NULL, "MICBIAS" },
	{ "In 2", NULL, "MICBIAS" },

	{ "Capless HP Out", NULL, "LHP" },
	{ "Capless HP Out", NULL, "RHP" },
	{ "Diff Out L", NULL, "LOUT" },
	{ "Diff Out R", NULL, "ROUT" },
	{ "Stereo Out", NULL, "LOUT" },
	{ "Stereo Out", NULL, "ROUT" },
};

/* Logic for the microburst adau1761 as connected on a davinci-evm */
static int evm_adau1761_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

	/* Add davinci-evm specific widgets */
	snd_soc_dapm_new_controls(dapm, adau_dapm_widgets,
				  ARRAY_SIZE(adau_dapm_widgets));

	/* Set up davinci-evm specific audio path audio_map */
	snd_soc_dapm_add_routes(dapm, audio_map, ARRAY_SIZE(audio_map));

	/* not connected - these 3 lines left over from TI EVB code */
	// _soc_dapm_disable_pin(dapm, "MONO_LOUT");
	// _soc_dapm_disable_pin(dapm, "HPLCOM");
	// _soc_dapm_disable_pin(dapm, "HPRCOM");

	/* always connected */
	//{ "LAUX", NULL, "In 3-4" },            // Signal ACC_LINE_IN
	//{ "RAUX", NULL, "In 3-4" },            // Signal CM
	//{ "LINP", NULL, "In 1" },              // Balanced Mic Pos
	//{ "LINN", NULL, "In 1"},               // Balanced Mic Neg
	//{ "RINP", NULL, "In 2" },              // Mic Pos (biased)
	//{ "RINN", NULL, "In 2" },              // Mic Neg

	//{ "In 1", NULL, "MICBIAS" },           // Mic Bias
	//{ "In 2", NULL, "MICBIAS" },           // Mic Bias

	//{ "Capless HP Out", NULL, "LHP" },     // Headphone Left
	//{ "Capless HP Out", NULL, "RHP" },     // Headphone Right
	//{ "Diff Out L", NULL, "LOUT" },        // 
	//{ "Diff Out R", NULL, "ROUT" },        // 
	//{ "Stereo Out", NULL, "LOUT" },        // Ext Speaker
	//{ "Stereo Out", NULL, "ROUT" },        // Ext Speaker

	snd_soc_dapm_enable_pin(dapm, "LINP");
	snd_soc_dapm_enable_pin(dapm, "LINN");
	snd_soc_dapm_enable_pin(dapm, "LAUX");
	snd_soc_dapm_enable_pin(dapm, "RAUX");
	snd_soc_dapm_enable_pin(dapm, "Capless HP Out");
	snd_soc_dapm_enable_pin(dapm, "Stereo Out");

	return 0;
}

/* davinci-evm digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link ti81xx_evm_dai[] = {
	{
		.name = "adau1x61",
		.stream_name = "adau1x61",
		.cpu_dai_name = "davinci-mcasp.2",
		.codec_dai_name = "adau-hifi",
		.platform_name = "davinci-pcm-audio",
		.codec_name = "adau1761.1-0038",
		.init = evm_adau1761_init,
		.ops = &microburst_ops,
	},
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
