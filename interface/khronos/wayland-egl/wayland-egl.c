/* Copied from Mesa */

#include <stdlib.h>

#include <wayland-client.h>
#include <wayland-egl.h>
#include "wayland-egl-priv.h"

WL_EGL_EXPORT void
wl_egl_window_resize(struct wl_egl_window *egl_window,
		     int width, int height,
		     int dx, int dy)
{
        if (egl_window->width == width &&
	    egl_window->height == height &&
	    egl_window->dx == dx &&
	    egl_window->dy == dy)
		return;

	egl_window->width = width;
	egl_window->height = height;
	egl_window->dx = dx;
	egl_window->dy = dy;
}

WL_EGL_EXPORT struct wl_egl_window *
wl_egl_window_create(struct wl_surface *surface,
		     int width, int height)
{
	struct wl_egl_window *egl_window;

	egl_window = calloc(1, sizeof *egl_window);
	if (!egl_window)
		return NULL;

	egl_window->wl_surface = surface;
	wl_egl_window_resize(egl_window, width, height, 0, 0);
	egl_window->attached_width  = 0;
	egl_window->attached_height = 0;
	egl_window->dummy_element = PLATFORM_WIN_NONE;
	
	return egl_window;
}

WL_EGL_EXPORT void
wl_egl_window_destroy(struct wl_egl_window *egl_window)
{
	free(egl_window);
}

WL_EGL_EXPORT void
wl_egl_window_get_attached_size(struct wl_egl_window *egl_window,
				int *width, int *height)
{
	if (width)
		*width = egl_window->attached_width;
	if (height)
		*height = egl_window->attached_height;
}
