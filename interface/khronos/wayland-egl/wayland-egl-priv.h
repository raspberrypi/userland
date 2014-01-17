/* Copied from Mesa */

#ifndef _WAYLAND_EGL_PRIV_H
#define _WAYLAND_EGL_PRIV_H

#ifdef  __cplusplus
extern "C" {
#endif

/* GCC visibility */
#if defined(__GNUC__) && __GNUC__ >= 4
#define WL_EGL_EXPORT __attribute__ ((visibility("default")))
#else
#define WL_EGL_EXPORT
#endif

#include "interface/vmcs_host/vc_dispmanx.h"
#include "interface/khronos/egl/egl_client_surface.h"

#include <wayland-client.h>

struct wl_dispmanx_client_buffer {
	struct wl_buffer *wl_buffer;
	DISPMANX_RESOURCE_HANDLE_T resource;

	int pending_allocation;
	int in_use;
	int width;
	int height;
};

struct wl_egl_window {
	struct wl_surface *wl_surface;

	int width;
	int height;
	int dx;
	int dy;

	int attached_width;
	int attached_height;

	/* XXX: The VC side seems to expect a valid element handle to be
	   passed to eglIntCreateSurface_impl and/or eglIntSwapBuffers_impl,
	   even for host-managed surfaces. */
	DISPMANX_ELEMENT_HANDLE_T dummy_element;
};

#ifdef  __cplusplus
}
#endif

#endif
