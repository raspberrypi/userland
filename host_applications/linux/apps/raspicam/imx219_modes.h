/*
Copyright (c) 2017, Raspberry Pi Foundation
Copyright (c) 2017, Dave Stevenson
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

// These values are copied from https://www.raspberrypi.org/forums/viewtopic.php?f=43&t=162722&p=1052339
// Raspberry Pi can not discuss these settings as we have information from
// Sony under NDA.

// REQUESTS FOR SUPPORT ABOUT THESE REGISTER VALUES WILL
// BE IGNORED BY PI TOWERS.

#ifndef IMX219MODES_H_
#define IMX219MODES_H_

struct sensor_regs imx219_8MPix[] =
{
      /* 3280x2464@15 FPS */
      {0x0100, 0x00},       // 0=OFF, 1=Stream, 2=MAX
      {0x30EB, 0x05},
      {0x30EB, 0x0C},
      {0x300A, 0xFF},
      {0x300B, 0xFF},
      {0x30EB, 0x05},
      {0x30EB, 0x09},
      {0x0114, 0x01},      // CSI MIPI Lanes [1:0]  (0x01=2, 0x03=4)
      {0x0128, 0x00},      // DPHY_CNTRL
      {0x012A, 0x18},      // EXCK_FREQ [15:8]
      {0x012B, 0x00},      // EXCK_FREQ [7:0]
      {0x0157, 0x00},         // Analog Gain
      {0x0158, 0x01},         // Digital Gain [15:8]
      {0x0159, 0x00},          // Digital Gain [7:0]
      {0x015A, 0x01},      // Shutter/Integration Time [15:8]
      {0x015B, 0x00},      // Shutter/Integration Time [7:0]
      {0x0160, 0x09},      // Frame Length [15:8]
      {0x0161, 0xC8},      // Frame Length [7:0]
      {0x0162, 0x0D},      // Line Length [15:8]
      {0x0163, 0x78},      // Line Length [7:0]
      {0x0164, 0x00},
      {0x0165, 0x00},
      {0x0166, 0x0C},
      {0x0167, 0xCF},
      {0x0168, 0x00},
      {0x0169, 0x00},
      {0x016A, 0x09},
      {0x016B, 0x9F},
      {0x016C, 0x0C},
      {0x016D, 0xD0},
      {0x016E, 0x09},
      {0x016F, 0xA0},
      {0x0170, 0x01},      // X_ODD_INC [2:0]
      {0x0171, 0x01},      // Y_ODD_INC [2:0]
      {0x0172, 0x03},
      {0x0174, 0x00},      // Binning Mode H_A
      {0x0175, 0x00},      // Binning Mode V_A
      {0x018C, 0x0A},      // CSI Data Format [15:8]
      {0x018D, 0x0A},      // CSI Data Format [7:0]
      {0x0301, 0x05},      // VTPXCK_DIV
      {0x0303, 0x01},      // VTSYCK_DIV
      {0x0304, 0x03},      // PREPLLCK_VT_DIV [3:0]
      {0x0305, 0x03},      // PREPLLCK_OP_DIV [3:0]
      {0x0306, 0x00},      // PLL_VT_MPY [10:8]
      {0x0307, 0x2B},      // PLL_VT_MPY [7:0]
      {0x0309, 0x0A},      // OPPXCK_DIV [4:0]
      {0x030B, 0x01},      // OPSYCK_DIV
      {0x030C, 0x00},      // PLL_OP_MPY [10:8]
      {0x030D, 0x55},      // PLL_OP_MPY [7:0]
      {0x455E, 0x00},      // CIS Tuning ?
      {0x471E, 0x4B},      // CIS Tuning ?
      {0x4767, 0x0F},      // CIS Tuning ?
      {0x4750, 0x14},      // CIS Tuning ?
      {0x4540, 0x00},      // CIS Tuning ?
      {0x47B4, 0x14},      // CIS Tuning ?
      {0x4713, 0x30},      // CIS Tuning ?
      {0x478B, 0x10},      // CIS Tuning ?
      {0x478F, 0x10},      // CIS Tuning ?
      {0x4797, 0x0E},      // CIS Tuning ?
      {0x479B, 0x0E},      // CIS Tuning ?
      {0x0100, 0x01}
};

struct mode_def imx219_modes[] = {
   { imx219_8MPix, NUM_ELEMENTS(imx219_8MPix), 3280, 2464, 0, BAYER_ORDER_BGGR, 10, 0x2B, 2, 2504, 18904 },
};

//From https://android.googlesource.com/kernel/bcm/+/android-bcm-tetra-3.10-lollipop-wear-release/drivers/media/video/imx219.c
struct sensor_regs imx219_stop[] = {
      /* to power down */
      {0x0100, 0x00},          /* disable streaming  */
};

// ID, exposure, and gain register settings taken from
// https://android.googlesource.com/kernel/bcm/+/android-bcm-tetra-3.10-lollipop-wear-release/drivers/media/video/imx219.c
// Flip settings taken from https://github.com/rellimmot/Sony-IMX219-Raspberry-Pi-V2-CMOS/blob/master/imx219mipiraw_Sensor.c#L585
struct sensor_def imx219 = {
      .name =                 "imx219",
      .modes =                imx219_modes,
      .num_modes =            NUM_ELEMENTS(imx219_modes),
      .stop =                 imx219_stop,
      .num_stop_regs =        NUM_ELEMENTS(imx219_stop),

      .i2c_addr =             0x10,
      .i2c_addressing =       2,
      .i2c_ident_length =     2,
      .i2c_ident_reg =        0x0000,
      .i2c_ident_value =      0x1902,     //Bytes reversed

      .vflip_reg =            0x172,
      .vflip_reg_bit =        0,
      .hflip_reg =            0x172,
      .hflip_reg_bit =        1,

      .exposure_reg =         0x015A,
      .exposure_reg_num_bits = 16,

      .vts_reg =              0x0160,
      .vts_reg_num_bits =     16,

      .gain_reg =             0x0157,
      .gain_reg_num_bits =    8,    //Only valid up to 230.
};

#endif
