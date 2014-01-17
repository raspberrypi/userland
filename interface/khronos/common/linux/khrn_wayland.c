/*
Copyright (c) 2013, Raspberry Pi Foundation
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

#define VCOS_LOG_CATEGORY (&khrn_client_log)

#include "interface/khronos/common/linux/khrn_wayland.h"
#include "interface/khronos/wayland-dispmanx-client-protocol.h"
#include "interface/khronos/wayland-egl/wayland-egl-priv.h"

extern VCOS_LOG_CAT_T khrn_client_log;

static void handle_dispmanx_format(void *data, struct wl_dispmanx *dispmanx,
                                   uint32_t format)
{
}

static void handle_dispmanx_allocated(void *data, struct wl_dispmanx *dispmanx,
                                      struct wl_buffer *wl_buffer,
                                      uint32_t resource_handle)
{
    struct wl_dispmanx_client_buffer *buffer = wl_buffer_get_user_data(wl_buffer);

    buffer->pending_allocation = 0;
    buffer->resource = resource_handle;
}

static const struct wl_dispmanx_listener dispmanx_listener = {
    handle_dispmanx_format,
    handle_dispmanx_allocated,
};

static void
sync_callback(void *data, struct wl_callback *callback, uint32_t serial)
{
   int *done = data;

   *done = 1;

   wl_callback_destroy(callback);
}

static const struct wl_callback_listener sync_listener = {
   sync_callback
};

static int
roundtrip(CLIENT_PROCESS_STATE_T *process)
{
   struct wl_display *wl_display = khrn_platform_get_wl_display();
   struct wl_callback *callback;
   int done = 0, ret = 0;

   callback = wl_display_sync(wl_display);
   wl_callback_add_listener(callback, &sync_listener, &done);
   wl_proxy_set_queue((struct wl_proxy *) callback, process->wl_queue);
   while (ret != -1 && !done)
      ret = wl_display_dispatch_queue(wl_display, process->wl_queue);

   if (!done)
      wl_callback_destroy(callback);

   return ret;
}

int do_wl_roundtrip()
{
   CLIENT_PROCESS_STATE_T *process = CLIENT_GET_PROCESS_STATE();
   return roundtrip(process);
}

static void
registry_handle_global(void *data, struct wl_registry *registry,
                       uint32_t name, const char *interface, uint32_t version)
{
   struct wl_display *wl_display = khrn_platform_get_wl_display();
   CLIENT_PROCESS_STATE_T *process = (CLIENT_PROCESS_STATE_T *)data;

   if (strcmp(interface, "wl_dispmanx") == 0) {
      process->wl_dispmanx = wl_registry_bind(registry, name,
	       &wl_dispmanx_interface, 1);

      wl_proxy_set_queue((struct wl_proxy *) process->wl_dispmanx,
                         process->wl_queue);
      wl_dispmanx_add_listener(process->wl_dispmanx, &dispmanx_listener, wl_display);
      roundtrip(process);
   }
}

static void
registry_handle_global_remove(void *data, struct wl_registry *registry,
                              uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	registry_handle_global_remove
};

int
init_process_wayland(CLIENT_PROCESS_STATE_T *process)
{
    struct wl_display *wl_display = khrn_platform_get_wl_display();

    process->wl_queue = wl_display_create_queue(wl_display);
    if (!process->wl_queue) {
        vcos_log_error("wl_display_create_queue failed\n");
        return false;
    }
    wl_display_dispatch_pending(wl_display);

    process->wl_registry = wl_display_get_registry(wl_display);
    if (!process->wl_registry) {
        vcos_log_error("wl_display_get_registry failed\n");
        return false;
    }

    wl_proxy_set_queue((struct wl_proxy *) process->wl_registry,
    process->wl_queue);

    wl_registry_add_listener(process->wl_registry, &registry_listener, process);

    if (roundtrip(process) < 0 || process->wl_dispmanx == NULL) {
        vcos_log_error("failed to get wl_dispmanx\n");
        return false;
    }

    return true;
}

#ifndef ALIGN_UP
#define ALIGN_UP(x,y)  ((x + (y)-1) & ~((y)-1))
#endif

static void handle_buffer_release(void *data, struct wl_buffer *buffer_wl)
{
   struct wl_dispmanx_client_buffer *wl_dispmanx_client_buffer = data;
   wl_dispmanx_client_buffer->in_use = 0;
}

static const struct wl_buffer_listener buffer_listener = {
   handle_buffer_release
};

struct wl_dispmanx_client_buffer *
allocate_wl_buffer(struct wl_egl_window *window, KHRN_IMAGE_FORMAT_T color)
{
   CLIENT_PROCESS_STATE_T *process = CLIENT_GET_PROCESS_STATE();
   struct wl_dispmanx_client_buffer *wl_dispmanx_client_buffer;
   struct wl_buffer *wl_buffer;
   uint32_t stride = ALIGN_UP(window->width * 4, 16);
   uint32_t buffer_height = ALIGN_UP(window->height, 16);
   enum wl_dispmanx_format color_format;
   int ret = 0;

   switch (color) {
   case ABGR_8888:
      color_format = WL_DISPMANX_FORMAT_ABGR8888;
      break;
   case XBGR_8888:
      color_format = WL_DISPMANX_FORMAT_XBGR8888;
      break;
   case RGB_565:
      color_format = WL_DISPMANX_FORMAT_RGB565;
      break;
   default:
      vcos_log_error("unknown KHRN_IMAGE_FORMAT_T 0x%x\n", color);
      return NULL;
   }

   wl_buffer = wl_dispmanx_create_buffer(process->wl_dispmanx, window->width,
                                         window->height, stride, buffer_height,
                                         color_format);
   if (wl_buffer == NULL)
      return NULL;

   wl_dispmanx_client_buffer = calloc(1, sizeof(struct wl_dispmanx_client_buffer));
   wl_dispmanx_client_buffer->wl_buffer = wl_buffer;
   wl_dispmanx_client_buffer->in_use = 0;
   wl_dispmanx_client_buffer->pending_allocation = 1;
   wl_dispmanx_client_buffer->width = window->width;
   wl_dispmanx_client_buffer->height = window->height;

   wl_proxy_set_queue((struct wl_proxy *) wl_buffer, process->wl_queue);
   wl_buffer_add_listener(wl_buffer, &buffer_listener, wl_dispmanx_client_buffer);

   while (ret != -1 && wl_dispmanx_client_buffer->pending_allocation)
      ret = do_wl_roundtrip();

   return wl_dispmanx_client_buffer;
}
