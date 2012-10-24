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

#include "mmal.h"
#include "util/mmal_il.h"
#include "interface/vmcs_host/khronos/IL/OMX_Broadcom.h"

/*****************************************************************************/
static struct {
   MMAL_STATUS_T mmal;
   OMX_ERRORTYPE omx;
} mmal_omx_error[] =
{
   {MMAL_SUCCESS, OMX_ErrorNone},
   {MMAL_ENOMEM, OMX_ErrorInsufficientResources},
   {MMAL_ENOSPC, OMX_ErrorInsufficientResources},
   {MMAL_EINVAL, OMX_ErrorBadParameter},
   {MMAL_ENOSYS, OMX_ErrorNotImplemented},
   {(MMAL_STATUS_T)-1, OMX_ErrorUndefined},
};

OMX_ERRORTYPE mmalil_error_to_omx(MMAL_STATUS_T status)
{
   unsigned int i;
   for(i = 0; mmal_omx_error[i].mmal != (MMAL_STATUS_T)-1; i++)
      if(mmal_omx_error[i].mmal == status) break;
   return mmal_omx_error[i].omx;
}

MMAL_STATUS_T mmalil_error_to_mmal(OMX_ERRORTYPE error)
{
   unsigned int i;
   for(i = 0; mmal_omx_error[i].mmal != (MMAL_STATUS_T)-1; i++)
      if(mmal_omx_error[i].omx == error) break;
   return mmal_omx_error[i].mmal;
}

/*****************************************************************************/
OMX_U32 mmalil_buffer_flags_to_omx(uint32_t flags)
{
   OMX_U32 omx_flags = 0;

   if(flags & MMAL_BUFFER_HEADER_FLAG_KEYFRAME)
      omx_flags |= OMX_BUFFERFLAG_SYNCFRAME;
   if(flags & MMAL_BUFFER_HEADER_FLAG_FRAME_END)
      omx_flags |= OMX_BUFFERFLAG_ENDOFFRAME;
   if(flags & MMAL_BUFFER_HEADER_FLAG_EOS)
      omx_flags |= OMX_BUFFERFLAG_EOS;
   if(flags & MMAL_BUFFER_HEADER_FLAG_CONFIG)
      omx_flags |= OMX_BUFFERFLAG_CODECCONFIG;
   if(flags & MMAL_BUFFER_HEADER_FLAG_DISCONTINUITY)
      omx_flags |= OMX_BUFFERFLAG_DISCONTINUITY;
   if (flags & MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO)
      omx_flags |= OMX_BUFFERFLAG_CODECSIDEINFO;
   if (flags & MMAL_BUFFER_HEADER_FLAGS_SNAPSHOT)
      omx_flags |= OMX_BUFFERFLAG_CAPTURE_PREVIEW;

   return omx_flags;
}

uint32_t mmalil_buffer_flags_to_mmal(OMX_U32 flags)
{
   uint32_t mmal_flags = 0;

   if (flags & OMX_BUFFERFLAG_SYNCFRAME)
      mmal_flags |= MMAL_BUFFER_HEADER_FLAG_KEYFRAME;
   if (flags & OMX_BUFFERFLAG_ENDOFFRAME)
      mmal_flags |= MMAL_BUFFER_HEADER_FLAG_FRAME_END;
   if (flags & OMX_BUFFERFLAG_EOS)
      mmal_flags |= MMAL_BUFFER_HEADER_FLAG_EOS;
   if (flags & OMX_BUFFERFLAG_CODECCONFIG)
      mmal_flags |= MMAL_BUFFER_HEADER_FLAG_CONFIG;
   if (flags & OMX_BUFFERFLAG_DISCONTINUITY)
      mmal_flags |= MMAL_BUFFER_HEADER_FLAG_DISCONTINUITY;
   if (flags & OMX_BUFFERFLAG_CODECSIDEINFO)
      mmal_flags |= MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO;
   if (flags & OMX_BUFFERFLAG_CAPTURE_PREVIEW)
      mmal_flags |= MMAL_BUFFER_HEADER_FLAGS_SNAPSHOT;

   return mmal_flags;
}

/*****************************************************************************/
void mmalil_buffer_header_to_omx(OMX_BUFFERHEADERTYPE *omx, MMAL_BUFFER_HEADER_T *mmal)
{
   omx->pBuffer = mmal->data;
   omx->nAllocLen = mmal->alloc_size;
   omx->nFilledLen = mmal->length;
   omx->nOffset = mmal->offset;
   omx->nFlags = mmalil_buffer_flags_to_omx(mmal->flags);
   omx->nTimeStamp = omx_ticks_from_s64(mmal->pts);
   if (mmal->pts == MMAL_TIME_UNKNOWN)
   {
      omx->nFlags |= OMX_BUFFERFLAG_TIME_UNKNOWN;
      omx->nTimeStamp = omx_ticks_from_s64(0);
   }
}

void mmalil_buffer_header_to_mmal(MMAL_BUFFER_HEADER_T *mmal, OMX_BUFFERHEADERTYPE *omx)
{
   mmal->cmd = 0;
   mmal->data = omx->pBuffer;
   mmal->alloc_size = omx->nAllocLen;
   mmal->length = omx->nFilledLen;
   mmal->offset = omx->nOffset;
   mmal->pts = omx_ticks_to_s64(omx->nTimeStamp);
   if (omx->nFlags & OMX_BUFFERFLAG_TIME_UNKNOWN)
      mmal->pts = MMAL_TIME_UNKNOWN;
   mmal->dts = MMAL_TIME_UNKNOWN;
   mmal->flags = mmalil_buffer_flags_to_mmal(omx->nFlags);
}

/*****************************************************************************/
static struct {
   MMAL_ES_TYPE_T type;
   OMX_PORTDOMAINTYPE domain;
} mmal_omx_es_type_table[] =
{
   {MMAL_ES_TYPE_VIDEO,           OMX_PortDomainVideo},
   {MMAL_ES_TYPE_VIDEO,           OMX_PortDomainImage},
   {MMAL_ES_TYPE_AUDIO,           OMX_PortDomainAudio},
   {MMAL_ES_TYPE_UNKNOWN,         OMX_PortDomainMax}
};

OMX_PORTDOMAINTYPE mmalil_es_type_to_omx_domain(MMAL_ES_TYPE_T type)
{
   unsigned int i;
   for(i = 0; mmal_omx_es_type_table[i].type != MMAL_ES_TYPE_UNKNOWN; i++)
      if(mmal_omx_es_type_table[i].type == type) break;
   return mmal_omx_es_type_table[i].domain;
}

MMAL_ES_TYPE_T mmalil_omx_domain_to_es_type(OMX_PORTDOMAINTYPE domain)
{
   unsigned int i;
   for(i = 0; mmal_omx_es_type_table[i].type != MMAL_ES_TYPE_UNKNOWN; i++)
      if(mmal_omx_es_type_table[i].domain == domain) break;
   return mmal_omx_es_type_table[i].type;
}

/*****************************************************************************/
static struct {
   uint32_t encoding;
   OMX_AUDIO_CODINGTYPE coding;
} mmal_omx_audio_coding_table[] =
{
   {MMAL_ENCODING_MP4A,           OMX_AUDIO_CodingAAC},
   {MMAL_ENCODING_UNKNOWN,        OMX_AUDIO_CodingUnused}
};

uint32_t mmalil_omx_audio_coding_to_encoding(OMX_AUDIO_CODINGTYPE coding)
{
   unsigned int i;
   for(i = 0; mmal_omx_audio_coding_table[i].encoding != MMAL_ENCODING_UNKNOWN; i++)
      if(mmal_omx_audio_coding_table[i].coding == coding) break;
   return mmal_omx_audio_coding_table[i].encoding;
}

OMX_VIDEO_CODINGTYPE mmalil_encoding_to_omx_audio_coding(uint32_t encoding)
{
   unsigned int i;
   for(i = 0; mmal_omx_audio_coding_table[i].encoding != MMAL_ENCODING_UNKNOWN; i++)
      if(mmal_omx_audio_coding_table[i].encoding == encoding) break;
   return mmal_omx_audio_coding_table[i].coding;
}

/*****************************************************************************/
static struct {
   uint32_t encoding;
   OMX_VIDEO_CODINGTYPE coding;
} mmal_omx_video_coding_table[] =
{
   {MMAL_ENCODING_H264,           OMX_VIDEO_CodingAVC},
   {MMAL_ENCODING_MP4V,           OMX_VIDEO_CodingMPEG4},
   {MMAL_ENCODING_MP2V,           OMX_VIDEO_CodingMPEG2},
   {MMAL_ENCODING_MP1V,           OMX_VIDEO_CodingMPEG2},
   {MMAL_ENCODING_H263,           OMX_VIDEO_CodingH263},
   {MMAL_ENCODING_WMV3,           OMX_VIDEO_CodingWMV},
   {MMAL_ENCODING_WMV2,           OMX_VIDEO_CodingWMV},
   {MMAL_ENCODING_WMV1,           OMX_VIDEO_CodingWMV},
   {MMAL_ENCODING_WVC1,           OMX_VIDEO_CodingWMV},
   {MMAL_ENCODING_VP6,            OMX_VIDEO_CodingVP6},
   {MMAL_ENCODING_VP7,            OMX_VIDEO_CodingVP7},
   {MMAL_ENCODING_VP8,            OMX_VIDEO_CodingVP8},
   {MMAL_ENCODING_SPARK,          OMX_VIDEO_CodingSorenson},
   {MMAL_ENCODING_THEORA,         OMX_VIDEO_CodingTheora},
   {MMAL_ENCODING_UNKNOWN,        OMX_VIDEO_CodingUnused}
};

uint32_t mmalil_omx_video_coding_to_encoding(OMX_VIDEO_CODINGTYPE coding)
{
   unsigned int i;
   for(i = 0; mmal_omx_video_coding_table[i].encoding != MMAL_ENCODING_UNKNOWN; i++)
      if(mmal_omx_video_coding_table[i].coding == coding) break;
   return mmal_omx_video_coding_table[i].encoding;
}

OMX_VIDEO_CODINGTYPE mmalil_encoding_to_omx_video_coding(uint32_t encoding)
{
   unsigned int i;
   for(i = 0; mmal_omx_video_coding_table[i].encoding != MMAL_ENCODING_UNKNOWN; i++)
      if(mmal_omx_video_coding_table[i].encoding == encoding) break;
   return mmal_omx_video_coding_table[i].coding;
}

/*****************************************************************************/
static struct {
   uint32_t encoding;
   OMX_IMAGE_CODINGTYPE coding;
} mmal_omx_image_coding_table[] =
{
   {MMAL_ENCODING_JPEG,           OMX_IMAGE_CodingJPEG},
   {MMAL_ENCODING_GIF,            OMX_IMAGE_CodingGIF},
   {MMAL_ENCODING_PNG,            OMX_IMAGE_CodingPNG},
   {MMAL_ENCODING_BMP,            OMX_IMAGE_CodingBMP},
   {MMAL_ENCODING_TGA,            OMX_IMAGE_CodingTGA},
   {MMAL_ENCODING_PPM,            OMX_IMAGE_CodingPPM},
   {MMAL_ENCODING_UNKNOWN,        OMX_IMAGE_CodingUnused}
};

uint32_t mmalil_omx_image_coding_to_encoding(OMX_IMAGE_CODINGTYPE coding)
{
   unsigned int i;
   for(i = 0; mmal_omx_image_coding_table[i].encoding != MMAL_ENCODING_UNKNOWN; i++)
      if(mmal_omx_image_coding_table[i].coding == coding) break;
   return mmal_omx_image_coding_table[i].encoding;
}

OMX_IMAGE_CODINGTYPE mmalil_encoding_to_omx_image_coding(uint32_t encoding)
{
   unsigned int i;
   for(i = 0; mmal_omx_image_coding_table[i].encoding != MMAL_ENCODING_UNKNOWN; i++)
      if(mmal_omx_image_coding_table[i].encoding == encoding) break;
   return mmal_omx_image_coding_table[i].coding;
}

uint32_t mmalil_omx_coding_to_encoding(uint32_t encoding, OMX_PORTDOMAINTYPE domain)
{
   if(domain == OMX_PortDomainVideo)
      return mmalil_omx_video_coding_to_encoding((OMX_VIDEO_CODINGTYPE)encoding);
   else if(domain == OMX_PortDomainAudio)
      return mmalil_omx_audio_coding_to_encoding((OMX_AUDIO_CODINGTYPE)encoding);
   else if(domain == OMX_PortDomainImage)
      return mmalil_omx_image_coding_to_encoding((OMX_IMAGE_CODINGTYPE)encoding);
   else
      return MMAL_ENCODING_UNKNOWN;
}

/*****************************************************************************/
static struct {
   uint32_t encoding;
   OMX_COLOR_FORMATTYPE coding;
} mmal_omx_colorformat_coding_table[] =
{
   {MMAL_ENCODING_I420,           OMX_COLOR_FormatYUV420PackedPlanar},
   {MMAL_ENCODING_I422,           OMX_COLOR_FormatYUV422PackedPlanar},
   {MMAL_ENCODING_I420,           OMX_COLOR_FormatYUV420Planar},
   {MMAL_ENCODING_NV21,           OMX_COLOR_FormatYUV420PackedSemiPlanar},
   {MMAL_ENCODING_NV21,           OMX_COLOR_FormatYUV420SemiPlanar},
   {MMAL_ENCODING_YUVUV128,       OMX_COLOR_FormatYUVUV128},
   {MMAL_ENCODING_RGB16,          OMX_COLOR_Format16bitRGB565},
   {MMAL_ENCODING_BGR24,          OMX_COLOR_Format24bitRGB888},
   {MMAL_ENCODING_BGRA,           OMX_COLOR_Format32bitARGB8888},
   {MMAL_ENCODING_BGR16,          OMX_COLOR_Format16bitBGR565},
   {MMAL_ENCODING_RGB24,          OMX_COLOR_Format24bitBGR888},
   {MMAL_ENCODING_ARGB,           OMX_COLOR_Format32bitBGRA8888},
   {MMAL_ENCODING_RGBA,           OMX_COLOR_Format32bitABGR8888},
   {MMAL_ENCODING_EGL_IMAGE,      OMX_COLOR_FormatBRCMEGL},
   {MMAL_ENCODING_UNKNOWN,        OMX_COLOR_FormatUnused}
};

uint32_t mmalil_omx_color_format_to_encoding(OMX_COLOR_FORMATTYPE coding)
{
   unsigned int i;
   for(i = 0; mmal_omx_colorformat_coding_table[i].encoding != MMAL_ENCODING_UNKNOWN; i++)
      if(mmal_omx_colorformat_coding_table[i].coding == coding) break;
   return mmal_omx_colorformat_coding_table[i].encoding;
}

OMX_COLOR_FORMATTYPE mmalil_encoding_to_omx_color_format(uint32_t encoding)
{
   unsigned int i;
   for(i = 0; mmal_omx_colorformat_coding_table[i].encoding != MMAL_ENCODING_UNKNOWN; i++)
      if(mmal_omx_colorformat_coding_table[i].encoding == encoding) break;
   return mmal_omx_colorformat_coding_table[i].coding;
}

/*****************************************************************************/
static struct {
   uint32_t mmal;
   OMX_U32 omx;
   OMX_VIDEO_CODINGTYPE omx_coding;
} mmal_omx_video_profile_table[] =
{
   { MMAL_VIDEO_PROFILE_H263_BASELINE,           OMX_VIDEO_H263ProfileBaseline,           OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_PROFILE_H263_H320CODING,         OMX_VIDEO_H263ProfileH320Coding,         OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_PROFILE_H263_BACKWARDCOMPATIBLE, OMX_VIDEO_H263ProfileBackwardCompatible, OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_PROFILE_H263_ISWV2,              OMX_VIDEO_H263ProfileISWV2,              OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_PROFILE_H263_ISWV3,              OMX_VIDEO_H263ProfileISWV3,              OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_PROFILE_H263_HIGHCOMPRESSION,    OMX_VIDEO_H263ProfileHighCompression,    OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_PROFILE_H263_INTERNET,           OMX_VIDEO_H263ProfileInternet,           OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_PROFILE_H263_INTERLACE,          OMX_VIDEO_H263ProfileInterlace,          OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_PROFILE_H263_HIGHLATENCY,        OMX_VIDEO_H263ProfileHighLatency,        OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_PROFILE_MP4V_SIMPLE,             OMX_VIDEO_MPEG4ProfileSimple,            OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_MP4V_SIMPLESCALABLE,     OMX_VIDEO_MPEG4ProfileSimpleScalable,    OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_MP4V_CORE,               OMX_VIDEO_MPEG4ProfileCore,              OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_MP4V_MAIN,               OMX_VIDEO_MPEG4ProfileMain,              OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_MP4V_NBIT,               OMX_VIDEO_MPEG4ProfileNbit,              OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_MP4V_SCALABLETEXTURE,    OMX_VIDEO_MPEG4ProfileScalableTexture,   OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_MP4V_SIMPLEFACE,         OMX_VIDEO_MPEG4ProfileSimpleFace,        OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_MP4V_SIMPLEFBA,          OMX_VIDEO_MPEG4ProfileSimpleFBA,         OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_MP4V_BASICANIMATED,      OMX_VIDEO_MPEG4ProfileBasicAnimated,     OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_MP4V_HYBRID,             OMX_VIDEO_MPEG4ProfileHybrid,            OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_MP4V_ADVANCEDREALTIME,   OMX_VIDEO_MPEG4ProfileAdvancedRealTime,  OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_MP4V_CORESCALABLE,       OMX_VIDEO_MPEG4ProfileCoreScalable,      OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_MP4V_ADVANCEDCODING,     OMX_VIDEO_MPEG4ProfileAdvancedCoding,    OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_MP4V_ADVANCEDCORE,       OMX_VIDEO_MPEG4ProfileAdvancedCore,      OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_MP4V_ADVANCEDSCALABLE,   OMX_VIDEO_MPEG4ProfileAdvancedScalable,  OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_MP4V_ADVANCEDSIMPLE,     OMX_VIDEO_MPEG4ProfileAdvancedSimple,    OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_PROFILE_H264_BASELINE,           OMX_VIDEO_AVCProfileBaseline,            OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_PROFILE_H264_MAIN,               OMX_VIDEO_AVCProfileMain,                OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_PROFILE_H264_EXTENDED,           OMX_VIDEO_AVCProfileExtended,            OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_PROFILE_H264_HIGH,               OMX_VIDEO_AVCProfileHigh,                OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_PROFILE_H264_HIGH10,             OMX_VIDEO_AVCProfileHigh10,              OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_PROFILE_H264_HIGH422,            OMX_VIDEO_AVCProfileHigh422,             OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_PROFILE_H264_HIGH444,            OMX_VIDEO_AVCProfileHigh444,             OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_PROFILE_DUMMY,                   OMX_VIDEO_AVCProfileMax,                 OMX_VIDEO_CodingAVC},
};

uint32_t mmalil_omx_video_profile_to_mmal(OMX_U32 profile, OMX_VIDEO_CODINGTYPE coding)
{
   unsigned int i;
   for(i = 0; mmal_omx_video_profile_table[i].mmal != MMAL_VIDEO_PROFILE_DUMMY; i++)
      if(mmal_omx_video_profile_table[i].omx == profile
         && mmal_omx_video_profile_table[i].omx_coding == coding) break;
   return mmal_omx_video_profile_table[i].mmal;
}

OMX_U32 mmalil_video_profile_to_omx(uint32_t profile)
{
   unsigned int i;
   for(i = 0; mmal_omx_video_profile_table[i].mmal != MMAL_VIDEO_PROFILE_DUMMY; i++)
      if(mmal_omx_video_profile_table[i].mmal == profile) break;
   return mmal_omx_video_profile_table[i].omx;
}

/*****************************************************************************/
static struct {
   uint32_t mmal;
   OMX_U32 omx;
   OMX_VIDEO_CODINGTYPE omx_coding;
} mmal_omx_video_level_table[] =
{
   { MMAL_VIDEO_LEVEL_H263_10, OMX_VIDEO_H263Level10,  OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_LEVEL_H263_20, OMX_VIDEO_H263Level20,  OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_LEVEL_H263_30, OMX_VIDEO_H263Level30,  OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_LEVEL_H263_40, OMX_VIDEO_H263Level40,  OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_LEVEL_H263_45, OMX_VIDEO_H263Level45,  OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_LEVEL_H263_50, OMX_VIDEO_H263Level50,  OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_LEVEL_H263_60, OMX_VIDEO_H263Level60,  OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_LEVEL_H263_70, OMX_VIDEO_H263Level70,  OMX_VIDEO_CodingH263},
   { MMAL_VIDEO_LEVEL_MP4V_0,  OMX_VIDEO_MPEG4Level0,  OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_LEVEL_MP4V_0b, OMX_VIDEO_MPEG4Level0b, OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_LEVEL_MP4V_1,  OMX_VIDEO_MPEG4Level1,  OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_LEVEL_MP4V_2,  OMX_VIDEO_MPEG4Level2,  OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_LEVEL_MP4V_3,  OMX_VIDEO_MPEG4Level3,  OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_LEVEL_MP4V_4,  OMX_VIDEO_MPEG4Level4,  OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_LEVEL_MP4V_4a, OMX_VIDEO_MPEG4Level4a, OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_LEVEL_MP4V_5,  OMX_VIDEO_MPEG4Level5,  OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_LEVEL_MP4V_6,  OMX_VIDEO_MPEG4Level6,  OMX_VIDEO_CodingMPEG4},
   { MMAL_VIDEO_LEVEL_H264_1,  OMX_VIDEO_AVCLevel1,    OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_H264_1b, OMX_VIDEO_AVCLevel1b,   OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_H264_11, OMX_VIDEO_AVCLevel11,   OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_H264_12, OMX_VIDEO_AVCLevel12,   OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_H264_13, OMX_VIDEO_AVCLevel13,   OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_H264_2,  OMX_VIDEO_AVCLevel2,    OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_H264_21, OMX_VIDEO_AVCLevel21,   OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_H264_22, OMX_VIDEO_AVCLevel22,   OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_H264_3,  OMX_VIDEO_AVCLevel3,    OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_H264_31, OMX_VIDEO_AVCLevel31,   OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_H264_32, OMX_VIDEO_AVCLevel32,   OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_H264_4,  OMX_VIDEO_AVCLevel4,    OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_H264_41, OMX_VIDEO_AVCLevel41,   OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_H264_42, OMX_VIDEO_AVCLevel42,   OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_H264_5,  OMX_VIDEO_AVCLevel5,    OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_H264_51, OMX_VIDEO_AVCLevel51,   OMX_VIDEO_CodingAVC},
   { MMAL_VIDEO_LEVEL_DUMMY,   OMX_VIDEO_AVCLevelMax,  OMX_VIDEO_CodingMax},
};

uint32_t mmalil_omx_video_level_to_mmal(OMX_U32 level, OMX_VIDEO_CODINGTYPE coding)
{
   unsigned int i;
   for(i = 0; mmal_omx_video_level_table[i].mmal != MMAL_VIDEO_LEVEL_DUMMY; i++)
      if(mmal_omx_video_level_table[i].omx == level
         && mmal_omx_video_level_table[i].omx_coding == coding) break;
   return mmal_omx_video_level_table[i].mmal;
}

OMX_U32 mmalil_video_level_to_omx(uint32_t level)
{
   unsigned int i;
   for(i = 0; mmal_omx_video_level_table[i].mmal != MMAL_VIDEO_LEVEL_DUMMY; i++)
      if(mmal_omx_video_level_table[i].mmal == level) break;
   return mmal_omx_video_level_table[i].omx;
}

/*****************************************************************************/
static struct {
   MMAL_VIDEO_RATECONTROL_T mmal;
   OMX_VIDEO_CONTROLRATETYPE omx;
} mmal_omx_video_ratecontrol_table[] =
{
   { MMAL_VIDEO_RATECONTROL_DEFAULT,              OMX_Video_ControlRateDisable},
   { MMAL_VIDEO_RATECONTROL_VARIABLE,             OMX_Video_ControlRateVariable},
   { MMAL_VIDEO_RATECONTROL_CONSTANT,             OMX_Video_ControlRateConstant},
   { MMAL_VIDEO_RATECONTROL_VARIABLE_SKIP_FRAMES, OMX_Video_ControlRateVariableSkipFrames},
   { MMAL_VIDEO_RATECONTROL_CONSTANT_SKIP_FRAMES, OMX_Video_ControlRateConstantSkipFrames},
   { MMAL_VIDEO_RATECONTROL_DUMMY,                OMX_Video_ControlRateMax},
};

MMAL_VIDEO_RATECONTROL_T mmalil_omx_video_ratecontrol_to_mmal(OMX_VIDEO_CONTROLRATETYPE omx)
{
   unsigned int i;
   for(i = 0; mmal_omx_video_ratecontrol_table[i].mmal != MMAL_VIDEO_RATECONTROL_DUMMY; i++)
      if(mmal_omx_video_ratecontrol_table[i].omx == omx) break;
   return mmal_omx_video_ratecontrol_table[i].mmal;
}

OMX_VIDEO_CONTROLRATETYPE mmalil_video_ratecontrol_to_omx(MMAL_VIDEO_RATECONTROL_T mmal)
{
   unsigned int i;
   for(i = 0; mmal_omx_video_ratecontrol_table[i].mmal != MMAL_VIDEO_RATECONTROL_DUMMY; i++)
      if(mmal_omx_video_ratecontrol_table[i].mmal == mmal) break;
   return mmal_omx_video_ratecontrol_table[i].omx;
}
