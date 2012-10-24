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

#ifndef MMAL_IL_H
#define MMAL_IL_H

/** \defgroup MmalILUtility MMAL to OMX IL conversion utilities
 * \ingroup MmalUtilities
 *
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "interface/vmcs_host/khronos/IL/OMX_Core.h"
#include "interface/vmcs_host/khronos/IL/OMX_Component.h"
#include "interface/vmcs_host/khronos/IL/OMX_Video.h"
#include "interface/vmcs_host/khronos/IL/OMX_Audio.h"

/** Convert MMAL status codes into OMX error codes.
 *
 * @param status MMAL status code.
 * @return OMX error code.
 */
OMX_ERRORTYPE mmalil_error_to_omx(MMAL_STATUS_T status);

/** Convert OMX error codes into MMAL status codes.
 *
 * @param error OMX error code.
 * @return MMAL status code.
 */
MMAL_STATUS_T mmalil_error_to_mmal(OMX_ERRORTYPE error);

/** Convert MMAL buffer header flags into OMX buffer header flags.
 *
 * @param flags OMX buffer header flags.
 * @return MMAL buffer header flags.
 */
uint32_t mmalil_buffer_flags_to_mmal(OMX_U32 flags);

/** Convert OMX buffer header flags into MMAL buffer header flags.
 *
 * @param flags MMAL buffer header flags.
 * @return OMX buffer header flags.
 */
OMX_U32 mmalil_buffer_flags_to_omx(uint32_t flags);

/** Convert a MMAL buffer header into an OMX buffer header.
 * Note that only the fields which have a direct mapping between OMX and MMAL are converted.
 *
 * @param omx  Pointer to the destination OMX buffer header.
 * @param mmal Pointer to the source MMAL buffer header.
 */
void mmalil_buffer_header_to_omx(OMX_BUFFERHEADERTYPE *omx, MMAL_BUFFER_HEADER_T *mmal);

/** Convert an OMX buffer header into a MMAL buffer header.
 *
 * @param mmal Pointer to the destination MMAL buffer header.
 * @param omx  Pointer to the source OMX buffer header.
 */
void mmalil_buffer_header_to_mmal(MMAL_BUFFER_HEADER_T *mmal, OMX_BUFFERHEADERTYPE *omx);


OMX_PORTDOMAINTYPE mmalil_es_type_to_omx_domain(MMAL_ES_TYPE_T type);
MMAL_ES_TYPE_T mmalil_omx_domain_to_es_type(OMX_PORTDOMAINTYPE domain);
uint32_t mmalil_omx_audio_coding_to_encoding(OMX_AUDIO_CODINGTYPE coding);
OMX_VIDEO_CODINGTYPE mmalil_encoding_to_omx_audio_coding(uint32_t encoding);
uint32_t mmalil_omx_video_coding_to_encoding(OMX_VIDEO_CODINGTYPE coding);
OMX_VIDEO_CODINGTYPE mmalil_encoding_to_omx_video_coding(uint32_t encoding);
uint32_t mmalil_omx_image_coding_to_encoding(OMX_IMAGE_CODINGTYPE coding);
OMX_IMAGE_CODINGTYPE mmalil_encoding_to_omx_image_coding(uint32_t encoding);
uint32_t mmalil_omx_coding_to_encoding(uint32_t encoding, OMX_PORTDOMAINTYPE domain);
uint32_t mmalil_omx_color_format_to_encoding(OMX_COLOR_FORMATTYPE coding);
OMX_COLOR_FORMATTYPE mmalil_encoding_to_omx_color_format(uint32_t encoding);
uint32_t mmalil_omx_video_profile_to_mmal(OMX_U32 level, OMX_VIDEO_CODINGTYPE coding);
OMX_U32 mmalil_video_profile_to_omx(uint32_t profile);
uint32_t mmalil_omx_video_level_to_mmal(OMX_U32 level, OMX_VIDEO_CODINGTYPE coding);
OMX_U32 mmalil_video_level_to_omx(uint32_t level);
MMAL_VIDEO_RATECONTROL_T mmalil_omx_video_ratecontrol_to_mmal(OMX_VIDEO_CONTROLRATETYPE omx);
OMX_VIDEO_CONTROLRATETYPE mmalil_video_ratecontrol_to_omx(MMAL_VIDEO_RATECONTROL_T mmal);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* MMAL_IL_H */
