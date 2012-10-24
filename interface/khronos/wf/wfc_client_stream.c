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

#define VCOS_VERIFY_BKPTS 1 // TODO remove
#define VCOS_LOG_CATEGORY (&log_cat)

#include "interface/khronos/common/khrn_client_platform.h"
#include "interface/khronos/common/khrn_client_pointermap.h"
#include "interface/vcos/vcos.h"

#include "interface/khronos/wf/wfc_client_stream.h"
#include "interface/khronos/wf/wfc_server_api.h"

//#define WFC_FULL_LOGGING
#ifdef WFC_FULL_LOGGING
#define WFC_LOG_LEVEL VCOS_LOG_TRACE
#else
#define WFC_LOG_LEVEL VCOS_LOG_WARN
#endif

//==============================================================================

//!@name Numbers of various things (all fixed size for now)
//!@{
#define WFC_STREAM_NUM_OF_BUFFERS            8
#define WFC_STREAM_NUM_OF_SOURCES_OR_MASKS   8
#define WFC_STREAM_NUM_OF_CONTEXT_INPUTS     8
//!@}

//!@name Stream-specific mutex
//!@{
#define STREAM_LOCK(stream_ptr)      (platform_mutex_acquire(&stream_ptr->mutex))
#define STREAM_UNLOCK(stream_ptr)    (platform_mutex_release(&stream_ptr->mutex))
//!@}

//==============================================================================

//! Top-level stream type
typedef struct
{
   //! Handle; may be assigned by window manager.
   WFCNativeStreamType handle;

   //! Flag, indicating that the stream has been created on the server.
   bool has_been_created;

   //! Flag indicating that destruction has been requested, but stream is still in use.
   bool destroy_pending;

   //! Mutex, for thread safety.
   PLATFORM_MUTEX_T mutex;

   //! Configuration flags.
   uint32_t flags;

   //!@brief Image providers to which this stream sends data; recorded so we do
   //! not destroy this stream if it is still associated with a source or mask.
   uint32_t num_of_sources_or_masks;
   //! Record if this stream holds the output from an off-screen context.
   bool used_for_off_screen;

   //! Thread for handling server-side request to change source and/or destination rectangles
   VCOS_THREAD_T rect_req_thread_data;
   //! Flag for when thread must terminate
   bool rect_req_terminate;
   //! Callback function notifying calling module
   WFC_STREAM_REQ_RECT_CALLBACK_T req_rect_callback;
   //! Argument to callback function
   void *req_rect_cb_args;
} WFC_STREAM_T;

//==============================================================================

//! Map containing all created streams.
static KHRN_POINTER_MAP_T stream_map;
//! Next stream handle, allocated by wfc_stream_get_next().
static WFCNativeStreamType wfc_next_stream = (1 << 31);

static VCOS_LOG_CAT_T log_cat = VCOS_LOG_INIT("wfc_client_stream", WFC_LOG_LEVEL);

//==============================================================================
//!@name Static functions
//!@{
static bool wfc_stream_initialise(void);
static void wfc_stream_create_internal(WFC_STREAM_T *stream_ptr, uint32_t flags);
static WFC_STREAM_T *wfc_stream_get_ptr_or_create_placeholder(WFCNativeStreamType stream);
static bool wfc_stream_destroy_actual(WFCNativeStreamType stream, WFC_STREAM_T *stream_ptr);
static void *wfc_stream_rect_req_thread(void *arg);
static void wfc_client_stream_post_sem(void *cb_data);
//!@}
//==============================================================================
//!@name Public functions
//!@{

WFCNativeStreamType wfc_stream_get_next(void)
// In cases where the caller doesn't want to assign a stream number, provide
// one for it.
{
   WFCNativeStreamType next_stream = wfc_next_stream;
   wfc_next_stream++;
   return next_stream;
} // wfc_stream_get_next()

//------------------------------------------------------------------------------

uint32_t wfc_stream_create(WFCNativeStreamType stream, uint32_t flags)
// Create a stream, using the given stream handle (typically assigned by the
// window manager). Return zero if OK.
{
   WFC_STREAM_T *stream_ptr;

   vcos_log_trace("%s: stream 0x%x flags 0x%x", VCOS_FUNCTION, stream, flags);

   // Create stream
   stream_ptr = wfc_stream_get_ptr_or_create_placeholder(stream);
   if(stream_ptr == NULL) {return 1;}

   STREAM_LOCK(stream_ptr);

   if(!stream_ptr->has_been_created)
   {
      vcos_log_info("wfc_stream_create_from_image: stream %X", stream);
      // Stream did not previously exist.
      wfc_stream_create_internal(stream_ptr, flags);

      vcos_assert(stream_ptr->handle == stream);
   } // if
   else
   {
      // Stream already exists, so nothing else to do.
      vcos_log_warn("wfc_stream_create_from_image: already exists: stream: %X", stream);

      vcos_assert(stream_ptr->flags == flags);
   } // else

   STREAM_UNLOCK(stream_ptr);

   return 0;
} // wfc_stream_create_from_image()

//------------------------------------------------------------------------------

WFCNativeStreamType wfc_stream_create_assign_id(uint32_t flags)
// Create a stream, and automatically assign it a new stream number, which is returned
{
   WFCNativeStreamType stream = wfc_stream_get_next();
   uint32_t failure = wfc_stream_create(stream, flags);

   if(failure) {return WFC_INVALID_HANDLE;}
   else {return stream;}
} // wfc_stream_create_assign_id()

//------------------------------------------------------------------------------

uint32_t wfc_stream_create_req_rect
   (WFCNativeStreamType stream, uint32_t flags,
      WFC_STREAM_REQ_RECT_CALLBACK_T callback, void *cb_args)
// Create a stream, using the given stream handle, which will notify the calling
// module when the server requests a change in source and/or destination rectangle,
// using the supplied callback. Return zero if OK.
{
   vcos_log_info("wfc_stream_create_req_rect: stream %X", stream);

   uint32_t failure;

   failure = wfc_stream_create(stream, flags | WFC_STREAM_FLAGS_REQ_RECT);

   WFC_STREAM_T *stream_ptr = wfc_stream_get_ptr_or_create_placeholder(stream);

   STREAM_LOCK(stream_ptr);

   // There's no point creating this type of stream if you don't supply a callback
   // to update the src/dest rects via WF-C.
   vcos_assert(callback != NULL);

   stream_ptr->req_rect_callback = callback;
   stream_ptr->req_rect_cb_args = cb_args;

   // Create thread for handling server-side request to change source
   // and/or destination rectangles. One per stream (if enabled).
   VCOS_STATUS_T status = vcos_thread_create(&stream_ptr->rect_req_thread_data, "wfc_stream_rect_req_thread",
      NULL, wfc_stream_rect_req_thread, (void *) stream);
   vcos_demand(status == VCOS_SUCCESS);

   STREAM_UNLOCK(stream_ptr);

   return failure;
} // wfc_stream_create_req_rect()

//------------------------------------------------------------------------------

void wfc_stream_register_source_or_mask(WFCNativeStreamType stream, bool add_source_or_mask)
// Indicate that a source or mask is now associated with this stream, or should
// now be removed from such an association.
{
   vcos_log_trace("%s: stream 0x%x add %d", VCOS_FUNCTION, stream, add_source_or_mask);

   WFC_STREAM_T *stream_ptr = wfc_stream_get_ptr_or_create_placeholder(stream);
   bool still_exists = true;

   STREAM_LOCK(stream_ptr);

   if(add_source_or_mask)
      {stream_ptr->num_of_sources_or_masks++;}
   else
   {
      if(vcos_verify(stream_ptr->num_of_sources_or_masks > 0))
         {stream_ptr->num_of_sources_or_masks--;}
      still_exists = wfc_stream_destroy_actual(stream, stream_ptr);
   } // else

   if(still_exists) {STREAM_UNLOCK(stream_ptr);}

} // wfc_stream_register_off_screen()

//------------------------------------------------------------------------------

void wfc_stream_await_buffer(WFCNativeStreamType stream)
// Suspend until buffer is available on the server.
{
   vcos_log_trace("%s: stream 0x%x", VCOS_FUNCTION, stream);

   WFC_STREAM_T *stream_ptr = wfc_stream_get_ptr_or_create_placeholder(stream);

   if(vcos_verify(stream_ptr->flags & WFC_STREAM_FLAGS_BUF_AVAIL))
   {
      VCOS_SEMAPHORE_T image_available_sem;
      VCOS_STATUS_T status;

      // Long running operation, so keep VC alive until it completes.
      wfc_server_use_keep_alive();

      status = vcos_semaphore_create(&image_available_sem, "WFC image available", 0);
      vcos_assert(status == VCOS_SUCCESS);      // For all relevant platforms
      vcos_unused(status);

      wfc_server_stream_on_image_available(stream, wfc_client_stream_post_sem, &image_available_sem);

      vcos_log_trace("%s: pre async sem wait: stream: %X", VCOS_FUNCTION, stream);
      vcos_semaphore_wait(&image_available_sem);
      vcos_log_trace("%s: post async sem wait: stream: %X", VCOS_FUNCTION, stream);

      vcos_semaphore_delete(&image_available_sem);
      wfc_server_release_keep_alive();
   } // if

} // wfc_stream_await_buffer()

//------------------------------------------------------------------------------

void wfc_stream_destroy(WFCNativeStreamType stream)
// Destroy a stream - unless it is still in use, in which case, mark it for
// destruction once all users have finished with it.
{
   vcos_log_info("wfc_stream_destroy: stream: %X", stream);

   WFC_STREAM_T *stream_ptr;

   // Look up stream
   stream_ptr = (WFC_STREAM_T *) khrn_pointer_map_lookup(&stream_map, stream);

   if(stream_ptr != NULL)
   {
      /* If stream is still in use (i.e. it's attached to at least one source/mask
       * which is associated with at least one element) then destruction is delayed
       * until it's no longer in use.
       * Element-source/mask associations must be dealt with in wfc_client.c. */
      STREAM_LOCK(stream_ptr);

      stream_ptr->destroy_pending = true;

      if(wfc_stream_destroy_actual(stream, stream_ptr))
         {STREAM_UNLOCK(stream_ptr);}
   } // if
   else
   {
      vcos_log_warn("wfc_stream_destroy: stream %X doesn't exist", stream);
   } // else

} // wfc_stream_destroy()

//------------------------------------------------------------------------------
//!@name
//! Off-screen composition functions
//!@{
//------------------------------------------------------------------------------

uint32_t wfc_stream_create_for_context
   (WFCNativeStreamType stream, uint32_t width, uint32_t height)
// Create a stream for an off-screen context to output to, with the default number of buffers.
{
   return wfc_stream_create_for_context_nbufs(stream, width, height, 0);
}

uint32_t wfc_stream_create_for_context_nbufs
   (WFCNativeStreamType stream, uint32_t width, uint32_t height, uint32_t nbufs)
// Create a stream for an off-screen context to output to, with a specific number of buffers.
{
   // Create stream
   wfc_stream_create(stream, WFC_STREAM_FLAGS_NONE);
   if(!vcos_verify(stream != WFC_INVALID_HANDLE))
      {return 1;}

   // Allocate buffers on the server.
   if (!wfc_server_stream_allocate_images(stream, width, height, nbufs))
   {
      // Failed to allocate buffers
      vcos_log_warn("%s: failed to allocate %u buffers for stream %X size %ux%u", VCOS_FUNCTION, nbufs, stream, width, height);
      wfc_stream_destroy(stream);
      return 1;
   }

   return 0;
} // wfc_stream_create_from_context()

//------------------------------------------------------------------------------

bool wfc_stream_used_for_off_screen(WFCNativeStreamType stream)
// Returns true if this stream exists, and is in use as the output of an
// off-screen context.
{
   bool used_for_off_screen;

   vcos_log_trace("%s: stream 0x%x", VCOS_FUNCTION, stream);

   WFC_STREAM_T *stream_ptr = wfc_stream_get_ptr_or_create_placeholder(stream);

   STREAM_LOCK(stream_ptr);

   used_for_off_screen = stream_ptr->used_for_off_screen;

   STREAM_UNLOCK(stream_ptr);

   return used_for_off_screen;

} // wfc_stream_used_for_off_screen()

//------------------------------------------------------------------------------

void wfc_stream_register_off_screen(WFCNativeStreamType stream, bool used_for_off_screen)
// Called on behalf of an off-screen context, to either set or clear the stream's
// flag indicating that it's being used as output for that context.
{
   if(stream == WFC_INVALID_HANDLE)
      {return;}

   vcos_log_trace("%s: stream 0x%x", VCOS_FUNCTION, stream);

   WFC_STREAM_T *stream_ptr = wfc_stream_get_ptr_or_create_placeholder(stream);
   bool still_exists = true;

   STREAM_LOCK(stream_ptr);

   stream_ptr->used_for_off_screen = used_for_off_screen;

   if(!used_for_off_screen)
      {still_exists = wfc_stream_destroy_actual(stream, stream_ptr);}

   if(still_exists) {STREAM_UNLOCK(stream_ptr);}

} // wfc_stream_register_off_screen()

//!@} // Off-screen composition functions
//!@} // Public functions
//==============================================================================

static bool wfc_stream_initialise(void)
//! Initialise logging and stream_map the first time around. Return true if OK.
{
   if(stream_map.storage == NULL) {
      // Logging
      vcos_log_set_level(&log_cat, WFC_LOG_LEVEL);
      vcos_log_register("wfc_client_stream", &log_cat);
      // Stream map
      if(!vcos_verify(khrn_pointer_map_init(&stream_map, 8)))
         return false;
   }

   return true;
} // wfc_stream_initialise()

//------------------------------------------------------------------------------

static void wfc_stream_create_internal(WFC_STREAM_T *stream_ptr, uint32_t flags)
//! Create stream, etc.
{
   stream_ptr->has_been_created = true;
   stream_ptr->flags = flags;

   uint64_t pid = vcos_process_id_current();
   uint32_t pid_lo = (uint32_t) pid;
   uint32_t pid_hi = (uint32_t) (pid >> 32);

   stream_ptr->handle = wfc_server_stream_create(stream_ptr->handle, flags, pid_lo, pid_hi);
} // wfc_stream_create_internal()

//------------------------------------------------------------------------------

static WFC_STREAM_T *wfc_stream_get_ptr_or_create_placeholder(WFCNativeStreamType stream)
//!@brief Return the pointer to the stream structure corresponding to the specified
//! stream handle. If it doesn't exist, create it.
{
   WFC_STREAM_T *stream_ptr = NULL;

   if (!wfc_stream_initialise()) return NULL;

   if (khrn_pointer_map_get_count(&stream_map) == 0)
      if (wfc_server_connect() != VCOS_SUCCESS)
         return NULL;

   // Look up stream
   stream_ptr = (WFC_STREAM_T *) khrn_pointer_map_lookup(&stream_map, stream);

   // If it doesn't exist, then create it, and insert into the map
   if(stream_ptr == NULL)
   {
      bool is_in_use = false;
      uint32_t in_use_timeout = 10;

      do
      {
         // In case this stream number is being re-used, block until the server
         // side has finished destroying it, or time out otherwise.
         is_in_use = wfc_server_stream_is_in_use(stream);
         in_use_timeout--;
         if(is_in_use) {vcos_sleep(1);}
      }
      while(is_in_use && (in_use_timeout > 0));

      if(!vcos_verify(in_use_timeout > 0))
      {
         vcos_log_warn("get_stream_ptr timeout");
         return NULL;
      } // if

      // Allocate memory for stream_ptr
      stream_ptr = (WFC_STREAM_T *) khrn_platform_malloc(sizeof(WFC_STREAM_T), "WFC_STREAM_T");

      if(vcos_verify(stream_ptr != NULL))
      {
         // Initialise new stream
         memset(stream_ptr, 0, sizeof(WFC_STREAM_T));
         platform_mutex_create(&stream_ptr->mutex);
         stream_ptr->handle = stream;
         // Insert stream into map
         khrn_pointer_map_insert(&stream_map, stream, stream_ptr);
      } // if
   } // if

   return stream_ptr;
} // wfc_stream_get_ptr_or_create_placeholder()

//------------------------------------------------------------------------------

static bool wfc_stream_destroy_actual(WFCNativeStreamType stream, WFC_STREAM_T *stream_ptr)
//!@brief Actually destroy the stream, _if_ it is no longer in use. Stream is already
//! locked before this function is called. If stream was destroyed, returns FALSE.
{
   if((stream_ptr == NULL)
      || !stream_ptr->destroy_pending
      || (stream_ptr->num_of_sources_or_masks > 0)
      || stream_ptr->used_for_off_screen)
      {return true;}

   vcos_log_info("wfc_stream_destroy_actual: stream: %X", stream);

   // Delete server-side stream
   wfc_server_stream_destroy(stream_ptr->handle);

   // Remove from map
   khrn_pointer_map_delete(&stream_map, stream);

   STREAM_UNLOCK(stream_ptr);

   // Wait for rectangle request thread to complete
   if(stream_ptr->flags & WFC_STREAM_FLAGS_REQ_RECT)
   {
      vcos_thread_join(&stream_ptr->rect_req_thread_data, NULL);
   } // if

   // Destroy mutex
   platform_mutex_destroy(&stream_ptr->mutex);

   // Delete
   khrn_platform_free(stream_ptr);

   if (khrn_pointer_map_get_count(&stream_map) == 0)
      wfc_server_disconnect();

   return false;

} // wfc_stream_destroy_actual()

//------------------------------------------------------------------------------

//! Convert from dispmanx source rectangle type (int * 2^16) to WF-C type (float).
#define WFC_DISPMANX_TO_SRC_VAL(value) (((WFCfloat) (value)) / 65536.0)

static void *wfc_stream_rect_req_thread(void *arg)
//!@brief Thread for handling server-side request to change source and/or destination
//! rectangles. One per stream (if enabled).
{
   WFCNativeStreamType stream = (WFCNativeStreamType) arg;

   WFC_STREAM_REQ_RECT_CALLBACK_T callback;
   void *cb_args;
   VCOS_SEMAPHORE_T rect_req_sem;
   VCOS_STATUS_T status;

   int32_t  vc_rects[8];
   WFCint   dest_rect[WFC_RECT_SIZE];
   WFCfloat src_rect[WFC_RECT_SIZE];

   vcos_log_info("wfc_stream_rect_req_thread: START: stream: %X", stream);

   WFC_STREAM_T *stream_ptr = wfc_stream_get_ptr_or_create_placeholder(stream);

   // Get local pointers to stream parameters
   callback = stream_ptr->req_rect_callback;
   cb_args = stream_ptr->req_rect_cb_args;

   status = vcos_semaphore_create(&rect_req_sem, "WFC rect req", 0);
   vcos_assert(status == VCOS_SUCCESS);      // On all relevant platforms

   while (status == VCOS_SUCCESS)
   {
      wfc_server_stream_on_rects_change(stream, wfc_client_stream_post_sem, &rect_req_sem);

      // Await new values from server
      vcos_semaphore_wait(&rect_req_sem);

      status = wfc_server_stream_get_rects(stream, vc_rects);
      if (status == VCOS_SUCCESS)
      {
         // Convert from VC/dispmanx to WF-C types.
         vcos_static_assert(sizeof(dest_rect) == (4 * sizeof(int32_t)));
         memcpy(dest_rect, vc_rects, sizeof(dest_rect)); // Types are equivalent

         src_rect[WFC_RECT_X] = WFC_DISPMANX_TO_SRC_VAL(vc_rects[4]);
         src_rect[WFC_RECT_Y] = WFC_DISPMANX_TO_SRC_VAL(vc_rects[5]);
         src_rect[WFC_RECT_WIDTH] = WFC_DISPMANX_TO_SRC_VAL(vc_rects[6]);
         src_rect[WFC_RECT_HEIGHT] = WFC_DISPMANX_TO_SRC_VAL(vc_rects[7]);

         callback(cb_args, dest_rect, src_rect);
      }
   }

   vcos_semaphore_delete(&rect_req_sem);

   vcos_log_info("wfc_stream_rect_req_thread: END: stream: %X", stream);

   return NULL;
}

//------------------------------------------------------------------------------

static void wfc_client_stream_post_sem(void *cb_data)
{
   VCOS_SEMAPHORE_T *sem = (VCOS_SEMAPHORE_T *)cb_data;

   vcos_log_trace("%s: sem %p", VCOS_FUNCTION, sem);
   vcos_assert(sem != NULL);
   vcos_semaphore_post(sem);
}

//==============================================================================

void wfc_stream_signal_eglimage_data(WFCNativeStreamType stream, EGLImageKHR im)
{
   wfc_server_stream_signal_eglimage_data(stream, im);
}

void wfc_stream_signal_mm_image_data(WFCNativeStreamType stream, uint32_t im)
{
   wfc_server_stream_signal_mm_image_data(stream, im);
}

void wfc_stream_signal_raw_pixels(WFCNativeStreamType stream, uint32_t handle, uint32_t format, uint32_t w, uint32_t h, uint32_t pitch)
{
   wfc_server_stream_signal_raw_pixels(stream, handle, format, w, h, pitch);
}

void wfc_stream_register(WFCNativeStreamType stream) {
   uint64_t pid = khronos_platform_get_process_id();
   uint32_t pid_lo = (uint32_t)pid;
   uint32_t pid_hi = (uint32_t)(pid >> 32);

   if (wfc_server_connect() == VCOS_SUCCESS)
      wfc_server_stream_register(stream, pid_lo, pid_hi);
}

void wfc_stream_unregister(WFCNativeStreamType stream) {
   wfc_server_stream_unregister(stream);
   wfc_server_disconnect();
}

//==============================================================================
