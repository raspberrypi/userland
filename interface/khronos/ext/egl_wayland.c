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

#include "interface/khronos/common/khrn_client_mangle.h"
#include "interface/khronos/common/khrn_client_rpc.h"

#include "interface/khronos/ext/egl_khr_sync_client.h"
#include "interface/khronos/include/EGL/egl.h"
#include "interface/khronos/include/EGL/eglext.h"

#include "interface/vmcs_host/vc_vchi_dispmanx.h"

#include <wayland-server.h>
#include "interface/khronos/wayland-dispmanx-server-protocol.h"

static void
destroy_buffer(struct wl_resource *resource)
{
   struct wl_dispmanx_server_buffer *buffer = wl_resource_get_user_data(resource);

   if(!buffer->in_use)
      vc_dispmanx_resource_delete(buffer->handle);

   free(buffer);
}

static void
buffer_destroy(struct wl_client *client, struct wl_resource *resource)
{
   wl_resource_destroy(resource);
}

static const struct wl_buffer_interface dispmanx_buffer_interface = {
   buffer_destroy
};

static VC_IMAGE_TYPE_T
get_vc_format(enum wl_dispmanx_format format)
{
	/* XXX: The app is likely to have been premultiplying in its shaders,
	 * but the VC scanout hardware on the RPi cannot mix premultiplied alpha
	 * channel with the element's alpha.
	 */
	switch (format) {
	case WL_DISPMANX_FORMAT_ABGR8888:
		return VC_IMAGE_RGBA32;
	case WL_DISPMANX_FORMAT_XBGR8888:
		return VC_IMAGE_BGRX8888;
	case WL_DISPMANX_FORMAT_RGB565:
		return VC_IMAGE_RGB565;
	default:
		/* invalid format */
		return VC_IMAGE_MIN;
	}
}

static void
dispmanx_create_buffer(struct wl_client *client, struct wl_resource *resource,
                       uint32_t id, int32_t width, int32_t height,
                       uint32_t stride, uint32_t buffer_height, uint32_t format)
{
   struct wl_dispmanx_server_buffer *buffer;
   VC_IMAGE_TYPE_T vc_format = get_vc_format(format);
   uint32_t dummy;

   if(vc_format == VC_IMAGE_MIN) {
      wl_resource_post_error(resource,
                             WL_DISPMANX_ERROR_INVALID_FORMAT,
                             "invalid format");
      return;
   }

   buffer = calloc(1, sizeof *buffer);
   if (buffer == NULL) {
      wl_resource_post_no_memory(resource);
      return;
   }

   buffer->handle = vc_dispmanx_resource_create(vc_format,
                                                width | (stride << 16),
                                                height | (buffer_height << 16),
                                                &dummy);
   if(buffer->handle == DISPMANX_NO_HANDLE) {
      wl_resource_post_error(resource,
                             WL_DISPMANX_ERROR_ALLOC_FAILED,
                             "allocation failed");
      free(buffer);
      return;
   }

   buffer->width = width;
   buffer->height = height;
   buffer->format = format;

   buffer->resource = wl_resource_create(resource->client, &wl_buffer_interface,
                                         1, id);
   if (!buffer->resource) {
      wl_resource_post_no_memory(resource);
      vc_dispmanx_resource_delete(buffer->handle);
      free(buffer);
      return;
   }

   wl_resource_set_implementation(buffer->resource,
				       (void (**)(void)) &dispmanx_buffer_interface,
				       buffer, destroy_buffer);

   wl_dispmanx_send_buffer_allocated(resource, buffer->resource,
                                     buffer->handle);
}

static const struct wl_dispmanx_interface dispmanx_interface = {
   dispmanx_create_buffer,
};

static void
bind_dispmanx(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
   struct wl_resource *resource;

   resource = wl_resource_create(client, &wl_dispmanx_interface, 1, id);
   wl_resource_set_implementation(resource, &dispmanx_interface, NULL, NULL);

   wl_resource_post_event(resource, WL_DISPMANX_FORMAT,
                          WL_DISPMANX_FORMAT_ARGB8888);

   wl_resource_post_event(resource, WL_DISPMANX_FORMAT,
                          WL_DISPMANX_FORMAT_XRGB8888);

   wl_resource_post_event(resource, WL_DISPMANX_FORMAT,
                          WL_DISPMANX_FORMAT_ABGR8888);

   wl_resource_post_event(resource, WL_DISPMANX_FORMAT,
                          WL_DISPMANX_FORMAT_XBGR8888);

   wl_resource_post_event(resource, WL_DISPMANX_FORMAT,
                          WL_DISPMANX_FORMAT_RGB565);
}

EGLBoolean EGLAPIENTRY
eglBindWaylandDisplayWL(EGLDisplay dpy, struct wl_display *display)
{
   CLIENT_THREAD_STATE_T *thread;
   CLIENT_PROCESS_STATE_T *process;

   if (!CLIENT_LOCK_AND_GET_STATES(dpy, &thread, &process))
      return EGL_FALSE;

   if (process->wl_global != NULL)
      goto error;

   process->wl_global = wl_global_create(display, &wl_dispmanx_interface, 1,
                                         NULL, bind_dispmanx);
   if (process->wl_global == NULL)
      goto error;

   return EGL_TRUE;

error:
   CLIENT_UNLOCK();
   return EGL_FALSE;
}

EGLBoolean EGLAPIENTRY
eglUnbindWaylandDisplayWL(EGLDisplay dpy, struct wl_display *display)
{
   CLIENT_THREAD_STATE_T *thread;
   CLIENT_PROCESS_STATE_T *process;

   if (!CLIENT_LOCK_AND_GET_STATES(dpy, &thread, &process))
      return EGL_FALSE;

   wl_global_destroy(process->wl_global);
   process->wl_global = NULL;

   CLIENT_UNLOCK();

   return EGL_TRUE;
}

static int
get_egl_format(enum wl_dispmanx_format format)
{
	switch (format) {
	case WL_DISPMANX_FORMAT_ABGR8888:
		return EGL_TEXTURE_RGBA;
	case WL_DISPMANX_FORMAT_XBGR8888:
		return EGL_TEXTURE_RGB;
	case WL_DISPMANX_FORMAT_RGB565:
		return EGL_TEXTURE_RGB;
	default:
		/* invalid format */
		return EGL_NO_TEXTURE;
	}
}

EGLBoolean EGLAPIENTRY
eglQueryWaylandBufferWL(EGLDisplay dpy, struct wl_resource *_buffer,
         EGLint attribute, EGLint *value)
{
   struct wl_dispmanx_server_buffer *buffer = wl_resource_get_user_data(_buffer);

   if (wl_resource_instance_of(_buffer, &wl_dispmanx_interface,
                               &dispmanx_buffer_interface))
      return EGL_FALSE;

   switch (attribute) {
   case EGL_TEXTURE_FORMAT:
      *value = get_egl_format(buffer->format);
      if (*value == EGL_NO_TEXTURE)
         return EGL_FALSE;
      return EGL_TRUE;
   case EGL_WIDTH:
      *value = buffer->width;
      return EGL_TRUE;
   case EGL_HEIGHT:
      *value = buffer->height;
      return EGL_TRUE;
   }

   return EGL_FALSE;
}
