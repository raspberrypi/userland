/*
Copyright (c) 2013, Broadcom Europe Ltd
Copyright (c) 2013, Tim Gover
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

#ifndef RASPITEX_H_
#define RASPITEX_H_

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
#include "interface/khronos/include/EGL/eglext_brcm.h"
#include "interface/mmal/mmal.h"

typedef enum {
   RASPITEX_SCENE_SQUARE = 0,
   RASPITEX_SCENE_MIRROR,
   RASPITEX_SCENE_TEAPOT

} RASPITEX_SCENE_T;

struct RASPITEX_STATE;

typedef struct RASPITEX_SCENE_OPS
{
   /// Creates a native window that will be used by egl_init
   /// to create a window surface.
   int (*create_native_window)(struct RASPITEX_STATE *state);

   /// Creates EGL surface for native window
   int (*gl_init)(struct RASPITEX_STATE *state);

   /// Advance to the next animation step
   int (*update_texture)(struct RASPITEX_STATE *state, EGLClientBuffer mm_buf);

   /// Advance to the next animation step
   int (*update_model)(struct RASPITEX_STATE *state);

   /// Draw the scene - called after update_model
   int (*redraw)(struct RASPITEX_STATE *state);

   /// Creates EGL surface for native window
   void (*gl_term)(struct RASPITEX_STATE *state);

   /// Destroys the native window
   void (*destroy_native_window)(struct RASPITEX_STATE *state);

   /// Called when the scene is unloaded
   void (*close)(struct RASPITEX_STATE *state);
} RASPITEX_SCENE_OPS;

/**
 * Contains the internal state and configuration for the GL rendered
 * preview window.
 */
typedef struct RASPITEX_STATE
{
   MMAL_PORT_T *preview_port;          /// Source port for preview opaque buffers
   MMAL_POOL_T *preview_pool;          /// Pool for storing opaque buffer handles
   MMAL_QUEUE_T *preview_queue;        /// Queue preview buffers to display in order
   VCOS_THREAD_T preview_thread;       /// Preview worker / GL rendering thread
   uint32_t preview_stop;              /// If zero the worker can continue

   /* Display rectangle for the native window */
   int32_t x;                          /// x-offset in pixels
   int32_t y;                          /// y-offset in pixels
   int32_t width;                      /// width in pixels
   int32_t height;                     /// height in pixels
   int opacity;                        /// Alpha value for display element
   int gl_win_defined;                 /// Use rect from --glwin instead of preview

   /* DispmanX info. This might be unused if a custom create_native_window
    * does something else. */
   DISPMANX_DISPLAY_HANDLE_T disp;     /// Dispmanx display for GL preview
   EGL_DISPMANX_WINDOW_T win;          /// Dispmanx handle for preview surface

   EGLNativeWindowType* native_window; /// Native window used for EGL surface
   EGLDisplay display;                 /// The current EGL display
   EGLSurface surface;                 /// The current EGL surface
   EGLContext context;                 /// The current EGL context

   GLuint texture;                     /// Texture name for the preview texture
   EGLImageKHR preview_egl_image;      /// The current preview EGL image
   MMAL_BUFFER_HEADER_T *preview_buf;  /// MMAL buffer currently bound to texture

   RASPITEX_SCENE_T scene_id;          /// Id of the scene to load
   RASPITEX_SCENE_OPS ops;             /// The interface for the current scene
   void *scene_state;                  /// Pointer to scene specific data
   int verbose;                        /// Log FPS

} RASPITEX_STATE;

int raspitex_init(RASPITEX_STATE *state);
void raspitex_destroy(RASPITEX_STATE *state);
int raspitex_start(RASPITEX_STATE *state);
void raspitex_stop(RASPITEX_STATE *state);
void raspitex_set_defaults(RASPITEX_STATE *state);
int raspitex_configure_preview_port(RASPITEX_STATE *state,
      MMAL_PORT_T *preview_port);
void raspitex_display_help();
int raspitex_parse_cmdline(RASPITEX_STATE *state,
      const char *arg1, const char *arg2);

#endif /* RASPITEX_H_ */
