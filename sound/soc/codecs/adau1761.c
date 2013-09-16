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


#define CODEC_MODULE_VERSION      1000

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

#define ADAU1761_SAFELOAD_DATA(x)  (0x0001 + (x))
#define ADAU1761_SAFELOAD_ADDR		0x0006
#define ADAU1761_SAFELOAD_SIZE		0x0007

/********************************************/

static struct reg_default adau1761_reg_defaults[] = {
	{ ADAU1761_DEJITTER,			0x00 },
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
	{ ADAU17X1_CONVERTER0,			0x00 },
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
	{ ADAU17X1_SERIAL_INPUT_ROUTE,		0x00 },
	{ ADAU17X1_SERIAL_OUTPUT_ROUTE,		0x00 },
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

uint32_t MICROBURST_SIGMADSP_FIXPT_TX_LPF_10000HZ[6] = { 0x0068464F, 0x00D08C9F, 0x0068464F, 0xFFA87724, 0xFF319B32,
                                                         //                                                         		0x000002B8,
                                                         //                                                         		0x0000028C,
                                                         0x00000001};
//uint32_t MICROBURST_SIGMADSP_FIXPT_TX_LPF_10000HZ[8] = { 0x0068464F, 0x00D08C9F, 0x0068464F, 0xFFA87724, 0xFF319B32,
//		0x0000028C, 0x000002B8, 0x00000001 };
uint32_t MICROBURST_SIGMADSP_FIXPT_TX_LPF_8000HZ[6] = { 0x0053954A, 0x00A72A95, 0x0053954A, 0xFFC5E3AA, 0xFF67E819,
                                                        //		0x000002B8,
                                                        //		0x0000028C,
 0x00000001 };
uint32_t MICROBURST_SIGMADSP_FIXPT_TX_LPF_4000HZ[6] = { 0x00283B63, 0x005076C5, 0x00283B63, 0xFFE4EBF3, 0xFFF84977,
                                                        //		0x000002B8,
                                                        //		0x0000028C,
 0x00000001 };
uint32_t MICROBURST_SIGMADSP_FIXPT_TX_LPF_3000HZ[6] = { 0x001C3366, 0x003866CC, 0x001C3366, 0xFFE29A52, 0x002B49B3,
                                                        //	0x000002B8,
                                                        //	0x0000028C, 
0x00000001 };
uint32_t MICROBURST_SIGMADSP_FIXPT_TX_LPF_2500HZ[6] = { 0x001617AF, 0x002C2F5F, 0x001616AF, 0xFFDE41F5, 0x00485958,
                                                        //	0x000002B8,
                                                        //	0x0000028C, 
0x00000001 };
uint32_t MICROBURST_SIGMADSP_FIXPT_TX_LPF_2000HZ[6] = { 0x001011B5, 0x0020236A, 0x001011B5, 0xFFD6DAD1, 0x00681FD1,
                                                        //	0x000002B8,
                                                        //	0x0000028C,
 0x00000001 };

/* TX_FILTER High Pass Parameters */

uint32_t MICROBURST_SIGMADSP_FIXPT_TX_HPF_60HZ[6] = { 0x007DD334, 0xFF045997, 0x007DD334, 0xFF816E67, 0x00FE8F2C,
                                                      //		0x000002C2,
                                                      //		0x00000294, 
0x00000001 };
uint32_t MICROBURST_SIGMADSP_FIXPT_TX_HPF_100HZ[6] = { 0x007D5A87, 0xFF054AF2, 0x007D5A87, 0xFF826068, 0x00FD98E1,
                                                       //		0x000002C2,
                                                       //		0x00000294, 
0x00000001 };
//uint32_t MICROBURST_SIGMADSP_FIXPT_TX_HPF_100HZ[8] = { 0x007D5A87, 0xFF054AF2, 0x007D5A87, 0xFF826068, 0x00FD98E1,
//		0x00000294, 0x000002C2, 0x00000001 };
uint32_t MICROBURST_SIGMADSP_FIXPT_TX_HPF_150HZ[6] = { 0x007CC431, 0xFF06779E, 0x007CC431, 0xFF838C66, 0x00FC6490,
                                                       //   0x00000294,
                                                       //   0x000002C2,
 0x00000001 };
uint32_t MICROBURST_SIGMADSP_FIXPT_TX_HPF_200HZ[6] = { 0x007C2E69, 0xFF07A32D, 0x007C2E69, 0xFF84B5A3, 0x00FB2FBF,
                                                       //		0x000002C2,
                                                       //		0x00000294,
 0x00000001 };
uint32_t MICROBURST_SIGMADSP_FIXPT_TX_HPF_400HZ[6] = { 0x0079DCAC, 0xFF0C46A7, 0x0079DCAC, 0xFF893FDD, 0x00F65785,
                                                       //	0x000002C2,
                                                       //	0x00000294, 
0x00000001 };
uint32_t MICROBURST_SIGMADSP_FIXPT_TX_HPF_450HZ[6] = { 0x0079498A, 0xFF0D6CEC, 0x0079498A, 0xFF8A5BED, 0x00F5203A,
                                                       //	0x000002C2,
                                                       //	0x00000294,
 0x00000001 };
uint32_t MICROBURST_SIGMADSP_FIXPT_TX_HPF_500HZ[6] = { 0x0078B6E8, 0xFF0E9231, 0x0078B6E8, 0xFF8B7578, 0x00F3E871,
                                                       //	0x000002C2,
                                                       //	0x00000294,
 0x00000001 };

/* EQ_PANEL Filter Parameters */

uint32_t MICROBURST_SIGMADSP_EQ_PANEL_STAGE_0_FIXPT_BOOST[21][5] = {
 {0x007D970C, 0xFF031CA8, 0x007F513F, 0xFF83125A, 0x00FCDDFD, },
 {0x007D9668, 0xFF030D1F, 0x007F61B9, 0xFF83031B, 0x00FCEE1C, },
 {0x007D95FB, 0xFF02FD0C, 0x007F7288, 0xFF82F349, 0x00FCFEC0, },
 {0x007D95B9, 0xFF02EC8A, 0x007F83A1, 0xFF82E300, 0x00FD0FD0, },
 {0x007D9597, 0xFF02DBB0, 0x007F94F7, 0xFF82D257, 0x00FD2135, },
 {0x007D958B, 0xFF02CA91, 0x007FA681, 0xFF82C161, 0x00FD32DD, },
 {0x007D958F, 0xFF02B93E, 0x007FB835, 0xFF82B030, 0x00FD44B6, },
 {0x007D959D, 0xFF02A7C6, 0x007FCA09, 0xFF829ED2, 0x00FD56B2, },
 {0x007D95B0, 0xFF029637, 0x007FDBF7, 0xFF828D54, 0x00FD68C5, },
 {0x007D95C4, 0xFF02849A, 0x007FEDF7, 0xFF827BC3, 0x00FD7AE4, },
 {0x007D95DA, 0xFF0272FA, 0x00800000, 0xFF826A26, 0x00FD8D06, },
 {0x007D95F0, 0xFF026160, 0x0080120C, 0xFF825887, 0x00FD9F23, },
 {0x007D9606, 0xFF024FD1, 0x00802413, 0xFF8246EC, 0x00FDB134, },
 {0x007D9620, 0xFF023E53, 0x0080360D, 0xFF82355B, 0x00FDC336, },
 {0x007D9641, 0xFF022CEC, 0x008047F4, 0xFF8223D9, 0x00FDD522, },
 {0x007D966E, 0xFF021B9F, 0x008059BE, 0xFF821269, 0x00FDE6F6, },
 {0x007D96AD, 0xFF020A6E, 0x00806B63, 0xFF82010E, 0x00FDF8AF, },
 {0x007D9707, 0xFF01F95D, 0x00807CD8, 0xFF81EFCA, 0x00FE0A4C, },
 {0x007D9785, 0xFF01E86C, 0x00808E15, 0xFF81DE9F, 0x00FE1BCC, },
 {0x007D9833, 0xFF01D79D, 0x00809F0C, 0xFF81CD8C, 0x00FE2D2E, },
 {0x007D991F, 0xFF01C6EE, 0x0080AFB1, 0xFF81BC92, 0x00FE3E74, }, };
uint32_t MICROBURST_SIGMADSP_EQ_PANEL_STAGE_1_FIXPT_BOOST[21][5] = {
 {0x007CCB6D, 0xFF050148, 0x007E55B7, 0xFF84DEDC, 0x00FAFEB8, },
 {0x007CE1C4, 0xFF04BCD3, 0x007E83DE, 0xFF849A5E, 0x00FB432D, },
 {0x007CF58E, 0xFF047C10, 0x007EB0E0, 0xFF845992, 0x00FB83F0, },
 {0x007D06D8, 0xFF043ECD, 0x007EDCE1, 0xFF841C47, 0x00FBC133, },
 {0x007D15AE, 0xFF0404DD, 0x007F0803, 0xFF83E24E, 0x00FBFB23, },
 {0x007D221A, 0xFF03CE12, 0x007F326A, 0xFF83AB7C, 0x00FC31EE, },
 {0x007D2C26, 0xFF039A41, 0x007F5C36, 0xFF8377A4, 0x00FC65BF, },
 {0x007D33D7, 0xFF036943, 0x007F858A, 0xFF83469F, 0x00FC96BD, },
 {0x007D3932, 0xFF033AF1, 0x007FAE87, 0xFF831847, 0x00FCC50F, },
 {0x007D3C3C, 0xFF030F27, 0x007FD74D, 0xFF82EC77, 0x00FCF0D9, },
 {0x00000000, 0x00000000, 0x00800000, 0x00000000, 0x00000000, },
 {0x007D3B5A, 0xFF02BEA1, 0x008028BF, 0xFF829BE6, 0x00FD415F, },
 {0x007D376D, 0xFF0299A6, 0x008051AD, 0xFF8276E6, 0x00FD665A, },
 {0x007D3127, 0xFF0276B2, 0x00807AEC, 0xFF8253ED, 0x00FD894E, },
 {0x007D2883, 0xFF0255AA, 0x0080A49D, 0xFF8232E1, 0x00FDAA56, },
 {0x007D1D78, 0xFF023673, 0x0080CEE2, 0xFF8213A5, 0x00FDC98D, },
 {0x007D0FFD, 0xFF0218F4, 0x0080F9E1, 0xFF81F622, 0x00FDE70C, },
 {0x007D0005, 0xFF01FD15, 0x008125BB, 0xFF81DA40, 0x00FE02EB, },
 {0x007CED83, 0xFF01E2C0, 0x00815296, 0xFF81BFE7, 0x00FE1D40, },
 {0x007CD866, 0xFF01C9DF, 0x00818098, 0xFF81A702, 0x00FE3621, },
 {0x007CC09C, 0xFF01B25D, 0x0081AFE7, 0xFF818F7D, 0x00FE4DA3, }, };
uint32_t MICROBURST_SIGMADSP_EQ_PANEL_STAGE_2_FIXPT_BOOST[21][5] = {
 {0x0079B652, 0xFF0A14FB, 0x007CBBC8, 0xFF898DE6, 0x00F5EB05, },
 {0x0079E08B, 0xFF099146, 0x007D158C, 0xFF8909E9, 0x00F66EBA, },
 {0x007A05EB, 0xFF09146D, 0x007D6D4A, 0xFF888CCB, 0x00F6EB93, },
 {0x007A2686, 0xFF089E1B, 0x007DC342, 0xFF881639, 0x00F761E5, },
 {0x007A426E, 0xFF082E00, 0x007E17B2, 0xFF87A5E0, 0x00F7D200, },
 {0x007A59B4, 0xFF07C3CD, 0x007E6AD9, 0xFF873B73, 0x00F83C33, },
 {0x007A6C63, 0xFF075F37, 0x007EBCF8, 0xFF86D6A5, 0x00F8A0C9, },
 {0x007A7A83, 0xFF06FFF7, 0x007F0E4C, 0xFF867730, 0x00F90009, },
 {0x007A841C, 0xFF06A5C7, 0x007F5F15, 0xFF861CCF, 0x00F95A39, },
 {0x007A892F, 0xFF065067, 0x007FAF91, 0xFF85C741, 0x00F9AF99, },
 {0x00000000, 0x00000000, 0x00800000, 0x00000000, 0x00000000, },
 {0x007A85BD, 0xFF05B31E, 0x008050A2, 0xFF8529A1, 0x00FA4CE2, },
 {0x007A7D2E, 0xFF056AC0, 0x0080A1B6, 0xFF84E11B, 0x00FA9540, },
 {0x007A7002, 0xFF052648, 0x0080F380, 0xFF849C7E, 0x00FAD9B8, },
 {0x007A5E2C, 0xFF04E582, 0x0081463F, 0xFF845B95, 0x00FB1A7E, },
 {0x007A4799, 0xFF04A83D, 0x00819A39, 0xFF841E2E, 0x00FB57C3, },
 {0x007A2C33, 0xFF046E4A, 0x0081EFB1, 0xFF83E41B, 0x00FB91B6, },
 {0x007A0BE2, 0xFF04377D, 0x008246EF, 0xFF83AD2F, 0x00FBC883, },
 {0x0079E687, 0xFF0403AA, 0x0082A039, 0xFF837940, 0x00FBFC56, },
 {0x0079BC01, 0xFF03D2AA, 0x0082FBDA, 0xFF834825, 0x00FC2D56, },
 {0x00798C2B, 0xFF03A456, 0x00835A1E, 0xFF8319B7, 0x00FC5BAA, }, };
uint32_t MICROBURST_SIGMADSP_EQ_PANEL_STAGE_3_FIXPT_BOOST[21][5] = {
 {0x0073E6A3, 0xFF146AD5, 0x0079B6F0, 0xFF92626D, 0x00EB952B, },
 {0x0074320A, 0xFF137794, 0x007A60E3, 0xFF916D13, 0x00EC886C, },
 {0x007474AA, 0xFF129020, 0x007B07B6, 0xFF90839F, 0x00ED6FE0, },
 {0x0074AE97, 0xFF11B3FD, 0x007BABD2, 0xFF8FA596, 0x00EE4C03, },
 {0x0074DFE1, 0xFF10E2B3, 0x007C4DA1, 0xFF8ED27E, 0x00EF1D4D, },
 {0x00750893, 0xFF101BCC, 0x007CED8E, 0xFF8E09DF, 0x00EFE434, },
 {0x007528B4, 0xFF0F5ED4, 0x007D8C06, 0xFF8D4B46, 0x00F0A12C, },
 {0x00754048, 0xFF0EAB5C, 0x007E2977, 0xFF8C9642, 0x00F154A4, },
 {0x00754F4B, 0xFF0E00F8, 0x007EC64F, 0xFF8BEA66, 0x00F1FF08, },
 {0x007555B7, 0xFF0D5F3F, 0x007F6301, 0xFF8B4747, 0x00F2A0C1, },
 {0x00000000, 0x00000000, 0x00800000, 0x00000000, 0x00000000, },
 {0x00754892, 0xFF0C343A, 0x00809DC0, 0xFF8A19AD, 0x00F3CBC6, },
 {0x007534D7, 0xFF0BAA2D, 0x00813CB9, 0xFF898E70, 0x00F455D3, },
 {0x00751831, 0xFF0B274A, 0x0081DD64, 0xFF890A6B, 0x00F4D8B6, },
 {0x0074F27A, 0xFF0AAB37, 0x0082803F, 0xFF888D47, 0x00F554C9, },
 {0x0074C389, 0xFF0A35A2, 0x008325C9, 0xFF8816AE, 0x00F5CA5E, },
 {0x00748B2D, 0xFF09C63A, 0x0083CE84, 0xFF87A64F, 0x00F639C6, },
 {0x0074492D, 0xFF095CAF, 0x00847AF8, 0xFF873BDC, 0x00F6A351, },
 {0x0073FD48, 0xFF08F8B9, 0x00852BAF, 0xFF86D709, 0x00F70747, },
 {0x0073A739, 0xFF089A10, 0x0085E138, 0xFF86778F, 0x00F765F0, },
 {0x007346B0, 0xFF084070, 0x00869C27, 0xFF861D29, 0x00F7BF90, }, };
uint32_t MICROBURST_SIGMADSP_EQ_PANEL_STAGE_4_FIXPT_BOOST[21][5] = {
 {0x00699734, 0xFF299D16, 0x00745BC5, 0xFFA20D07, 0x00D662EA, },
 {0x006A0F8F, 0xFF2801AB, 0x00758D59, 0xFFA06318, 0x00D7FE55, },
 {0x006A794D, 0xFF267790, 0x0076BB9D, 0xFF9ECB16, 0x00D98870, },
 {0x006AD457, 0xFF24FE50, 0x0077E721, 0xFF9D4487, 0x00DB01B0, },
 {0x006B2094, 0xFF239572, 0x0079107E, 0xFF9BCEEE, 0x00DC6A8E, },
 {0x006B5DE3, 0xFF223C79, 0x007A3852, 0xFF9A69CB, 0x00DDC387, },
 {0x006B8C23, 0xFF20F2E9, 0x007B5F43, 0xFF99149A, 0x00DF0D17, },
 {0x006BAB2C, 0xFF1FB83F, 0x007C85FD, 0xFF97CED7, 0x00E047C1, },
 {0x006BBAD0, 0xFF1E8BFD, 0x007DAD33, 0xFF9697FD, 0x00E17403, },
 {0x006BBADA, 0xFF1D6DA2, 0x007ED59E, 0xFF956F88, 0x00E2925E, },
 {0x00000000, 0x00000000, 0x00800000, 0x00000000, 0x00000000, },
 {0x006B8B2A, 0xFF1B589E, 0x00812D20, 0xFF9347B6, 0x00E4A762, },
 {0x006B5AE1, 0xFF1A60F7, 0x00825DCC, 0xFF924753, 0x00E59F09, },
 {0x006B19DB, 0xFF19753D, 0x008392DC, 0xFF915348, 0x00E68AC3, },
 {0x006AC7BB, 0xFF1894F5, 0x0084CD2E, 0xFF906B17, 0x00E76B0B, },
 {0x006A6416, 0xFF17BFA7, 0x00860DA8, 0xFF8F8E42, 0x00E84059, },
 {0x0069EE76, 0xFF16F4DD, 0x00875539, 0xFF8EBC51, 0x00E90B23, },
 {0x00696659, 0xFF163425, 0x0088A4DA, 0xFF8DF4CD, 0x00E9CBDB, },
 {0x0068CB30, 0xFF157D10, 0x0089FD8D, 0xFF8D3742, 0x00EA82F0, },
 {0x00681C60, 0xFF14CF31, 0x008B605F, 0xFF8C8341, 0x00EB30CF, },
 {0x0067593F, 0xFF142A1E, 0x008CCE66, 0xFF8BD85B, 0x00EBD5E2, }, };
uint32_t MICROBURST_SIGMADSP_EQ_PANEL_STAGE_5_FIXPT_BOOST[21][5] = {
 {0x00597B89, 0xFF54FBBF, 0x006BFD8C, 0xFFBA86EB, 0x00AB0441, },
 {0x005A1619, 0xFF52C498, 0x006DF1E0, 0xFFB7F807, 0x00AD3B68, },
 {0x005A9B28, 0xFF509F84, 0x006FE6D7, 0xFFB57E01, 0x00AF607C, },
 {0x005B0A0F, 0xFF4E8C87, 0x0071DD12, 0xFFB318DF, 0x00B17379, },
 {0x005B6227, 0xFF4C8B96, 0x0073D545, 0xFFB0C894, 0x00B3746A, },
 {0x005BA2C8, 0xFF4A9C98, 0x0075D035, 0xFFAE8D03, 0x00B56368, },
 {0x005BCB45, 0xFF48BF67, 0x0077CEBB, 0xFFAC6600, 0x00B74099, },
 {0x005BDAEF, 0xFF46F3D1, 0x0079D1C0, 0xFFAA5350, 0x00B90C2F, },
 {0x005BD110, 0xFF453999, 0x007BDA40, 0xFFA854B0, 0x00BAC667, },
 {0x005BACEA, 0xFF43907A, 0x007DE94B, 0xFFA669CB, 0x00BC6F86, },
 {0x00000000, 0x00000000, 0x00800000, 0x00000000, 0x00000000, },
 {0x005B12A7, 0xFF40703F, 0x00821F94, 0xFFA2CDC5, 0x00BF8FC1, },
 {0x005A9AE0, 0xFF3EF870, 0x0084494E, 0xFFA11BD3, 0x00C10790, },
 {0x005A0578, 0xFF3D9054, 0x00867E87, 0xFF9F7C01, 0x00C26FAC, },
 {0x00595179, 0xFF3C3783, 0x0088C0AE, 0xFF9DEDD9, 0x00C3C87D, },
 {0x00587DDD, 0xFF3AED93, 0x008B1146, 0xFF9C70DD, 0x00C5126D, },
 {0x0057898A, 0xFF39B214, 0x008D71E6, 0xFF9B0490, 0x00C64DEC, },
 {0x00567356, 0xFF388497, 0x008FE43B, 0xFF99A86F, 0x00C77B69, },
 {0x00553A00, 0xFF3764AA, 0x00926A09, 0xFF985BF7, 0x00C89B56, },
 {0x0053DC31, 0xFF3651D9, 0x0095052C, 0xFF971EA2, 0x00C9AE27, },
 {0x0052587C, 0xFF354BB1, 0x0097B798, 0xFF95EFEC, 0x00CAB44F, }, };
uint32_t MICROBURST_SIGMADSP_EQ_PANEL_STAGE_6_FIXPT_BOOST[21][5] = {
 {0x0046D9BA, 0xFFAB6B4E, 0x00624FAA, 0xFFD6D69C, 0x005494B2, },
 {0x00476440, 0xFFA9C87E, 0x00650AC4, 0xFFD390FC, 0x00563782, },
 {0x0047D31F, 0xFFA82DF7, 0x0067D0F4, 0xFFD05BEE, 0x0057D209, },
 {0x004824C8, 0xFFA69C23, 0x006AA2F2, 0xFFCD3846, 0x005963DD, },
 {0x004857AA, 0xFFA51361, 0x006D8194, 0xFFCA26C2, 0x005AEC9F, },
 {0x00486A2B, 0xFFA39404, 0x00706DCC, 0xFFC72808, 0x005C6BFC, },
 {0x00485AAF, 0xFFA21E54, 0x007368A9, 0xFFC43CA8, 0x005DE1AC, },
 {0x0048278F, 0xFFA0B28C, 0x00767359, 0xFFC16518, 0x005F4D74, },
 {0x0047CF1D, 0xFF9F50DD, 0x00798F28, 0xFFBEA1BA, 0x0060AF23, },
 {0x00474FA1, 0xFF9DF96D, 0x007CBD86, 0xFFBBF2D9, 0x00620693, },
 {0x00000000, 0x00000000, 0x00800000, 0x00000000, 0x00000000, },
 {0x0045D465, 0xFF9B69A9, 0x00835849, 0xFFB6D352, 0x00649657, },
 {0x0044D4EF, 0xFF9A316E, 0x0086C835, 0xFFB462DC, 0x0065CE92, },
 {0x0043A6FD, 0xFF9903A3, 0x008A51BE, 0xFFB20746, 0x0066FC5D, },
 {0x00424883, 0xFF97E03E, 0x008DF702, 0xFFAFC07B, 0x00681FC2, },
 {0x0040B760, 0xFF96C72C, 0x0091BA48, 0xFFAD8E58, 0x006938D4, },
 {0x003EF159, 0xFF95B855, 0x00959DFD, 0xFFAB70AA, 0x006A47AB, },
 {0x003CF416, 0xFF94B399, 0x0099A4B9, 0xFFA96731, 0x006B4C67, },
 {0x003ABD1F, 0xFF93B8D2, 0x009DD13E, 0xFFA771A3, 0x006C472E, },
 {0x003849DC, 0xFF92C7D5, 0x00A2267A, 0xFFA58FA9, 0x006D382B, },
 {0x0035978F, 0xFF91E072, 0x00A6A78C, 0xFFA3C0E5, 0x006E1F8E, }, };
uint32_t MICROBURST_SIGMADSP_EQ_PANEL_STAGE_7_FIXPT_BOOST[21][5] = {
 {0x0025293C, 0x005524DC, 0x00574439, 0xFFD919F8, 0xFFD553B8, },
 {0x00258BA6, 0x0055BAB8, 0x005AA971, 0xFFD92C07, 0xFFD0E42A, },
 {0x0025F4BA, 0x005638DD, 0x005E31A8, 0xFFD92199, 0xFFCC7F28, },
 {0x00266536, 0x00569C60, 0x0061DE1B, 0xFFD8FC07, 0xFFC82448, },
 {0x0026DE03, 0x0056E235, 0x0065B013, 0xFFD8BC85, 0xFFC3D330, },
 {0x00276032, 0x00570726, 0x0069A8ED, 0xFFD8642B, 0xFFBF8B91, },
 {0x0027ED02, 0x005707D2, 0x006DCA12, 0xFFD7F3F5, 0xFFBB4D25, },
 {0x002885E5, 0x0056E0A9, 0x007214FC, 0xFFD76CCA, 0xFFB717AD, },
 {0x00292C82, 0x00568DE5, 0x00768B34, 0xFFD6CF7A, 0xFFB2EAEC, },
 {0x0029E2B9, 0x00560B87, 0x007B2E53, 0xFFD61CC4, 0xFFAEC6A9, },
 {0x002AAAAB, 0x00555555, 0x00800000, 0xFFD55555, 0xFFAAAAAB, },
 {0x002B86BF, 0x005466D0, 0x008501F1, 0xFFD479C9, 0xFFA696B8, },
 {0x002C79A9, 0x00533B30, 0x008A35E8, 0xFFD38AAD, 0xFFA28A92, },
 {0x002D8675, 0x0051CD61, 0x008F9DB5, 0xFFD2887C, 0xFF9E85F9, },
 {0x002EB08B, 0x005017F9, 0x00953B34, 0xFFD173A4, 0xFF9A88A5, },
 {0x002FFBBE, 0x004E1532, 0x009B1049, 0xFFD04C81, 0xFF969246, },
 {0x00316C59, 0x004BBEE2, 0x00A11EE0, 0xFFCF1360, 0xFF92A284, },
 {0x0033072A, 0x00490E74, 0x00A768EC, 0xFFCDC87A, 0xFF8EB8FC, },
 {0x0034D195, 0x0045FCDC, 0x00ADF05E, 0xFFCC6BF6, 0xFF8AD53B, },
 {0x0036D1A5, 0x0042828C, 0x00B4B727, 0xFFCAFDE5, 0xFF86F6C2, },
 {0x00390E28, 0x003E976D, 0x00BBBF2B, 0xFFC97E43, 0xFF831CFD, }, };

uint32_t MICROBURST_SIGMADSP_TX_EQ_PANEL_DATA_COEFF_LOOP_FIXPT[3] = { MOD_TX_EQ_TX_EQ_PANEL_ALG0_DATA_ADR_FIXPT, MOD_TX_EQ_TX_EQ_PANEL_ALG0_COEFF_ADR_FIXPT, MOD_TX_EQ_TX_EQ_PANEL_ALG0_LOOP_FIXPT };

uint32_t MICROBURST_SIGMADSP_RX_EQ_PANEL_DATA_COEFF_LOOP_FIXPT[4] = { MOD_RX_EQ_RX_EQ_PANEL_ALG0_DATA_ADR_FIXPT, MOD_RX_EQ_RX_EQ_PANEL_ALG0_DATAR_ADR_FIXPT, MOD_RX_EQ_RX_EQ_PANEL_ALG0_COEFF_ADR_FIXPT, MOD_RX_EQ_RX_EQ_PANEL_ALG0_LOOP_FIXPT };


/* Safeload write function for SigmaDSP Firmware parameters */

static int adau1761_safeload_write(struct adau *adau, uint32_t addr, uint32_t *data,
          unsigned int size)
{
          unsigned int i;
          int ret;
          uint32_t bytes_to_write = size/4;
          uint32_t data_swapped[(size/4)];
         // printk (KERN_DEBUG "MB-sigmadsp: register safeload write addr %d size %d\n", addr, size);

          if (size/4 > 5)
                    return -EINVAL;
          for (i = 0; i < size/4; i++)
          {
              //printk (KERN_DEBUG "MB-sigmadsp: %d: %08X\n", addr+i, data[i]);
              data_swapped[i] = htonl(data[i]);
          }

          for (i = 0; i < size/4; i++)
          {
          	//printk (KERN_DEBUG "MB-sigmadsp: safeload regmap write addr %d data %08X\n", ADAU1761_SAFELOAD_DATA(i), data_swapped[i]);
          		ret = regmap_raw_write(adau->regmap, ADAU1761_SAFELOAD_DATA(i), &data_swapped[i], 4);
          	if (ret)
                    	return ret;
          }

//          addr = htonl(addr - 1);
          addr = addr - 0x0001;
          addr = htonl(addr);
          //printk (KERN_DEBUG "MB-sigmadsp: safeload regmap write addr %08X data %08X\n", ADAU1761_SAFELOAD_ADDR, addr);
          ret = regmap_raw_write(adau->regmap, ADAU1761_SAFELOAD_ADDR, &addr, 4);
          if (ret)
                    return ret;
          //printk (KERN_DEBUG "MB-sigmadsp: safeload regmap write addr %08X data %08X\n", ADAU1761_SAFELOAD_SIZE, size/4);
          bytes_to_write = htonl(bytes_to_write);
          ret = regmap_raw_write(adau->regmap, ADAU1761_SAFELOAD_SIZE, &bytes_to_write , 4);
          if (ret)
                    return ret;

          /* Wait for the operation to finish */
          udelay(60);

          return 0;
}

/* Block write function for SigmaDSP Firmware parameters     */

static int adau1761_block_write(struct adau *adau, uint32_t addr, uint32_t *data,
          unsigned int size)
{	
          int ret;
          unsigned int i;
          uint32_t data_swapped[(size/4)];
          //printk (KERN_DEBUG "MB-sigmadsp: register block write addr %d size %d\n", addr, size);
          for (i = 0; i < size/4; i++)
          {
        	  //printk (KERN_DEBUG "MB-sigmadsp: %d: %08X\n", addr+i, data[i]);

        	  data_swapped[i] = htonl(data[i]);
        	  //printk (KERN_DEBUG "MB-sigmadsp: blockwrite regmap write addr %08X data %08X\n", addr, data_swapped[i]);
          }

          ret = regmap_raw_write(adau->regmap, addr, data_swapped, size);
		  return ret;
};

/* 
 * TODO: Update for your system's data type
 */
typedef unsigned short ADI_DATA_U16;
typedef unsigned char  ADI_REG_TYPE;



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
	//printk (KERN_DEBUG "MB-codecdsp: microburst_sigmadsp_cw_key_get called\n");
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

	//printk (KERN_DEBUG "MB-codecdsp: microburst_sigmadsp_cw_key_put called.  Key state: %d\n", key_state);
	if (key_state) {
		////printk ("MB-codecdsp: setting key down state\n");
		adau1761_block_write(adau, cw_key_addr, &key_down ,sizeof(key_down));
	}
	else  {
		////printk ("MB-codecdsp: setting key up state\n");
		adau1761_block_write(adau, cw_key_addr, &key_up, sizeof(key_up));
	}
	return 0;
};

static int microburst_sigmadsp_monitor_voice_cw_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: monitor_voice_cw_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_monitor_voice_cw_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: monitor_voice_cw_put called\n");
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

	//printk (KERN_DEBUG "MB-sigmadsp: monitor voice cw setting to %d\n", mon_cw);
	adau1761_block_write(adau, mux_addr, buf, 8);

	return 0;
};

static int microburst_sigmadsp_compander_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: compander_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_compander_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: compander_put called\n");
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

	//printk (KERN_DEBUG "MB-sigmadsp: compander setting to %d\n", compander_enable);
	adau1761_block_write(adau, mux_addr, buf, 8);

	return 0;
};

static int microburst_sigmadsp_tx_eq_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: tx_eq_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_tx_eq_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: tx_eq_put called\n");
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

	//printk (KERN_DEBUG "MB-sigmadsp: tx_eq setting to %d\n", tx_eq_enable);
	adau1761_block_write(adau, mux_addr, buf, 8);
	return 0;
};

static int microburst_sigmadsp_rx_eq_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: rx_eq_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_rx_eq_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: rx_eq_put called\n");
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

	//printk (KERN_DEBUG "MB-sigmadsp: rx_eq setting to %d\n", rx_eq_enable);
	adau1761_block_write(adau, mux_addr, buf, 8);
	return 0;
};

static int microburst_sigmadsp_meter_select_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: meter_select_get called\n");

	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_meter_select_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: meter_select_put called\n");
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

	//printk (KERN_DEBUG "MB-sigmadsp: meter_select setting to %d\n", meter_select);
	adau1761_block_write(adau, mux_addr, buf, 8);

	return 0;
};

static int microburst_sigmadsp_echo_cancel_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: echo_cancel_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_echo_cancel_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: echo_cancel_put called\n");
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

	//printk (KERN_DEBUG "MB-sigmadsp: echo_cancel setting to %d\n", echo_cancel);
	adau1761_block_write(adau, mux_addr, buf, 8);

	return 0;
};

static int microburst_sigmadsp_input_source_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: input_source_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_input_source_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: input_source_put called\n");
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

	//printk (KERN_DEBUG "MB-sigmadsp: input_source setting to %d\n", input_source);
	adau1761_block_write(adau, mux_addr, buf, 12);

	return 0;
};

static int microburst_sigmadsp_sig_gen_select_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: sig_gen_select_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_sig_gen_select_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: sig_gen_select_put called\n");
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
	adau1761_block_write(adau, mux_addr, buf, 16);

	return 0;
};

static int microburst_sigmadsp_monitor_level_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: monitor_level_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_monitor_level_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: monitor_level_put called\n");
	uint32_t buf;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int monitor_level;
	uint32_t mux_addr = MOD_MONITOR_LEVEL_GAIN1940ALGNS2_ADDR;
	monitor_level = ucontrol->value.integer.value[0];
	buf = MICROBURST_SIGMADSP_FIXPT_LEVEL_LOOKUP_64_STEP_MINUS_96_TO_ZERO[monitor_level];
	//printk (KERN_DEBUG "MB-sigmadsp: monitor_level setting to %d\n", monitor_level);
	adau1761_block_write(adau, mux_addr, &buf, 4);

	return 0;
};

static int microburst_sigmadsp_sig_gen_level_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: sig_gen_level_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_sig_gen_level_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: sig_gen_level_put called\n");
	uint32_t buf;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int sig_gen_level;
	uint32_t mux_addr = MOD_SIGGEN_SIG_GEN_LEVEL_GAIN1940ALGNS3_ADDR;
	sig_gen_level = ucontrol->value.integer.value[0];
	buf = MICROBURST_SIGMADSP_FIXPT_LEVEL_LOOKUP_64_STEP_MINUS_96_TO_ZERO[sig_gen_level];
	//printk (KERN_DEBUG "MB-sigmadsp: sig_gen_level setting to %d\n", sig_gen_level);
	adau1761_block_write(adau, mux_addr, &buf, 4);

	return 0;
};

static int microburst_sigmadsp_cw_sidetone_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: cw_sidetone_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_cw_sidetone_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: cw_sidetone_put called\n");
	uint32_t buf[3];
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int sidetone_frequency;
	uint32_t mux_addr = MOD_STATIC_CW_SIDETONE_ALG0_MASK_ADDR;
	sidetone_frequency = ucontrol->value.integer.value[0];
	buf[0] = 0x000000FF;
	buf[1] = (uint32_t)(sidetone_frequency * 699);
	buf[2] = MICROBURST_SIGMADSP_FIXPT_ONE;
	//printk (KERN_DEBUG "MB-sigmadsp: sidetone_frequency setting to %d\n", sidetone_frequency);
	adau1761_block_write(adau, mux_addr, buf, 12);

	return 0;
};

static int microburst_sigmadsp_tx_filter_bw_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: tx_filter_bw_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_tx_filter_bw_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	printk(KERN_DEBUG "MB-sigmadsp: tx_filter_bw_put called\n");
	uint32_t *buf_lp;
	uint32_t *buf_hp;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int bandwidth_select;
	uint32_t lp_addr = MOD_TX_FILTER_TX_LPF_ALG0_STAGE0_B2_ADDR;
	uint32_t hp_addr = MOD_TX_FILTER_TX_HPF_ALG0_STAGE0_B2_ADDR;
        uint32_t lp_loop_addr = MOD_TX_FILTER_TX_LPF_ALG0_LOOP_ADDR;
        uint32_t hp_loop_addr = MOD_TX_FILTER_TX_HPF_ALG0_LOOP_ADDR;
	bandwidth_select = ucontrol->value.integer.value[0];
	switch (bandwidth_select)
          {
          case 2:      
            buf_lp = MICROBURST_SIGMADSP_FIXPT_TX_LPF_10000HZ;
            buf_hp = MICROBURST_SIGMADSP_FIXPT_TX_HPF_60HZ;
            break;
          case 1: 
            buf_lp = MICROBURST_SIGMADSP_FIXPT_TX_LPF_8000HZ;
            buf_hp = MICROBURST_SIGMADSP_FIXPT_TX_HPF_200HZ;
                        
            break;
          default:
            buf_lp = MICROBURST_SIGMADSP_FIXPT_TX_LPF_2500HZ;
            buf_hp = MICROBURST_SIGMADSP_FIXPT_TX_HPF_500HZ;
            break;
	}

	printk (KERN_DEBUG "MB-sigmadsp: tx_filter_bw setting to %d\n", bandwidth_select);
        //	adau1761_block_write(adau, lp_addr, buf_lp, 24);
        //	adau1761_block_write(adau, hp_addr, buf_hp, 24);
        adau1761_safeload_write(adau, lp_addr, buf_lp, 20);
        adau1761_block_write(adau, lp_loop_addr ,buf_lp + 5, 4);

        adau1761_safeload_write(adau, hp_addr, buf_hp, 20);
        adau1761_block_write(adau, hp_loop_addr, buf_hp + 5, 4);

	return 0;
};

static int microburst_sigmadsp_tx_eq_stage_0_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: tx_eq_stage_0_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_tx_eq_stage_0_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: tx_eq_stage_0_put called\n");
	uint32_t buf[5];
	uint32_t buf2[3];
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int boost_level;
	int i;
	uint32_t stage_addr = MOD_TX_EQ_TX_EQ_PANEL_ALG0_STAGE0_B2_ADDR;
	uint32_t data_addr = MOD_TX_EQ_TX_EQ_PANEL_ALG0_DATA_ADR_ADDR;
	boost_level = ucontrol->value.integer.value[0];
	for (i = 0; i < 5; i++)
	{
	buf[i] = MICROBURST_SIGMADSP_EQ_PANEL_STAGE_0_FIXPT_BOOST[boost_level][i];
	////printk (KERN_DEBUG "MB-sigmadsp: tx eq stage data %d %d %x\n", boost_level, i, buf[i]);
	};
	for (i = 0; i < 3; i++)
	{
	buf2[i] = MICROBURST_SIGMADSP_TX_EQ_PANEL_DATA_COEFF_LOOP_FIXPT[i];
	};
	//printk (KERN_DEBUG "MB-sigmadsp: tx_eq_stage_0 boost level setting to %d\n", boost_level);
//	adau1761_block_write(adau, stage_addr, &buf, 20);
//	adau1761_block_write(adau, data_addr, &buf2, 12);
	adau1761_safeload_write(adau, stage_addr, buf, 20);
	adau1761_block_write(adau, data_addr, buf2, 12);


	return 0;
};

static int microburst_sigmadsp_tx_eq_stage_1_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: tx_eq_stage_1_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_tx_eq_stage_1_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: tx_eq_stage_1_put called\n");

        uint32_t buf[5];
	uint32_t buf2[3];
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int boost_level;
	int i;
	uint32_t stage_addr = MOD_TX_EQ_TX_EQ_PANEL_ALG0_STAGE1_B2_ADDR;
	uint32_t data_addr = MOD_TX_EQ_TX_EQ_PANEL_ALG0_DATA_ADR_ADDR;
	boost_level = ucontrol->value.integer.value[0];
	for (i = 0; i < 5; i++)
	{
	buf[i] = MICROBURST_SIGMADSP_EQ_PANEL_STAGE_1_FIXPT_BOOST[boost_level][i];
	////printk (KERN_DEBUG "MB-sigmadsp: tx eq stage data %d %d %x\n", boost_level, i, buf[i]);
	};
	for (i = 0; i < 3; i++)
	{
	buf2[i] = MICROBURST_SIGMADSP_TX_EQ_PANEL_DATA_COEFF_LOOP_FIXPT[i];
	};
	//printk (KERN_DEBUG "MB-sigmadsp: tx_eq_stage_1 boost level setting to %d\n", boost_level);
//	adau1761_block_write(adau, stage_addr, &buf, 20);
//	adau1761_block_write(adau, data_addr, &buf2, 12);

	adau1761_safeload_write(adau, stage_addr, buf, 20);
	adau1761_block_write(adau, data_addr, buf2, 12);
	return 0;
};

static int microburst_sigmadsp_tx_eq_stage_2_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: tx_eq_stage_2_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_tx_eq_stage_2_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: tx_eq_stage_2_put called\n");
	uint32_t buf[5];
	uint32_t buf2[3];
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int boost_level;
	int i;
	uint32_t stage_addr = MOD_TX_EQ_TX_EQ_PANEL_ALG0_STAGE2_B2_ADDR;
	uint32_t data_addr = MOD_TX_EQ_TX_EQ_PANEL_ALG0_DATA_ADR_ADDR;
	boost_level = ucontrol->value.integer.value[0];
	for (i = 0; i < 5; i++)
	{
	buf[i] = MICROBURST_SIGMADSP_EQ_PANEL_STAGE_2_FIXPT_BOOST[boost_level][i];
	////printk (KERN_DEBUG "MB-sigmadsp: tx eq stage data %d %d %x\n", boost_level, i, buf[i]);
	};
	for (i = 0; i < 3; i++)
	{
	buf2[i] = MICROBURST_SIGMADSP_TX_EQ_PANEL_DATA_COEFF_LOOP_FIXPT[i];
	};
	//printk (KERN_DEBUG "MB-sigmadsp: tx_eq_stage_2 boost level setting to %d\n", boost_level);
	adau1761_safeload_write(adau, stage_addr, buf, 20);
	adau1761_block_write(adau, data_addr, buf2, 12);

	return 0;
};

static int microburst_sigmadsp_tx_eq_stage_3_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: tx_eq_stage_3_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_tx_eq_stage_3_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: tx_eq_stage_3_put called\n");
	uint32_t buf[5];
	uint32_t buf2[3];
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int boost_level;
	int i;
	uint32_t stage_addr = MOD_TX_EQ_TX_EQ_PANEL_ALG0_STAGE3_B2_ADDR;
	uint32_t data_addr = MOD_TX_EQ_TX_EQ_PANEL_ALG0_DATA_ADR_ADDR;
	boost_level = ucontrol->value.integer.value[0];
	for (i = 0; i < 5; i++)
	{
	buf[i] = MICROBURST_SIGMADSP_EQ_PANEL_STAGE_3_FIXPT_BOOST[boost_level][i];
	////printk (KERN_DEBUG "MB-sigmadsp: tx eq stage data %d %d %x\n", boost_level, i, buf[i]);
	};
	for (i = 0; i < 3; i++)
	{
	buf2[i] = MICROBURST_SIGMADSP_TX_EQ_PANEL_DATA_COEFF_LOOP_FIXPT[i];
	};
	//printk (KERN_DEBUG "MB-sigmadsp: tx_eq_stage_3 boost level setting to %d\n", boost_level);
	adau1761_safeload_write(adau, stage_addr, buf, 20);
	adau1761_block_write(adau, data_addr, buf2, 12);

	return 0;
};

static int microburst_sigmadsp_tx_eq_stage_4_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: tx_eq_stage_4_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_tx_eq_stage_4_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: tx_eq_stage_4_put called\n");
	uint32_t buf[5];
	uint32_t buf2[3];
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int boost_level;
	int i;
	uint32_t stage_addr = MOD_TX_EQ_TX_EQ_PANEL_ALG0_STAGE4_B2_ADDR;
	uint32_t data_addr = MOD_TX_EQ_TX_EQ_PANEL_ALG0_DATA_ADR_ADDR;
	boost_level = ucontrol->value.integer.value[0];
	for (i = 0; i < 5; i++)
	{
	buf[i] = MICROBURST_SIGMADSP_EQ_PANEL_STAGE_4_FIXPT_BOOST[boost_level][i];
	////printk (KERN_DEBUG "MB-sigmadsp: tx eq stage data %d %d %x\n", boost_level, i, buf[i]);
	};
	for (i = 0; i < 3; i++)
	{
	buf2[i] = MICROBURST_SIGMADSP_TX_EQ_PANEL_DATA_COEFF_LOOP_FIXPT[i];
	};
	//printk (KERN_DEBUG "MB-sigmadsp: tx_eq_stage_4 boost level setting to %d\n", boost_level);
	adau1761_safeload_write(adau, stage_addr, buf, 20);
	adau1761_block_write(adau, data_addr, buf2, 12);

	return 0;
};

static int microburst_sigmadsp_tx_eq_stage_5_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: tx_eq_stage_5_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_tx_eq_stage_5_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: tx_eq_stage_5_put called\n");
	uint32_t buf[5];
	uint32_t buf2[3];
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int boost_level;
	int i;
	uint32_t stage_addr = MOD_TX_EQ_TX_EQ_PANEL_ALG0_STAGE5_B2_ADDR;
	uint32_t data_addr = MOD_TX_EQ_TX_EQ_PANEL_ALG0_DATA_ADR_ADDR;
	boost_level = ucontrol->value.integer.value[0];
	for (i = 0; i < 5; i++)
	{
	buf[i] = MICROBURST_SIGMADSP_EQ_PANEL_STAGE_5_FIXPT_BOOST[boost_level][i];
	////printk (KERN_DEBUG "MB-sigmadsp: tx eq stage data %d %d %x\n", boost_level, i, buf[i]);
	};
	for (i = 0; i < 3; i++)
	{
	buf2[i] = MICROBURST_SIGMADSP_TX_EQ_PANEL_DATA_COEFF_LOOP_FIXPT[i];
	};
	//printk (KERN_DEBUG "MB-sigmadsp: tx_eq_stage_5 boost level setting to %d\n", boost_level);
	adau1761_safeload_write(adau, stage_addr, buf, 20);
	adau1761_block_write(adau, data_addr, buf2, 12);

	return 0;
};

static int microburst_sigmadsp_tx_eq_stage_6_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: tx_eq_stage_6_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_tx_eq_stage_6_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: tx_eq_stage_6_put called\n");
	uint32_t buf[5];
	uint32_t buf2[3];
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int boost_level;
	int i;
	uint32_t stage_addr = MOD_TX_EQ_TX_EQ_PANEL_ALG0_STAGE6_B2_ADDR;
	uint32_t data_addr = MOD_TX_EQ_TX_EQ_PANEL_ALG0_DATA_ADR_ADDR;
	boost_level = ucontrol->value.integer.value[0];
	for (i = 0; i < 5; i++)
	{
	buf[i] = MICROBURST_SIGMADSP_EQ_PANEL_STAGE_6_FIXPT_BOOST[boost_level][i];
	////printk (KERN_DEBUG "MB-sigmadsp: tx eq stage data %d %d %x\n", boost_level, i, buf[i]);
	};
	for (i = 0; i < 3; i++)
	{
	buf2[i] = MICROBURST_SIGMADSP_TX_EQ_PANEL_DATA_COEFF_LOOP_FIXPT[i];
	};
	//printk (KERN_DEBUG "MB-sigmadsp: tx_eq_stage_6 boost level setting to %d\n", boost_level);
	adau1761_safeload_write(adau, stage_addr, buf, 20);
	adau1761_block_write(adau, data_addr, buf2, 12);

	return 0;
};

static int microburst_sigmadsp_tx_eq_stage_7_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: tx_eq_stage_7_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_tx_eq_stage_7_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: tx_eq_stage_7_put called\n");
	uint32_t buf[5];
	uint32_t buf2[3];
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int boost_level;
	int i;
	uint32_t stage_addr = MOD_TX_EQ_TX_EQ_PANEL_ALG0_STAGE7_B2_ADDR;
	uint32_t data_addr = MOD_TX_EQ_TX_EQ_PANEL_ALG0_DATA_ADR_ADDR;
	boost_level = ucontrol->value.integer.value[0];
	for (i = 0; i < 5; i++)
	{
	buf[i] = MICROBURST_SIGMADSP_EQ_PANEL_STAGE_7_FIXPT_BOOST[boost_level][i];
	////printk (KERN_DEBUG "MB-sigmadsp: tx eq stage data %d %d %x\n", boost_level, i, buf[i]);
	};
	for (i = 0; i < 3; i++)
	{
	buf2[i] = MICROBURST_SIGMADSP_TX_EQ_PANEL_DATA_COEFF_LOOP_FIXPT[i];
	};
	//printk (KERN_DEBUG "MB-sigmadsp: tx_eq_stage_7 boost level setting to %d\n", boost_level);
	adau1761_safeload_write(adau, stage_addr, buf, 20);
	adau1761_block_write(adau, data_addr, buf2, 12);

	return 0;
};

static int microburst_sigmadsp_rx_eq_stage_0_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: rx_eq_stage_0_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_rx_eq_stage_0_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: rx_eq_stage_0_put called\n");
	uint32_t buf[5];
	uint32_t buf2[4];
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int boost_level;
	int i;
	uint32_t stage_addr = MOD_RX_EQ_RX_EQ_PANEL_ALG0_STAGE0_B2_ADDR;
	uint32_t data_addr = MOD_RX_EQ_RX_EQ_PANEL_ALG0_DATA_ADR_ADDR;
	boost_level = ucontrol->value.integer.value[0];
	for (i = 0; i < 5; i++)
	{
	buf[i] = MICROBURST_SIGMADSP_EQ_PANEL_STAGE_0_FIXPT_BOOST[boost_level][i];
	////printk (KERN_DEBUG "MB-sigmadsp: tx eq stage data %d %d %x\n", boost_level, i, buf[i]);
	};
	for (i = 0; i < 4; i++)
	{
	buf2[i] = MICROBURST_SIGMADSP_RX_EQ_PANEL_DATA_COEFF_LOOP_FIXPT[i];
	};
	//printk (KERN_DEBUG "MB-sigmadsp: rx_eq_stage_0 boost level setting to %d\n", boost_level);
	adau1761_safeload_write(adau, stage_addr, buf, 20);
	adau1761_block_write(adau, data_addr, buf2, 16);

	return 0;
};

static int microburst_sigmadsp_rx_eq_stage_1_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: rx_eq_stage_1_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_rx_eq_stage_1_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: rx_eq_stage_1_put called\n");
	uint32_t buf[5];
	uint32_t buf2[4];
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int boost_level;
	int i;
	uint32_t stage_addr = MOD_RX_EQ_RX_EQ_PANEL_ALG0_STAGE1_B2_ADDR;
	uint32_t data_addr = MOD_RX_EQ_RX_EQ_PANEL_ALG0_DATA_ADR_ADDR;
	boost_level = ucontrol->value.integer.value[0];
	for (i = 0; i < 5; i++)
	{
	buf[i] = MICROBURST_SIGMADSP_EQ_PANEL_STAGE_1_FIXPT_BOOST[boost_level][i];
	////printk (KERN_DEBUG "MB-sigmadsp: tx eq stage data %d %d %x\n", boost_level, i, buf[i]);
	};
	for (i = 0; i < 4; i++)
	{
	buf2[i] = MICROBURST_SIGMADSP_RX_EQ_PANEL_DATA_COEFF_LOOP_FIXPT[i];
	};
	//printk (KERN_DEBUG "MB-sigmadsp: rx_eq_stage_1 boost level setting to %d\n", boost_level);
	adau1761_safeload_write(adau, stage_addr, buf, 20);
	adau1761_block_write(adau, data_addr, buf2, 16);

	return 0;
};
static int microburst_sigmadsp_rx_eq_stage_2_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: rx_eq_stage_2_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_rx_eq_stage_2_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: rx_eq_stage_2_put called\n");
	uint32_t buf[5];
	uint32_t buf2[4];
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int boost_level;
	int i;
	uint32_t stage_addr = MOD_RX_EQ_RX_EQ_PANEL_ALG0_STAGE2_B2_ADDR;
	uint32_t data_addr = MOD_RX_EQ_RX_EQ_PANEL_ALG0_DATA_ADR_ADDR;
	boost_level = ucontrol->value.integer.value[0];
	for (i = 0; i < 5; i++)
	{
	buf[i] = MICROBURST_SIGMADSP_EQ_PANEL_STAGE_2_FIXPT_BOOST[boost_level][i];
	////printk (KERN_DEBUG "MB-sigmadsp: tx eq stage data %d %d %x\n", boost_level, i, buf[i]);
	};
	for (i = 0; i < 4; i++)
	{
	buf2[i] = MICROBURST_SIGMADSP_RX_EQ_PANEL_DATA_COEFF_LOOP_FIXPT[i];
	};
	//printk (KERN_DEBUG "MB-sigmadsp: rx_eq_stage_2 boost level setting to %d\n", boost_level);
	adau1761_safeload_write(adau, stage_addr, buf, 20);
	adau1761_block_write(adau, data_addr, buf2, 16);

	return 0;
};
static int microburst_sigmadsp_rx_eq_stage_3_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: rx_eq_stage_3_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_rx_eq_stage_3_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: rx_eq_stage_3_put called\n");
	uint32_t buf[5];
	uint32_t buf2[4];
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int boost_level;
	int i;
	uint32_t stage_addr = MOD_RX_EQ_RX_EQ_PANEL_ALG0_STAGE3_B2_ADDR;
	uint32_t data_addr = MOD_RX_EQ_RX_EQ_PANEL_ALG0_DATA_ADR_ADDR;
	boost_level = ucontrol->value.integer.value[0];
	for (i = 0; i < 5; i++)
	{
	buf[i] = MICROBURST_SIGMADSP_EQ_PANEL_STAGE_3_FIXPT_BOOST[boost_level][i];
	////printk (KERN_DEBUG "MB-sigmadsp: tx eq stage data %d %d %x\n", boost_level, i, buf[i]);
	};
	for (i = 0; i < 4; i++)
	{
	buf2[i] = MICROBURST_SIGMADSP_RX_EQ_PANEL_DATA_COEFF_LOOP_FIXPT[i];
	};
	//printk (KERN_DEBUG "MB-sigmadsp: rx_eq_stage_3 boost level setting to %d\n", boost_level);
	adau1761_safeload_write(adau, stage_addr, buf, 20);
	adau1761_block_write(adau, data_addr, buf2, 16);

	return 0;
};
static int microburst_sigmadsp_rx_eq_stage_4_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: rx_eq_stage_4_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_rx_eq_stage_4_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: rx_eq_stage_4_put called\n");
	uint32_t buf[5];
	uint32_t buf2[4];
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int boost_level;
	int i;
	uint32_t stage_addr = MOD_RX_EQ_RX_EQ_PANEL_ALG0_STAGE4_B2_ADDR;
	uint32_t data_addr = MOD_RX_EQ_RX_EQ_PANEL_ALG0_DATA_ADR_ADDR;
	boost_level = ucontrol->value.integer.value[0];
	for (i = 0; i < 5; i++)
	{
	buf[i] = MICROBURST_SIGMADSP_EQ_PANEL_STAGE_4_FIXPT_BOOST[boost_level][i];
	////printk (KERN_DEBUG "MB-sigmadsp: tx eq stage data %d %d %x\n", boost_level, i, buf[i]);
	};
	for (i = 0; i < 4; i++)
	{
	buf2[i] = MICROBURST_SIGMADSP_RX_EQ_PANEL_DATA_COEFF_LOOP_FIXPT[i];
	};
	//printk (KERN_DEBUG "MB-sigmadsp: rx_eq_stage_4 boost level setting to %d\n", boost_level);
	adau1761_safeload_write(adau, stage_addr, buf, 20);
	adau1761_block_write(adau, data_addr, buf2, 16);

	return 0;
};
static int microburst_sigmadsp_rx_eq_stage_5_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: rx_eq_stage_5_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_rx_eq_stage_5_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: rx_eq_stage_5_put called\n");
	uint32_t buf[5];
	uint32_t buf2[4];
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int boost_level;
	int i;
	uint32_t stage_addr = MOD_RX_EQ_RX_EQ_PANEL_ALG0_STAGE5_B2_ADDR;
	uint32_t data_addr = MOD_RX_EQ_RX_EQ_PANEL_ALG0_DATA_ADR_ADDR;
	boost_level = ucontrol->value.integer.value[0];
	for (i = 0; i < 5; i++)
	{
	buf[i] = MICROBURST_SIGMADSP_EQ_PANEL_STAGE_5_FIXPT_BOOST[boost_level][i];
	////printk (KERN_DEBUG "MB-sigmadsp: tx eq stage data %d %d %x\n", boost_level, i, buf[i]);
	};
	for (i = 0; i < 4; i++)
	{
	buf2[i] = MICROBURST_SIGMADSP_RX_EQ_PANEL_DATA_COEFF_LOOP_FIXPT[i];
	};
	//printk (KERN_DEBUG "MB-sigmadsp: rx_eq_stage_5 boost level setting to %d\n", boost_level);
	adau1761_safeload_write(adau, stage_addr, buf, 20);
	adau1761_block_write(adau, data_addr, buf2, 16);

	return 0;
};
static int microburst_sigmadsp_rx_eq_stage_6_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: rx_eq_stage_6_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_rx_eq_stage_6_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: rx_eq_stage_6_put called\n");
	uint32_t buf[5];
	uint32_t buf2[4];
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int boost_level;
	int i;
	uint32_t stage_addr = MOD_RX_EQ_RX_EQ_PANEL_ALG0_STAGE6_B2_ADDR;
	uint32_t data_addr = MOD_RX_EQ_RX_EQ_PANEL_ALG0_DATA_ADR_ADDR;
	boost_level = ucontrol->value.integer.value[0];
	for (i = 0; i < 5; i++)
	{
	buf[i] = MICROBURST_SIGMADSP_EQ_PANEL_STAGE_6_FIXPT_BOOST[boost_level][i];
	////printk (KERN_DEBUG "MB-sigmadsp: tx eq stage data %d %d %x\n", boost_level, i, buf[i]);
	};
	for (i = 0; i < 4; i++)
	{
	buf2[i] = MICROBURST_SIGMADSP_RX_EQ_PANEL_DATA_COEFF_LOOP_FIXPT[i];
	};
	//printk (KERN_DEBUG "MB-sigmadsp: rx_eq_stage_6 boost level setting to %d\n", boost_level);
	adau1761_safeload_write(adau, stage_addr, buf, 20);
	adau1761_block_write(adau, data_addr, buf2, 16);

	return 0;
};
static int microburst_sigmadsp_rx_eq_stage_7_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: rx_eq_stage_7_get called\n");
	ucontrol->value.integer.value[0] = kcontrol->private_value;
	return 0;
};

static int microburst_sigmadsp_rx_eq_stage_7_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: rx_eq_stage_7_put called\n");
	uint32_t buf[5];
	uint32_t buf2[4];
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	int boost_level;
	int i;
	uint32_t stage_addr = MOD_RX_EQ_RX_EQ_PANEL_ALG0_STAGE7_B2_ADDR;
	uint32_t data_addr = MOD_RX_EQ_RX_EQ_PANEL_ALG0_DATA_ADR_ADDR;
	boost_level = ucontrol->value.integer.value[0];
	for (i = 0; i < 5; i++)
	{
	buf[i] = MICROBURST_SIGMADSP_EQ_PANEL_STAGE_7_FIXPT_BOOST[boost_level][i];
	////printk (KERN_DEBUG "MB-sigmadsp: tx eq stage data %d %d %x\n", boost_level, i, buf[i]);
	};
	for (i = 0; i < 4; i++)
	{
	buf2[i] = MICROBURST_SIGMADSP_RX_EQ_PANEL_DATA_COEFF_LOOP_FIXPT[i];
	};
	//printk (KERN_DEBUG "MB-sigmadsp: rx_eq_stage_7 boost level setting to %d\n", boost_level);
	adau1761_safeload_write(adau, stage_addr, buf, 20);
	adau1761_block_write(adau, data_addr, buf2, 16);

	return 0;
};

static int microburst_sigmadsp_compander_decay_put(struct snd_kcontrol *kcontrol,
                                                  struct snd_ctl_elem_value *ucontrol)
{
  uint32_t c_decay_address = MOD_COMPANDER_ALG0_STDPEAKINGCOMPRESSORALG1DECAY_ADDR;
  struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
  struct adau *adau = snd_soc_codec_get_drvdata(codec);

  uint32_t buf[1];

  buf[0] = ucontrol->value.integer.value[0];

  adau1761_block_write(adau, c_decay_address, buf, 4);

  return 0;

};

static int microburst_sigmadsp_compander_decay_get(struct snd_kcontrol *kcontrol,
                                                  struct snd_ctl_elem_value *ucontrol)
{
  uint32_t c_decay_address = MOD_COMPANDER_ALG0_STDPEAKINGCOMPRESSORALG1DECAY_ADDR;
  struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
  struct adau *adau = snd_soc_codec_get_drvdata(codec);

  uint32_t decay_value;

  //  buf[0] = SIGMASTUDIOTYPE_INTEGER_CONVERT(ucontrol->value.integer.value[0]);
  regmap_raw_read(adau->regmap, c_decay_address, &decay_value, 4);
  decay_value = htonl(decay_value);
  ucontrol->value.integer.value[0] = decay_value;
  return 0;

};


/// Compander Hold 
static int microburst_sigmadsp_compander_hold_put(struct snd_kcontrol *kcontrol,
                                                  struct snd_ctl_elem_value *ucontrol)
{
  uint32_t c_hold_address = MOD_COMPANDER_ALG0_STDPEAKINGCOMPRESSORALG1HOLD_ADDR;
  struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
  struct adau *adau = snd_soc_codec_get_drvdata(codec);

  uint32_t buf[1];

  buf[0] = ucontrol->value.integer.value[0];

  adau1761_block_write(adau, c_hold_address, buf, 4);

  return 0;

};

static int microburst_sigmadsp_compander_hold_get(struct snd_kcontrol *kcontrol,
                                                  struct snd_ctl_elem_value *ucontrol)
{
  uint32_t c_hold_address = MOD_COMPANDER_ALG0_STDPEAKINGCOMPRESSORALG1HOLD_ADDR;
  struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
  struct adau *adau = snd_soc_codec_get_drvdata(codec);

  uint32_t hold_value;

  //  buf[0] = SIGMASTUDIOTYPE_INTEGER_CONVERT(ucontrol->value.integer.value[0]);
  regmap_raw_read(adau->regmap, c_hold_address, &hold_value, 4);
  hold_value = htonl(hold_value);
  ucontrol->value.integer.value[0] = hold_value;
  return 0;

};

static int microburst_sigmadsp_extra_line_input_gain_put(struct snd_kcontrol *kcontrol,
                                                  struct snd_ctl_elem_value *ucontrol)
{
  uint32_t extra_line_gain_addr = MOD_LINE_GAIN_GAIN1940ALGNS5_ADDR;
  struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
  struct adau *adau = snd_soc_codec_get_drvdata(codec);

  uint32_t buf[1];
  buf[0] = ucontrol->value.integer.value[0];

  adau1761_block_write(adau, extra_line_gain_addr, buf, 4);
  return 0;
}

// Conversion needs to be done in Firmware!
static int microburst_sigmadsp_extra_line_input_gain_get(struct snd_kcontrol *kcontrol,
                                                  struct snd_ctl_elem_value *ucontrol)
{
  uint32_t extra_line_gain_addr = MOD_LINE_GAIN_GAIN1940ALGNS5_ADDR;
  struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
  struct adau *adau = snd_soc_codec_get_drvdata(codec);

  uint32_t line_gain_value;

  //  buf[0] = SIGMASTUDIOTYPE_INTEGER_CONVERT(ucontrol->value.integer.value[0]);
  regmap_raw_read(adau->regmap, extra_line_gain_addr, &line_gain_value, 4);
  line_gain_value = htonl(line_gain_value);
  ucontrol->value.integer.value[0] = line_gain_value;
  return 0;

};

// Conversion needs to be done in Firmware!
static int microburst_sigmadsp_extra_mic_input_gain_put(struct snd_kcontrol *kcontrol,
                                                struct snd_ctl_elem_value *ucontrol)
{
  uint32_t extra_mic_gain_addr = MOD_MIC_GAIN_GAIN1940ALGNS4_ADDR;
  struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
  struct adau *adau = snd_soc_codec_get_drvdata(codec);

  uint32_t buf[1];
  buf[0] = ucontrol->value.integer.value[0];

  adau1761_block_write(adau, extra_mic_gain_addr, buf, 4);
  return 0;
}

// Conversion needs to be done in Firmware!
static int microburst_sigmadsp_extra_mic_input_gain_get(struct snd_kcontrol *kcontrol,
                                                  struct snd_ctl_elem_value *ucontrol)
{
  uint32_t extra_mic_gain_addr = MOD_MIC_GAIN_GAIN1940ALGNS4_ADDR;
  struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
  struct adau *adau = snd_soc_codec_get_drvdata(codec);

  uint32_t mic_gain_value;

  regmap_raw_read(adau->regmap, extra_mic_gain_addr, &mic_gain_value, 4);
  mic_gain_value = htonl(mic_gain_value);
  ucontrol->value.integer.value[0] = mic_gain_value;
  return 0;

};

// Conversion needs to be done in Firmware!
static int microburst_sigmadsp_compander_input_gain_get(struct snd_kcontrol *kcontrol,
                                                        struct snd_ctl_elem_value *ucontrol)
{
  struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
  struct adau *adau = snd_soc_codec_get_drvdata(codec);

  uint32_t value;

  regmap_raw_read(adau->regmap, MOD_COMPANDER_ALG0_STDPEAKINGCOMPRESSORALG1ATTENUATION_ADDR, &value, 4);
  value = htonl(value);
  ucontrol->value.integer.value[0] = value ; // 20 * log10(value) ! 
  return 0;
};
// Conversion needs to be done in Firmware
static int microburst_sigmadsp_compander_input_gain_put(struct snd_kcontrol *kcontrol,
                                                        struct snd_ctl_elem_value *ucontrol)
{
  struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
  struct adau *adau = snd_soc_codec_get_drvdata(codec);

  uint32_t value[1];

  value[0] = ucontrol->value.integer.value[0]; // Convert by  pow( 10.0, (ucontrol->value.integer.value[0] - 90) / 20.0);
  adau1761_block_write(adau, MOD_COMPANDER_ALG0_STDPEAKINGCOMPRESSORALG1ATTENUATION_ADDR, value, 4);
  return 0;
};

static int microburst_sigmadsp_compander_curve_put(struct  snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	uint32_t expander_index;
	uint32_t value;;

	uint32_t i;

	//uint32_t compressor_array[34];
//	compressor_array[0]  = MICROBURST_SIGMADSP_FIXPT_ONE;
  //value = ucontrol->value.integer.value[0]; // Convert by  pow( 10.0, (ucontrol->value.integer.value[0] - 90) / 20.0);

	// for(i = 0 ; i < 34 ; i++)
	// {
	// 	printk(KERN_DEBUG "Item %d is %x\n", i, ucontrol->value.integer.value[i]);
	// }

//   float level = value / 100;
//   expander_index = 90 + (expander_ratio_level);
//   expander_index = expander_index / 3; // Integer division

//   for ( i = 1 ; i < expander_index ; i++)
//   {
//   	compressor_array[i] = MICROBURST_SIGMADSP_FIXPT_ONE / (i * 2); // Integer division
//   }

//   for ( i = expander_index ; i <= 33 ; i ++ )
//   {
//   	float tmp = 1.0 - exp( level * i );
//   	tmp = tmp * compressor_ratio_level;
//   	tmp = pow(10.0, tmp / 20.0);

//   	tmp = tmp * MICROBURST_SIGMADSP_FIXPT_ONE;
//   	compressor_array[i + (33 - expander_index)] =  tmp / 1; //Integer division
//   }

uint32_t *compressor_array = ucontrol->value.integer.value;

adau1761_safeload_write(adau, MOD_COMPANDER_ALG0_STDPEAKINGCOMPRESSORALG10_ADDR, compressor_array, 20 );
adau1761_safeload_write(adau, MOD_COMPANDER_ALG0_STDPEAKINGCOMPRESSORALG10_ADDR, compressor_array+5, 20 );
adau1761_safeload_write(adau, MOD_COMPANDER_ALG0_STDPEAKINGCOMPRESSORALG10_ADDR, compressor_array+10, 20 );
adau1761_safeload_write(adau, MOD_COMPANDER_ALG0_STDPEAKINGCOMPRESSORALG10_ADDR, compressor_array+15, 20 );
adau1761_safeload_write(adau, MOD_COMPANDER_ALG0_STDPEAKINGCOMPRESSORALG10_ADDR, compressor_array+20, 20 );
adau1761_safeload_write(adau, MOD_COMPANDER_ALG0_STDPEAKINGCOMPRESSORALG10_ADDR, compressor_array+25, 20 );
adau1761_safeload_write(adau, MOD_COMPANDER_ALG0_STDPEAKINGCOMPRESSORALG10_ADDR, compressor_array+30, 12 );

  return 0;	
};

static int microburst_sigmadsp_compander_curve_get(struct snd_kcontrol *kcontrol,
	                                               struct snd_ctl_elem_value *ucontrol)
{
	return 0;
};


// Conversion needs to be done in Firmware!
static int microburst_sigmadsp_compander_post_gain_get(struct snd_kcontrol *kcontrol,
                                                        struct snd_ctl_elem_value *ucontrol)
{
  struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
  struct adau *adau = snd_soc_codec_get_drvdata(codec);

  uint32_t value;

  regmap_raw_read(adau->regmap, MOD_POST_COMP_GAIN_GAIN1940ALGNS1_ADDR, &value, 4);
  value = htonl(value);
  ucontrol->value.integer.value[0] = value ; // 20 * log10(value) ! 
  return 0;
};
// Conversion needs to be done in Firmware
static int microburst_sigmadsp_compander_post_gain_put(struct snd_kcontrol *kcontrol,
                                                        struct snd_ctl_elem_value *ucontrol)
{
  struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
  struct adau *adau = snd_soc_codec_get_drvdata(codec);

  uint32_t value[1];

  value[0] = ucontrol->value.integer.value[0]; // Convert by  pow( 10.0, (ucontrol->value.integer.value[0] - 90) / 20.0);
  adau1761_block_write(adau, MOD_POST_COMP_GAIN_GAIN1940ALGNS1_ADDR, value, 4);
  return 0;
};


static int microburst_sigmadsp_echo_cancel_adapt_get(struct snd_kcontrol *kcontrol,
	                                                struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);

	uint32_t value;

	regmap_raw_read(adau->regmap, MOD_ECHO_CANCEL_ECHO_CANCEL_ADAPT_ISON_ADDR, &value, 4);
	value = htonl(value);

	if(value)
	{
		ucontrol->value.integer.value[0] = 1;
	}
	else
	{
		ucontrol->value.integer.value[0] = 0;
	}
	return 0;
};

static int microburst_sigmadsp_echo_cancel_adapt_put(struct snd_kcontrol *kcontrol,
                                                 	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);


	uint32_t value;
	if(ucontrol->value.integer.value[0])
	{
		value = MICROBURST_SIGMADSP_FIXPT_ONE;
		adau1761_block_write(adau, MOD_ECHO_CANCEL_ECHO_CANCEL_ADAPT_ISON_ADDR,
			&value, 4);
	}
	else
	{
		value = MICROBURST_SIGMADSP_FIXPT_ZERO;
		adau1761_block_write(adau, MOD_ECHO_CANCEL_ECHO_CANCEL_ADAPT_ISON_ADDR,
			&value, 4);
	}

	return 0;
};

static int microburst_sigmadsp_binary_version_get(struct snd_kcontrol *kcontrol,
	                                               struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);

	uint32_t value , ret_val;
	value = 0x9999;
	ret_val = regmap_raw_read(adau->regmap, MOD_CODEC_BINARY_VERSION_DCINPALG1_ADDR, &value, 4);

	value = htonl(value);

//	printk(KERN_DEBUG "Version Readback %d", value);

	ucontrol->value.integer.value[0] = value;

	return 0;
}

static int microburst_sigmadsp_binary_version_put(struct snd_kcontrol *kcontrol,
	                                               struct snd_ctl_elem_value *ucontrol)
{
	// Dummy function
	return 0;
}

static int microburst_sigmadsp_module_version_get(struct snd_kcontrol *kcontrol,
	                                               struct snd_ctl_elem_value *ucontrol)
{
	printk(KERN_DEBUG "Module Version %d", CODEC_MODULE_VERSION);

	ucontrol->value.integer.value[0] = CODEC_MODULE_VERSION;

	return 0;
}

static int microburst_sigmadsp_module_version_put(struct snd_kcontrol *kcontrol,
	                                               struct snd_ctl_elem_value *ucontrol)
{
	// Dummy function
	return 0;
}

static int microburst_sigmadsp_peak_meter_readback_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: peak_meter_readback_get called\n");
	uint32_t readback_address = MOD_MIC_LEVEL_PEAK_READBACKALGSIGMA2001_ADDR;
	uint32_t meter_reading;
	uint32_t meter_reading_inverted;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	//printk (KERN_DEBUG "MB-sigmadsp: reading peak meter addr %d", readback_address);
	regmap_raw_read(adau->regmap, readback_address, &meter_reading_inverted, 4);
	meter_reading = htonl(meter_reading_inverted);
	ucontrol->value.integer.value[0] = meter_reading;
	//printk (KERN_DEBUG "MB-sigmadsp: peak reading value %08X", meter_reading);
	return 0;
};

static int microburst_sigmadsp_peak_meter_readback_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: peak_meter_readback_put called\n");
//	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
//	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	//the put function does not do anything, this is a read-only control
	return 0;
};

static int microburst_sigmadsp_average_meter_readback_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: average_meter_readback_get called\n");
	uint32_t readback_address = MOD_MIC_LEVEL_AVG_READBACKALGSIGMA2002_ADDR;
	uint32_t meter_reading;
	uint32_t meter_reading_inverted;
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct adau *adau = snd_soc_codec_get_drvdata(codec);
	//printk (KERN_DEBUG "MB-sigmadsp: reading average meter addr %d", readback_address);
	regmap_raw_read(adau->regmap, readback_address, &meter_reading_inverted, 4);
	meter_reading = htonl(meter_reading_inverted);
	ucontrol->value.integer.value[0] = meter_reading;
	//printk (KERN_DEBUG "MB-sigmadsp: average reading value %08X", meter_reading);
	return 0;
};

static int microburst_sigmadsp_average_meter_readback_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	//printk (KERN_DEBUG "MB-sigmadsp: average_meter_readback_put called\n");
//	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
//	struct ad*adau = snd_soc_codec_get_drvdata(codec);
	//the put function does not do anything, this is a read-only control
	return 0;
};

/* Microburst SigmaDSP kcontrols */
static const DECLARE_TLV_DB_MINMAX(adau1761_input_gain, 0, 24);

static const struct snd_kcontrol_new microburst_sigmadsp_controls[] = {
		SOC_SINGLE_BOOL_EXT("Microburst SigmaDSP CW Key", 0, microburst_sigmadsp_cw_key_get,
				microburst_sigmadsp_cw_key_put),
		SOC_SINGLE_BOOL_EXT("Microburst SigmaDSP Monitor Voice CW", 1, microburst_sigmadsp_monitor_voice_cw_get,
				microburst_sigmadsp_monitor_voice_cw_put),
		SOC_SINGLE_BOOL_EXT("Microburst SigmaDSP Compander", 1, microburst_sigmadsp_compander_get,
				microburst_sigmadsp_compander_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP Compander Curve", 101, microburst_sigmadsp_compander_curve_get, 
			microburst_sigmadsp_compander_curve_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP Compander Hold", 12000, microburst_sigmadsp_compander_hold_get,
			microburst_sigmadsp_compander_hold_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP Compander Decay", 0x7B89, microburst_sigmadsp_compander_decay_get,
			microburst_sigmadsp_compander_decay_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP Compander Input Gain", 0x00FFFFFF, microburst_sigmadsp_compander_input_gain_get, 
			microburst_sigmadsp_compander_input_gain_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP Compander Post Gain", 0x07FFFFFF, microburst_sigmadsp_compander_post_gain_get,
			microburst_sigmadsp_compander_post_gain_put),
		SOC_SINGLE_BOOL_EXT("Microburst SigmaDSP TX EQ", 0, microburst_sigmadsp_tx_eq_get,
				microburst_sigmadsp_tx_eq_put),
		SOC_SINGLE_BOOL_EXT("Microburst SigmaDSP RX EQ", 0, microburst_sigmadsp_rx_eq_get,
				microburst_sigmadsp_rx_eq_put),
		SOC_SINGLE_BOOL_EXT("Microburst SigmaDSP Meter Select", 0, microburst_sigmadsp_meter_select_get,
				microburst_sigmadsp_meter_select_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP Echo Cancel", 2, microburst_sigmadsp_echo_cancel_get,
				microburst_sigmadsp_echo_cancel_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP Echo Cancel Adapt", 2, microburst_sigmadsp_echo_cancel_adapt_get,
			microburst_sigmadsp_echo_cancel_adapt_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP Input Source", 2, microburst_sigmadsp_input_source_get,
				microburst_sigmadsp_input_source_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP Sig Gen Select", 4, microburst_sigmadsp_sig_gen_select_get,
				microburst_sigmadsp_sig_gen_select_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP Monitor Level", 63, microburst_sigmadsp_monitor_level_get,
				microburst_sigmadsp_monitor_level_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP Sig Gen Level", 63, microburst_sigmadsp_sig_gen_level_get,
				microburst_sigmadsp_sig_gen_level_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP CW Sidetone", 10000, microburst_sigmadsp_cw_sidetone_get,
				microburst_sigmadsp_cw_sidetone_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP TX Filter Bandwidth", 2, microburst_sigmadsp_tx_filter_bw_get,
				microburst_sigmadsp_tx_filter_bw_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP TX EQ Stage 0", 20, microburst_sigmadsp_tx_eq_stage_0_get,
				microburst_sigmadsp_tx_eq_stage_0_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP TX EQ Stage 1", 20, microburst_sigmadsp_tx_eq_stage_1_get,
				microburst_sigmadsp_tx_eq_stage_1_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP TX EQ Stage 2", 20, microburst_sigmadsp_tx_eq_stage_2_get,
				microburst_sigmadsp_tx_eq_stage_2_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP TX EQ Stage 3", 20, microburst_sigmadsp_tx_eq_stage_3_get,
				microburst_sigmadsp_tx_eq_stage_3_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP TX EQ Stage 4", 20, microburst_sigmadsp_tx_eq_stage_4_get,
				microburst_sigmadsp_tx_eq_stage_4_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP TX EQ Stage 5", 20, microburst_sigmadsp_tx_eq_stage_5_get,
				microburst_sigmadsp_tx_eq_stage_5_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP TX EQ Stage 6", 20, microburst_sigmadsp_tx_eq_stage_6_get,
				microburst_sigmadsp_tx_eq_stage_6_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP TX EQ Stage 7", 20, microburst_sigmadsp_tx_eq_stage_7_get,
				microburst_sigmadsp_tx_eq_stage_7_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP RX EQ Stage 0", 20, microburst_sigmadsp_rx_eq_stage_0_get,
				microburst_sigmadsp_rx_eq_stage_0_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP RX EQ Stage 1", 20, microburst_sigmadsp_rx_eq_stage_1_get,
				microburst_sigmadsp_rx_eq_stage_1_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP RX EQ Stage 2", 20, microburst_sigmadsp_rx_eq_stage_2_get,
				microburst_sigmadsp_rx_eq_stage_2_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP RX EQ Stage 3", 20, microburst_sigmadsp_rx_eq_stage_3_get,
				microburst_sigmadsp_rx_eq_stage_3_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP RX EQ Stage 4", 20, microburst_sigmadsp_rx_eq_stage_4_get,
				microburst_sigmadsp_rx_eq_stage_4_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP RX EQ Stage 5", 20, microburst_sigmadsp_rx_eq_stage_5_get,
				microburst_sigmadsp_rx_eq_stage_5_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP RX EQ Stage 6", 20, microburst_sigmadsp_rx_eq_stage_6_get,
				microburst_sigmadsp_rx_eq_stage_6_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP RX EQ Stage 7", 20, microburst_sigmadsp_rx_eq_stage_7_get,
				microburst_sigmadsp_rx_eq_stage_7_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP Peak Meter Readback", 0x00800000, microburst_sigmadsp_peak_meter_readback_get,
				microburst_sigmadsp_peak_meter_readback_put),
		SOC_SINGLE_INT_EXT("Microburst SigmaDSP Average Meter Readback", 0x00800000, microburst_sigmadsp_average_meter_readback_get,
				microburst_sigmadsp_average_meter_readback_put),
		SOC_SINGLE_INT_EXT("Extra Line Input Gain", 0x7FFFFFF, microburst_sigmadsp_extra_line_input_gain_get, 
			microburst_sigmadsp_extra_line_input_gain_put),
		SOC_SINGLE_INT_EXT("Extra Mic Input Gain", 0x7FFFFFF, microburst_sigmadsp_extra_mic_input_gain_get, 
			microburst_sigmadsp_extra_mic_input_gain_put),
		SOC_SINGLE_INT_EXT("Codec Binary Version", 0xFFFFFF, microburst_sigmadsp_binary_version_get,
			microburst_sigmadsp_binary_version_put),
		SOC_SINGLE_INT_EXT("Codec Module Version", 0xFFFFFF, microburst_sigmadsp_module_version_get,
			microburst_sigmadsp_module_version_put),
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

	regcache_sync(adau->regmap);
	/* After any power changes have been made the dejitter circuit
	 * has to be reinitialized. */
        snd_soc_write(codec, ADAU1761_DEJITTER, 0);
	if (!adau->master)
          snd_soc_write(codec, ADAU1761_DEJITTER, 3);

        //        printk(KERN_INFO "adau1761_dejitter_fixup called with event %d", event);
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
  //  printk(KERN_INFO "adau1761_set_bias_level called ");
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
	//struct adau1761_platform_data *pdata = codec->dev->platform_data;

        //	if (pdata)
        //		return pdata->lineout_mode;

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
		snd_soc_update_bits(codec, ADAU1761_PLAY_LINE_LEFT_VOL, 1, 0);
		snd_soc_update_bits(codec, ADAU1761_PLAY_LINE_RIGHT_VOL, 1, 0);
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
			//printk("MB-codecdsp: adding kcontrols for dsp module\n");

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
