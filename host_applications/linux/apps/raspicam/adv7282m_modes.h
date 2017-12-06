/*
Copyright (c) 2015-2017, Raspberry Pi Foundation
Copyright (c) 2015, Dave Stevenson
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


// These register settings were as logged off the line
// by jbeale. There is a datasheet for OV5647 floating
// about on the internet, but the Pi Foundation/Trading have
// information from Omnivision under NDA, therefore
// we can not offer support on this.
// There is some information/discussion on the Freescale
// i.MX6 forums about supporting OV5647 on that board.
// There may be information available there that is of use.
//
// REQUESTS FOR SUPPORT ABOUT THESE REGISTER VALUES WILL
// BE IGNORED.

#ifndef ADV7282M_MODES_H_
#define ADV7282M_MODES_H_

// Setup taken from the Linux kernel drivers/media/i2c/adv7180.c driver
// settings for ADV7282-M.

#define ADV7180_STD_AD_PAL_BG_NTSC_J_SECAM      0x0
#define ADV7180_STD_AD_PAL_BG_NTSC_J_SECAM_PED     0x1
#define ADV7180_STD_AD_PAL_N_NTSC_J_SECAM    0x2
#define ADV7180_STD_AD_PAL_N_NTSC_M_SECAM    0x3
#define ADV7180_STD_NTSC_J          0x4
#define ADV7180_STD_NTSC_M          0x5
#define ADV7180_STD_PAL60           0x6
#define ADV7180_STD_NTSC_443           0x7
#define ADV7180_STD_PAL_BG          0x8
#define ADV7180_STD_PAL_N           0x9
#define ADV7180_STD_PAL_M           0xa
#define ADV7180_STD_PAL_M_PED          0xb
#define ADV7180_STD_PAL_COMB_N            0xc
#define ADV7180_STD_PAL_COMB_N_PED        0xd
#define ADV7180_STD_PAL_SECAM          0xe
#define ADV7180_STD_PAL_SECAM_PED         0xf

#define ADV7180_REG_INPUT_CONTROL         0x0000
#define ADV7180_INPUT_CONTROL_INSEL_MASK     0x0f

#define ADV7182_REG_INPUT_VIDSEL       0x0002

#define ADV7180_REG_OUTPUT_CONTROL        0x0003
#define ADV7180_REG_EXTENDED_OUTPUT_CONTROL     0x0004
#define ADV7180_EXTENDED_OUTPUT_CONTROL_NTSCDIS    0xC5

#define ADV7180_REG_AUTODETECT_ENABLE        0x0007
#define ADV7180_AUTODETECT_DEFAULT        0x7f
/* Contrast */
#define ADV7180_REG_CON    0x0008   /*Unsigned */
#define ADV7180_CON_MIN    0
#define ADV7180_CON_DEF    128
#define ADV7180_CON_MAX    255
/* Brightness*/
#define ADV7180_REG_BRI    0x000a   /*Signed */
#define ADV7180_BRI_MIN    -128
#define ADV7180_BRI_DEF    0
#define ADV7180_BRI_MAX    127
/* Hue */
#define ADV7180_REG_HUE    0x000b   /*Signed, inverted */
#define ADV7180_HUE_MIN    -127
#define ADV7180_HUE_DEF    0
#define ADV7180_HUE_MAX    128

#define ADV7180_REG_CTRL      0x000e
#define ADV7180_CTRL_IRQ_SPACE      0x20

#define ADV7180_REG_PWR_MAN      0x0f
#define ADV7180_PWR_MAN_ON    0x04
#define ADV7180_PWR_MAN_OFF      0x24
#define ADV7180_PWR_MAN_RES      0x80

#define ADV7180_REG_STATUS1      0x0010
#define ADV7180_STATUS1_IN_LOCK     0x01
#define ADV7180_STATUS1_AUTOD_MASK  0x70
#define ADV7180_STATUS1_AUTOD_NTSM_M_J 0x00
#define ADV7180_STATUS1_AUTOD_NTSC_4_43 0x10
#define ADV7180_STATUS1_AUTOD_PAL_M 0x20
#define ADV7180_STATUS1_AUTOD_PAL_60   0x30
#define ADV7180_STATUS1_AUTOD_PAL_B_G  0x40
#define ADV7180_STATUS1_AUTOD_SECAM 0x50
#define ADV7180_STATUS1_AUTOD_PAL_COMB 0x60
#define ADV7180_STATUS1_AUTOD_SECAM_525   0x70

#define ADV7180_REG_IDENT 0x0011
#define ADV7180_ID_7180 0x18

#define ADV7180_REG_STATUS3      0x0013
#define ADV7180_REG_ANALOG_CLAMP_CTL   0x0014
#define ADV7180_REG_SHAP_FILTER_CTL_1  0x0017
#define ADV7180_REG_CTRL_2    0x001d
#define ADV7180_REG_VSYNC_FIELD_CTL_1  0x0031
#define ADV7180_REG_MANUAL_WIN_CTL_1   0x003d
#define ADV7180_REG_MANUAL_WIN_CTL_2   0x003e
#define ADV7180_REG_MANUAL_WIN_CTL_3   0x003f
#define ADV7180_REG_LOCK_CNT     0x0051
#define ADV7180_REG_CVBS_TRIM    0x0052
#define ADV7180_REG_CLAMP_ADJ    0x005a
#define ADV7180_REG_RES_CIR      0x005f
#define ADV7180_REG_DIFF_MODE    0x0060

#define ADV7180_REG_ICONF1    0x2040
#define ADV7180_ICONF1_ACTIVE_LOW   0x01
#define ADV7180_ICONF1_PSYNC_ONLY   0x10
#define ADV7180_ICONF1_ACTIVE_TO_CLR   0xC0
/* Saturation */
#define ADV7180_REG_SD_SAT_CB 0x00e3   /*Unsigned */
#define ADV7180_REG_SD_SAT_CR 0x00e4   /*Unsigned */
#define ADV7180_SAT_MIN    0
#define ADV7180_SAT_DEF    128
#define ADV7180_SAT_MAX    255

#define ADV7180_IRQ1_LOCK  0x01
#define ADV7180_IRQ1_UNLOCK   0x02
#define ADV7180_REG_ISR1   0x2042
#define ADV7180_REG_ICR1   0x2043
#define ADV7180_REG_IMR1   0x2044
#define ADV7180_REG_IMR2   0x2048
#define ADV7180_IRQ3_AD_CHANGE   0x08
#define ADV7180_REG_ISR3   0x204A
#define ADV7180_REG_ICR3   0x204B
#define ADV7180_REG_IMR3   0x204C
#define ADV7180_REG_IMR4   0x2050

#define ADV7180_REG_NTSC_V_BIT_END  0x00E6
#define ADV7180_NTSC_V_BIT_END_MANUAL_NVEND  0x4F

#define ADV7180_REG_VPP_SLAVE_ADDR  0xFD
#define ADV7180_REG_CSI_SLAVE_ADDR  0xFE

#define ADV7180_REG_ACE_CTRL1    0x4080
#define ADV7180_REG_ACE_CTRL5    0x4084
#define ADV7180_REG_FLCONTROL    0x40e0
#define ADV7180_FLCONTROL_FL_ENABLE 0x1

#define ADV7180_REG_RST_CLAMP 0x809c
#define ADV7180_REG_AGC_ADJ1  0x80b6
#define ADV7180_REG_AGC_ADJ2  0x80c0

#define ADV7180_CSI_REG_PWRDN 0x00
#define ADV7180_CSI_PWRDN  0x80

#define ADV7180_INPUT_CVBS_AIN1 0x00
#define ADV7180_INPUT_CVBS_AIN2 0x01
#define ADV7180_INPUT_CVBS_AIN3 0x02
#define ADV7180_INPUT_CVBS_AIN4 0x03
#define ADV7180_INPUT_CVBS_AIN5 0x04
#define ADV7180_INPUT_CVBS_AIN6 0x05
#define ADV7180_INPUT_SVIDEO_AIN1_AIN2 0x06
#define ADV7180_INPUT_SVIDEO_AIN3_AIN4 0x07
#define ADV7180_INPUT_SVIDEO_AIN5_AIN6 0x08
#define ADV7180_INPUT_YPRPB_AIN1_AIN2_AIN3 0x09
#define ADV7180_INPUT_YPRPB_AIN4_AIN5_AIN6 0x0a

#define ADV7182_INPUT_CVBS_AIN1 0x00
#define ADV7182_INPUT_CVBS_AIN2 0x01
#define ADV7182_INPUT_CVBS_AIN3 0x02
#define ADV7182_INPUT_CVBS_AIN4 0x03
#define ADV7182_INPUT_CVBS_AIN5 0x04
#define ADV7182_INPUT_CVBS_AIN6 0x05
#define ADV7182_INPUT_CVBS_AIN7 0x06
#define ADV7182_INPUT_CVBS_AIN8 0x07
#define ADV7182_INPUT_SVIDEO_AIN1_AIN2 0x08
#define ADV7182_INPUT_SVIDEO_AIN3_AIN4 0x09
#define ADV7182_INPUT_SVIDEO_AIN5_AIN6 0x0a
#define ADV7182_INPUT_SVIDEO_AIN7_AIN8 0x0b
#define ADV7182_INPUT_YPRPB_AIN1_AIN2_AIN3 0x0c
#define ADV7182_INPUT_YPRPB_AIN4_AIN5_AIN6 0x0d
#define ADV7182_INPUT_DIFF_CVBS_AIN1_AIN2 0x0e
#define ADV7182_INPUT_DIFF_CVBS_AIN3_AIN4 0x0f
#define ADV7182_INPUT_DIFF_CVBS_AIN5_AIN6 0x10
#define ADV7182_INPUT_DIFF_CVBS_AIN7_AIN8 0x11

#define ADV7180_DEFAULT_BASE_I2C_ADDR 0x21
#define ADV7180_DEFAULT_CSI_I2C_ADDR 0x44
#define ADV7180_DEFAULT_VPP_I2C_ADDR 0x42

#define V4L2_CID_ADV_FAST_SWITCH (V4L2_CID_USER_ADV7180_BASE + 0x00)

#define adv7180_write(STT, reg, dat)  {reg, dat}
#define adv7180_csi_write(STT, reg, dat)  {reg, dat}
#define adv7180_vpp_write(STT, reg, dat)  {reg, dat}

#define SET_BASE  {0xFFFF, ADV7180_DEFAULT_BASE_I2C_ADDR}
#define SET_CSI   {0xFFFF, ADV7180_DEFAULT_CSI_I2C_ADDR}
#define SET_VPP   {0xFFFF, ADV7180_DEFAULT_VPP_I2C_ADDR}

struct sensor_regs adv7282_pal[] =
{
   //init_device
   adv7180_write(state, ADV7180_REG_PWR_MAN, ADV7180_PWR_MAN_RES),
   {0xFFFE, 100},

   //adv7182_init
   adv7180_write(state, ADV7180_REG_CSI_SLAVE_ADDR,
   ADV7180_DEFAULT_CSI_I2C_ADDR << 1),

   adv7180_write(state, ADV7180_REG_VPP_SLAVE_ADDR,
   ADV7180_DEFAULT_VPP_I2C_ADDR << 1),

   /* ADI recommended writes for improved video quality */
   adv7180_write(state, 0x0080, 0x51),
   adv7180_write(state, 0x0081, 0x51),
   adv7180_write(state, 0x0082, 0x68),

   /* ADI required writes */
   adv7180_write(state, ADV7180_REG_OUTPUT_CONTROL, 0x4e),
   adv7180_write(state, ADV7180_REG_EXTENDED_OUTPUT_CONTROL, 0x57),
   adv7180_write(state, ADV7180_REG_CTRL_2, 0xc0),

   adv7180_write(state, 0x0013, 0x00),

   //adv7182_set_std
   adv7180_write(state, ADV7182_REG_INPUT_VIDSEL, ADV7180_STD_PAL_BG << 4),


   //adv7180_set_field_mode
   SET_CSI,
   adv7180_csi_write(state, 0x01, 0x20),
   adv7180_csi_write(state, 0x02, 0x28),
   adv7180_csi_write(state, 0x03, 0x38),
   adv7180_csi_write(state, 0x04, 0x30),
   adv7180_csi_write(state, 0x05, 0x30),
   adv7180_csi_write(state, 0x06, 0x80),
   adv7180_csi_write(state, 0x07, 0x70),
   adv7180_csi_write(state, 0x08, 0x50),
   SET_VPP,
   adv7180_vpp_write(state, 0xa3, 0x00),
   adv7180_vpp_write(state, 0x5b, 0x00),
   adv7180_vpp_write(state, 0x55, 0x80),

   //s_input
   SET_BASE,
   adv7180_write(state, ADV7180_REG_INPUT_CONTROL, ADV7180_INPUT_CVBS_AIN1),

   /* Reset clamp circuitry - ADI recommended writes */
   adv7180_write(state, ADV7180_REG_RST_CLAMP, 0x00),
   adv7180_write(state, ADV7180_REG_RST_CLAMP, 0xff),

   //input_type = adv7182_get_input_type(input),

   //switch (input_type) {
   //case ADV7182_INPUT_TYPE_CVBS:
   //case ADV7182_INPUT_TYPE_DIFF_CVBS:
   /* ADI recommends to use the SH1 filter */
   adv7180_write(state, ADV7180_REG_SHAP_FILTER_CTL_1, 0x41),
   //break,
   //default:
   //adv7180_write(state, ADV7180_REG_SHAP_FILTER_CTL_1, 0x01),
   //break,
  // }


   //[ADV7182_INPUT_TYPE_CVBS] = { 0xCD, 0x4E, 0x80 },
   //for (i = 0, i < ARRAY_SIZE(adv7182_lbias_settings[0]), i++)
   //adv7180_write(state, ADV7180_REG_CVBS_TRIM + i, lbias[i]),
   adv7180_write(state, ADV7180_REG_CVBS_TRIM + 0, 0xCD),
   adv7180_write(state, ADV7180_REG_CVBS_TRIM + 1, 0x4E),
   adv7180_write(state, ADV7180_REG_CVBS_TRIM + 2, 0x80),

   //if (input_type == ADV7182_INPUT_TYPE_DIFF_CVBS) {
   /* ADI required writes to make differential CVBS work */
   adv7180_write(state, ADV7180_REG_RES_CIR, 0xa8),
   adv7180_write(state, ADV7180_REG_CLAMP_ADJ, 0x90),
   adv7180_write(state, ADV7180_REG_DIFF_MODE, 0xb0),
   adv7180_write(state, ADV7180_REG_AGC_ADJ1, 0x08),
   adv7180_write(state, ADV7180_REG_AGC_ADJ2, 0xa0),

   //adv7180_set_power
   adv7180_write(state, ADV7180_REG_PWR_MAN, ADV7180_PWR_MAN_ON),

   SET_CSI,
   adv7180_csi_write(state, 0xDE, 0x02),
   adv7180_csi_write(state, 0xD2, 0xF7),
   adv7180_csi_write(state, 0xD8, 0x65),
   adv7180_csi_write(state, 0xE0, 0x09),
   adv7180_csi_write(state, 0x2C, 0x00),
   adv7180_csi_write(state, 0x1D, 0x80),
   adv7180_csi_write(state, 0x00, 0x00),
   SET_BASE,
};

struct sensor_regs adv7282_ntsc[] =
{
};

struct mode_def adv7282_modes[] = {
   { adv7282_pal,  NUM_ELEMENTS(adv7282_pal),  720, 576, MMAL_ENCODING_UYVY, 0, 0, 0x1E, 1 },
   { adv7282_ntsc, NUM_ELEMENTS(adv7282_ntsc), 720, 480, MMAL_ENCODING_UYVY, 0, 0, 0x1E, 1 },
};

#undef addreg

struct sensor_regs adv7282_stop[] = {
   //adv7180_set_power
   SET_BASE,
   adv7180_write(state, ADV7180_REG_PWR_MAN, ADV7180_PWR_MAN_OFF),
   SET_CSI,
   adv7180_csi_write(state, 0x00, 0x80),
};

// ID register settings taken from http://www.mail-archive.com/linux-kernel@vger.kernel.org/msg1298623.html
struct sensor_def adv7282 = {
   .name =                 "adv7282",
   .modes =                adv7282_modes,
   .num_modes =            NUM_ELEMENTS(adv7282_modes),
   .stop =                 adv7282_stop,
   .num_stop_regs =        NUM_ELEMENTS(adv7282_stop),

   .i2c_addr =             ADV7180_DEFAULT_BASE_I2C_ADDR,
   .i2c_addressing =       1,
   .i2c_ident_length =     1,
   .i2c_ident_reg =        0x1100,
   .i2c_ident_value =      0x42,

   .vflip_reg =            0,
   .vflip_reg_bit =        0,
   .hflip_reg =            0,
   .hflip_reg_bit =        0,

   .exposure_reg =         0,
   .exposure_reg_num_bits = 0,

   .gain_reg =             0,
   .gain_reg_num_bits =    0,
};

#endif
