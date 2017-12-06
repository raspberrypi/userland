/*
Copyright (c) 2017, Raspberry Pi Trading
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

#ifndef RAWHEADER_H_
#define RAWHEADER_H_

#include "vc_image_types.h"

#define BRCM_ID_SIG 0x4D435242 /* 'BRCM' */
#define HEADER_VERSION 111

#define BRCM_RAW_HEADER_LENGTH 32768 // 1024

struct brcm_camera_mode {
 uint8_t name[32];
 uint16_t width;
 uint16_t height;
 uint16_t padding_right;
 uint16_t padding_down;
 uint32_t dummy[6];
 uint16_t transform;
 uint16_t format;
 uint8_t bayer_order;
 uint8_t bayer_format;
};


struct brcm_raw_header {
   uint32_t id;               // Must be set to "BRCM"
   uint32_t version;          // Header version id
   uint32_t offset;           // Offset to the image data
   uint32_t preamble_pad;     // pad to address 16

   // offset 16 0x10
   uint8_t block1[160];       // 160 bytes long.

   // offset 176 0xB0
   struct brcm_camera_mode mode;
   uint8_t  cam_mode_pad[16 - (sizeof(struct brcm_camera_mode) & 15)];
};

#endif
