/*
Copyright (c) 2012, Broadcom Europe Ltd
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

#ifndef MMAL_ENCODINGS_H
#define MMAL_ENCODINGS_H

#include "mmal_common.h"

/** \defgroup MmalEncodings List of pre-defined encodings
 * This defines a list of common encodings. This list isn't exhaustive and is only
 * provided as a convenience to avoid clients having to use FourCC codes directly.
 * However components are allowed to define and use their own FourCC codes. */
/* @{ */

/** \name Pre-defined video encodings */
/* @{ */
#define MMAL_ENCODING_H264             MMAL_FOURCC('H','2','6','4')
#define MMAL_ENCODING_H263             MMAL_FOURCC('H','2','6','3')
#define MMAL_ENCODING_MP4V             MMAL_FOURCC('M','P','4','V')
#define MMAL_ENCODING_MP2V             MMAL_FOURCC('M','P','2','V')
#define MMAL_ENCODING_MP1V             MMAL_FOURCC('M','P','1','V')
#define MMAL_ENCODING_WMV3             MMAL_FOURCC('W','M','V','3')
#define MMAL_ENCODING_WMV2             MMAL_FOURCC('W','M','V','2')
#define MMAL_ENCODING_WMV1             MMAL_FOURCC('W','M','V','1')
#define MMAL_ENCODING_WVC1             MMAL_FOURCC('W','V','C','1')
#define MMAL_ENCODING_VP8              MMAL_FOURCC('V','P','8',' ')
#define MMAL_ENCODING_VP7              MMAL_FOURCC('V','P','7',' ')
#define MMAL_ENCODING_VP6              MMAL_FOURCC('V','P','6',' ')
#define MMAL_ENCODING_THEORA           MMAL_FOURCC('T','H','E','O')
#define MMAL_ENCODING_SPARK            MMAL_FOURCC('S','P','R','K')

#define MMAL_ENCODING_JPEG             MMAL_FOURCC('J','P','E','G')
#define MMAL_ENCODING_GIF              MMAL_FOURCC('G','I','F',' ')
#define MMAL_ENCODING_PNG              MMAL_FOURCC('P','N','G',' ')
#define MMAL_ENCODING_PPM              MMAL_FOURCC('P','P','M',' ')
#define MMAL_ENCODING_TGA              MMAL_FOURCC('T','G','A',' ')
#define MMAL_ENCODING_BMP              MMAL_FOURCC('B','M','P',' ')

#define MMAL_ENCODING_I420             MMAL_FOURCC('I','4','2','0')
#define MMAL_ENCODING_I422             MMAL_FOURCC('I','4','2','2')
#define MMAL_ENCODING_NV21             MMAL_FOURCC('N','V','2','1')
#define MMAL_ENCODING_ARGB             MMAL_FOURCC('A','R','G','B')
#define MMAL_ENCODING_RGBA             MMAL_FOURCC('R','G','B','A')
#define MMAL_ENCODING_ABGR             MMAL_FOURCC('A','B','G','R')
#define MMAL_ENCODING_BGRA             MMAL_FOURCC('B','G','R','A')
#define MMAL_ENCODING_RGB16            MMAL_FOURCC('R','G','B','2')
#define MMAL_ENCODING_RGB24            MMAL_FOURCC('R','G','B','3')
#define MMAL_ENCODING_RGB32            MMAL_FOURCC('R','G','B','4')
#define MMAL_ENCODING_BGR16            MMAL_FOURCC('B','G','R','2')
#define MMAL_ENCODING_BGR24            MMAL_FOURCC('B','G','R','3')
#define MMAL_ENCODING_BGR32            MMAL_FOURCC('B','G','R','4')

/** SAND Video (YUVUV128) format, native format understood by VideoCore.
 * This format is *not* opaque - if requested you will receive full frames
 * of YUV_UV video.
 */
#define MMAL_ENCODING_YUVUV128         MMAL_FOURCC('S','A','N','D')

/** VideoCore opaque image format, image handles are returned to
 * the host but not the actual image data.
 */
#define MMAL_ENCODING_OPAQUE           MMAL_FOURCC('O','P','Q','V')

/** An EGL image handle
 */
#define MMAL_ENCODING_EGL_IMAGE        MMAL_FOURCC('E','G','L','I')

/* }@ */

/** \name Pre-defined audio encodings */
/* @{ */
#define MMAL_ENCODING_MP4A             MMAL_FOURCC('M','P','4','A')
/* @} */

/* @} */

#endif /* MMAL_ENCODINGS_H */
