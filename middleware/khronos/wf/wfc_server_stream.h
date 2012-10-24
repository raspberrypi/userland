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

#ifndef WFC_SERVER_STREAM_H
#define WFC_SERVER_STREAM_H

#include "interface/khronos/include/WF/wfc.h"
#include "helpers/vc_image/vc_image.h"
#include "interface/khronos/common/khrn_int_common.h"
#include "interface/khronos/include/EGL/eglext.h"
#include "interface/khronos/wf/wfc_server_api.h"

// Special SurfaceFlinger stream ID (cf SF_DISPMANX_ID in khrn_client_platform_android.c)
#define SF_STREAM_ID 0x2000000F

//==============================================================================

typedef enum
{
   WFC_IMAGE_NO_FLAGS         = 0,
   WFC_IMAGE_FLIP_VERT        = (1 << 0), //< Vertically flip image
   WFC_IMAGE_DISP_NOTHING     = (1 << 1), //< Display nothing on screen
   WFC_IMAGE_CB_ON_NO_CHANGE  = (1 << 2), //< Callback, even if the image is the same
   WFC_IMAGE_CB_ON_COMPOSE    = (1 << 3), //< Callback on composition, instead of when not in use

   WFC_IMAGE_SENTINEL         = 0x7FFFFFFF
} WFC_IMAGE_FLAGS_T;

// Callback function types.

/** Called when the buffer of a stream is no longer in use.
 *
 * @param The client stream handle.
 * @param The value passed in when the buffer was set as the front buffer.
 */
typedef void (*WFC_SERVER_STREAM_CALLBACK_T)(WFCNativeStreamType stream, void *cb_data);

//==============================================================================

//------------------------------------------------------------------------------
// Functions called by renderer server (for elements).

// Queue image in stream's buffer. Returns an error code.
#define WFC_QUEUE_OK                   0
#define WFC_QUEUE_NO_IMAGE             (1 << 0)
#define WFC_QUEUE_WRONG_DIMENSIONS     (1 << 1)
#define WFC_QUEUE_WRONG_PIXEL_FORMAT   (1 << 2)
#define WFC_QUEUE_NO_ASSOC_ELEMENT     (1 << 3)
#define WFC_QUEUE_STREAM_PLACEHOLDER   (1 << 4)
#define WFC_QUEUE_BUSY                 (1 << 5)
#define WFC_QUEUE_NOT_REGISTERED       (1 << 6)
#define WFC_QUEUE_NO_MEMORY            (1 << 7)

/** Set the given buffer to be the new front buffer for the stream.
 *
 * If the vc_image is NULL, it means that there is no front buffer for the stream,
 * and NULL will be returned when the front buffer is requested. The callback function
 * shall not be called in this situation.
 *
 * If the callback function is not NULL, it shall either be called when the buffer
 * is no longer in use (either as the current front buffer, or because it is part of a
 * current composition), or when it has been used in composition, according to whether
 * the WFC_IMAGE_CB_ON_COMPOSE flag is set.
 *
 * @param stream The client handle for the stream.
 * @param vc_image Details of the new buffer, or NULL.
 * @param flags Flags associated with the new buffer, combination of WFC_IMAGE_FLAGS_T
 * values.
 * @param cb_func Function to call when either the buffer is no longer in use or
 * has been used in composition, or NULL.
 * @param cb_data Data to be passed to the callback function, or NULL.
 * @return TBD
 */
uint32_t wfc_server_stream_set_front_image(
      WFCNativeStreamType stream, const VC_IMAGE_T *vc_image, int flags,
      WFC_SERVER_STREAM_CALLBACK_T cb_func, void *cb_data);

/** Signal to client that images are available for writing to. For use when
 * renderer is allocating its images.
 *
 * @param stream The client handle for the stream.
 * @param num_of_images The number of images allocated by the renderer.
 */
void wfc_server_stream_signal_images_avail(WFCNativeStreamType stream, uint32_t num_of_images);

// Request update to destination and/or source rectangles. Sends request to host
// for executing via WF-C.
void wfc_server_stream_update_rects(WFCNativeStreamType stream,
   const VC_RECT_T *vc_dest_rect, const VC_RECT_T *vc_src_rect);

//------------------------------------------------------------------------------
// Functions called only by Khronos dispatcher

bool wfc_server_stream_is_empty(void);

#endif /* WFC_SERVER_STREAM_H_ */
