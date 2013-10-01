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

// A rotating cube rendered with OpenGL|ES. Three images used as textures on the cube faces.

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <wayland-egl.h>
#include <wayland-client.h>

#include "GLES/gl.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"

#include "cube_texture_and_coords.h"

#define PATH "./"

#define IMAGE_SIZE 128

#ifndef M_PI
   #define M_PI 3.141592654
#endif
	

typedef struct
{
   uint32_t screen_width;
   uint32_t screen_height;
// OpenGL|ES objects
   EGLDisplay display;
   EGLSurface surface;
   EGLContext context;
   GLuint tex[6];
// model rotation vector and direction
   GLfloat rot_angle_x_inc;
   GLfloat rot_angle_y_inc;
   GLfloat rot_angle_z_inc;
// current model rotation angles
   GLfloat rot_angle_x;
   GLfloat rot_angle_y;
   GLfloat rot_angle_z;
// current distance from camera
   GLfloat distance;
   GLfloat distance_inc;
// pointers to texture buffers
   char *tex_buf1;
   char *tex_buf2;
   char *tex_buf3;
   struct wl_display *wl_display;
   struct wl_registry *wl_registry;
   struct wl_shell *wl_shell;
   struct wl_shell_surface *wl_shell_surface;
   struct wl_compositor *wl_compositor;
   struct wl_surface *wl_surface;
   struct wl_callback *wl_callback;
   struct wl_egl_window *wl_egl_window;
   int needs_update;
   int ellapsed_frames;
   int kill_compositor;
   int single_frame;
   int terminate_abruptly;
} CUBE_STATE_T;

static void init_ogl(CUBE_STATE_T *state);
static void init_model_proj(CUBE_STATE_T *state);
static void reset_model(CUBE_STATE_T *state);
static GLfloat inc_and_wrap_angle(GLfloat angle, GLfloat angle_inc);
static GLfloat inc_and_clip_distance(GLfloat distance, GLfloat distance_inc);
static void redraw_scene(CUBE_STATE_T *state);
static void update_model(CUBE_STATE_T *state);
static void init_textures(CUBE_STATE_T *state);
static void load_tex_images(CUBE_STATE_T *state);
static void exit_func(CUBE_STATE_T *state);

static void
registry_handle_global(void *data, struct wl_registry *registry,
		       uint32_t name, const char *interface, uint32_t version)
{
	CUBE_STATE_T *state = data;

	if (strcmp(interface, "wl_compositor") == 0) {
		state->wl_compositor =
			wl_registry_bind(registry, name,
					 &wl_compositor_interface, 1);
	} else if (strcmp(interface, "wl_shell") == 0) {
		state->wl_shell = wl_registry_bind(registry, name,
					    &wl_shell_interface, 1);
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

/***********************************************************
 * Name: init_ogl
 *
 * Arguments:
 *       CUBE_STATE_T *state - holds OGLES model info
 *
 * Description: Sets the display, OpenGL|ES context and screen stuff
 *
 * Returns: void
 *
 ***********************************************************/
static void init_ogl(CUBE_STATE_T *state)
{
   EGLBoolean result;
   EGLint num_config;

   static const EGLint attribute_list[] =
   {
      EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_NONE
   };
   
   EGLConfig config;

   state->wl_display = wl_display_connect(NULL);

   state->wl_registry = wl_display_get_registry(state->wl_display);
   wl_registry_add_listener(state->wl_registry, &registry_listener, state);

   wl_display_dispatch(state->wl_display);

   // get an EGL display connection
   state->display = eglGetDisplay(state->wl_display);
   assert(state->display!=EGL_NO_DISPLAY);

   // initialize the EGL display connection
   result = eglInitialize(state->display, NULL, NULL);
   assert(EGL_FALSE != result);

   // get an appropriate EGL frame buffer configuration
   result = eglChooseConfig(state->display, attribute_list, &config, 1, &num_config);
   assert(EGL_FALSE != result);

   // create an EGL rendering context
   state->context = eglCreateContext(state->display, config, EGL_NO_CONTEXT, NULL);
   assert(state->context!=EGL_NO_CONTEXT);

   // create an EGL window surface
   state->screen_width = 1024;
   state->screen_height = 860;

   state->wl_surface = wl_compositor_create_surface(state->wl_compositor);
   state->wl_shell_surface = wl_shell_get_shell_surface(state->wl_shell, state->wl_surface);

   wl_shell_surface_set_toplevel(state->wl_shell_surface);
   wl_shell_surface_set_title(state->wl_shell_surface, "triangle.c");

   state->wl_egl_window = wl_egl_window_create(state->wl_surface, state->screen_width, state->screen_height);

   state->surface = eglCreateWindowSurface( state->display, config, state->wl_egl_window, NULL );
   assert(state->surface != EGL_NO_SURFACE);

   // connect the context to the surface
   result = eglMakeCurrent(state->display, state->surface, state->surface, state->context);
   assert(EGL_FALSE != result);

   // Set background color and clear buffers
   glClearColor(0.15f, 0.25f, 0.35f, 1.0f);

   // Enable back face culling.
   glEnable(GL_CULL_FACE);

   glMatrixMode(GL_MODELVIEW);
}

/***********************************************************
 * Name: init_model_proj
 *
 * Arguments:
 *       CUBE_STATE_T *state - holds OGLES model info
 *
 * Description: Sets the OpenGL|ES model to default values
 *
 * Returns: void
 *
 ***********************************************************/
static void init_model_proj(CUBE_STATE_T *state)
{
   float nearp = 1.0f;
   float farp = 500.0f;
   float hht;
   float hwd;

   glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );

   glViewport(0, 0, (GLsizei)state->screen_width, (GLsizei)state->screen_height);
      
   glMatrixMode(GL_PROJECTION);
   glLoadIdentity();

   hht = nearp * (float)tan(45.0 / 2.0 / 180.0 * M_PI);
   hwd = hht * (float)state->screen_width / (float)state->screen_height;

   glFrustumf(-hwd, hwd, -hht, hht, nearp, farp);
   
   glEnableClientState( GL_VERTEX_ARRAY );
   glVertexPointer( 3, GL_BYTE, 0, quadx );

   reset_model(state);
}

/***********************************************************
 * Name: reset_model
 *
 * Arguments:
 *       CUBE_STATE_T *state - holds OGLES model info
 *
 * Description: Resets the Model projection and rotation direction
 *
 * Returns: void
 *
 ***********************************************************/
static void reset_model(CUBE_STATE_T *state)
{
   // reset model position
   glMatrixMode(GL_MODELVIEW);
   glLoadIdentity();
   glTranslatef(0.f, 0.f, -50.f);

   // reset model rotation
   state->rot_angle_x = 45.f; state->rot_angle_y = 30.f; state->rot_angle_z = 0.f;
   state->rot_angle_x_inc = 0.5f; state->rot_angle_y_inc = 0.5f; state->rot_angle_z_inc = 0.f;
   state->distance = 40.f;
}

/***********************************************************
 * Name: update_model
 *
 * Arguments:
 *       CUBE_STATE_T *state - holds OGLES model info
 *
 * Description: Updates model projection to current position/rotation
 *
 * Returns: void
 *
 ***********************************************************/
static void update_model(CUBE_STATE_T *state)
{
   // update position
   state->rot_angle_x = inc_and_wrap_angle(state->rot_angle_x, state->rot_angle_x_inc);
   state->rot_angle_y = inc_and_wrap_angle(state->rot_angle_y, state->rot_angle_y_inc);
   state->rot_angle_z = inc_and_wrap_angle(state->rot_angle_z, state->rot_angle_z_inc);
   state->distance    = inc_and_clip_distance(state->distance, state->distance_inc);

   glLoadIdentity();
   // move camera back to see the cube
   glTranslatef(0.f, 0.f, -state->distance);

   // Rotate model to new position
   glRotatef(state->rot_angle_x, 1.f, 0.f, 0.f);
   glRotatef(state->rot_angle_y, 0.f, 1.f, 0.f);
   glRotatef(state->rot_angle_z, 0.f, 0.f, 1.f);
}

/***********************************************************
 * Name: inc_and_wrap_angle
 *
 * Arguments:
 *       GLfloat angle     current angle
 *       GLfloat angle_inc angle increment
 *
 * Description:   Increments or decrements angle by angle_inc degrees
 *                Wraps to 0 at 360 deg.
 *
 * Returns: new value of angle
 *
 ***********************************************************/
static GLfloat inc_and_wrap_angle(GLfloat angle, GLfloat angle_inc)
{
   angle += angle_inc;

   if (angle >= 360.0)
      angle -= 360.f;
   else if (angle <=0)
      angle += 360.f;

   return angle;
}

/***********************************************************
 * Name: inc_and_clip_distance
 *
 * Arguments:
 *       GLfloat distance     current distance
 *       GLfloat distance_inc distance increment
 *
 * Description:   Increments or decrements distance by distance_inc units
 *                Clips to range
 *
 * Returns: new value of angle
 *
 ***********************************************************/
static GLfloat inc_and_clip_distance(GLfloat distance, GLfloat distance_inc)
{
   distance += distance_inc;

   if (distance >= 120.0f)
      distance = 120.f;
   else if (distance <= 40.0f)
      distance = 40.0f;

   return distance;
}

static pid_t get_server_pid(CUBE_STATE_T *state)
{
   struct ucred ucred;
   socklen_t len;
   int fd;

   fd = wl_display_get_fd(state->wl_display);
   len = sizeof ucred;
   getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &ucred, &len);

   return ucred.pid;
}

static void
frame(void *data, struct wl_callback *callback, uint32_t time)
{
   CUBE_STATE_T *state = (CUBE_STATE_T *) data;

   state->needs_update = 1;
}

static const struct wl_callback_listener frame_listener = {
	frame
};

static void
update(CUBE_STATE_T *state)
{
   if (!state->single_frame || state->ellapsed_frames == 0) {
      update_model(state);
      redraw_scene(state);
   }

   state->wl_callback = wl_surface_frame(state->wl_surface);
   wl_callback_add_listener(state->wl_callback, &frame_listener, state);

   if (state->ellapsed_frames == 100) {
      if (state->kill_compositor) {
         fprintf(stderr, "reached frame 100, killing compositor\n");
         pid_t pid = get_server_pid(state);
         kill(pid, SIGTERM);
      } else if (state->terminate_abruptly) {
         fprintf(stderr, "reached frame 100, terminating right away\n");
         exit_func(state);
         exit(0);
      }
   }

   if (!state->single_frame || state->ellapsed_frames == 0)
      eglSwapBuffers(state->display, state->surface);
   else {
      wl_surface_damage(state->wl_surface, 0, 0, state->screen_width,
                        state->screen_height);
      wl_surface_commit(state->wl_surface);
   }

   state->ellapsed_frames++;
}

/***********************************************************
 * Name: redraw_scene
 *
 * Arguments:
 *       CUBE_STATE_T *state - holds OGLES model info
 *
 * Description:   Draws the model and calls eglSwapBuffers
 *                to render to screen
 *
 * Returns: void
 *
 ***********************************************************/
static void redraw_scene(CUBE_STATE_T *state)
{
   // Start with a clear screen
   glClear( GL_COLOR_BUFFER_BIT );

   // Draw first (front) face:
   // Bind texture surface to current vertices
   glBindTexture(GL_TEXTURE_2D, state->tex[0]);

   // Need to rotate textures - do this by rotating each cube face
   glRotatef(270.f, 0.f, 0.f, 1.f ); // front face normal along z axis

   // draw first 4 vertices
   glDrawArrays( GL_TRIANGLE_STRIP, 0, 4);

   // same pattern for other 5 faces - rotation chosen to make image orientation 'nice'
   glBindTexture(GL_TEXTURE_2D, state->tex[1]);
   glRotatef(90.f, 0.f, 0.f, 1.f ); // back face normal along z axis
   glDrawArrays( GL_TRIANGLE_STRIP, 4, 4);

   glBindTexture(GL_TEXTURE_2D, state->tex[2]);
   glRotatef(90.f, 1.f, 0.f, 0.f ); // left face normal along x axis
   glDrawArrays( GL_TRIANGLE_STRIP, 8, 4);

   glBindTexture(GL_TEXTURE_2D, state->tex[3]);
   glRotatef(90.f, 1.f, 0.f, 0.f ); // right face normal along x axis
   glDrawArrays( GL_TRIANGLE_STRIP, 12, 4);

   glBindTexture(GL_TEXTURE_2D, state->tex[4]);
   glRotatef(270.f, 0.f, 1.f, 0.f ); // top face normal along y axis
   glDrawArrays( GL_TRIANGLE_STRIP, 16, 4);

   glBindTexture(GL_TEXTURE_2D, state->tex[5]);
   glRotatef(90.f, 0.f, 1.f, 0.f ); // bottom face normal along y axis
   glDrawArrays( GL_TRIANGLE_STRIP, 20, 4);
}

/***********************************************************
 * Name: init_textures
 *
 * Arguments:
 *       CUBE_STATE_T *state - holds OGLES model info
 *
 * Description:   Initialise OGL|ES texture surfaces to use image
 *                buffers
 *
 * Returns: void
 *
 ***********************************************************/
static void init_textures(CUBE_STATE_T *state)
{
   // load three texture buffers but use them on six OGL|ES texture surfaces
   load_tex_images(state);
   glGenTextures(6, &state->tex[0]);

   // setup first texture
   glBindTexture(GL_TEXTURE_2D, state->tex[0]);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, IMAGE_SIZE, IMAGE_SIZE, 0,
                GL_RGB, GL_UNSIGNED_BYTE, state->tex_buf1);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_NEAREST);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_NEAREST);

   // setup second texture - reuse first image
   glBindTexture(GL_TEXTURE_2D, state->tex[1]);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, IMAGE_SIZE, IMAGE_SIZE, 0,
                GL_RGB, GL_UNSIGNED_BYTE, state->tex_buf1);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_NEAREST);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_NEAREST);

   // third texture
   glBindTexture(GL_TEXTURE_2D, state->tex[2]);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, IMAGE_SIZE, IMAGE_SIZE, 0,
                GL_RGB, GL_UNSIGNED_BYTE, state->tex_buf2);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_NEAREST);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_NEAREST);

   // fourth texture  - reuse second image
   glBindTexture(GL_TEXTURE_2D, state->tex[3]);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, IMAGE_SIZE, IMAGE_SIZE, 0,
                GL_RGB, GL_UNSIGNED_BYTE, state->tex_buf2);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_NEAREST);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_NEAREST);

   //fifth texture
   glBindTexture(GL_TEXTURE_2D, state->tex[4]);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, IMAGE_SIZE, IMAGE_SIZE, 0,
                GL_RGB, GL_UNSIGNED_BYTE, state->tex_buf3);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_NEAREST);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_NEAREST);

   // sixth texture  - reuse third image
   glBindTexture(GL_TEXTURE_2D, state->tex[5]);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, IMAGE_SIZE, IMAGE_SIZE, 0,
                GL_RGB, GL_UNSIGNED_BYTE, state->tex_buf3);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_NEAREST);
   glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_NEAREST);

   // setup overall texture environment
   glTexCoordPointer(2, GL_FLOAT, 0, texCoords);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
   
   glEnable(GL_TEXTURE_2D);
}

/***********************************************************
 * Name: load_tex_images
 *
 * Arguments:
 *       void
 *
 * Description: Loads three raw images to use as textures on faces
 *
 * Returns: void
 *
 ***********************************************************/
static void load_tex_images(CUBE_STATE_T *state)
{
   FILE *tex_file1 = NULL, *tex_file2=NULL, *tex_file3 = NULL;
   int bytes_read, image_sz = IMAGE_SIZE*IMAGE_SIZE*3;

   state->tex_buf1 = malloc(image_sz);
   state->tex_buf2 = malloc(image_sz);
   state->tex_buf3 = malloc(image_sz);

   tex_file1 = fopen(PATH "Lucca_128_128.raw", "rb");
   if (tex_file1 && state->tex_buf1)
   {
      bytes_read=fread(state->tex_buf1, 1, image_sz, tex_file1);
      assert(bytes_read == image_sz);  // some problem with file?
      fclose(tex_file1);
   }

   tex_file2 = fopen(PATH "Djenne_128_128.raw", "rb");
   if (tex_file2 && state->tex_buf2)
   {
      bytes_read=fread(state->tex_buf2, 1, image_sz, tex_file2);
      assert(bytes_read == image_sz);  // some problem with file?
      fclose(tex_file2);      
   }

   tex_file3 = fopen(PATH "Gaudi_128_128.raw", "rb");
   if (tex_file3 && state->tex_buf3)
   {
      bytes_read=fread(state->tex_buf3, 1, image_sz, tex_file3);
      assert(bytes_read == image_sz);  // some problem with file?
      fclose(tex_file3);
   }   
}

//------------------------------------------------------------------------------

static void exit_func(CUBE_STATE_T *state)
{
   // clear screen
   glClear( GL_COLOR_BUFFER_BIT );
   eglSwapBuffers(state->display, state->surface);

   // Release OpenGL resources
   eglMakeCurrent( state->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );

   wl_egl_window_destroy(state->wl_egl_window);
   wl_shell_surface_destroy(state->wl_shell_surface);
   wl_surface_destroy(state->wl_surface);

   eglDestroySurface( state->display, state->surface );
   eglDestroyContext( state->display, state->context );
   eglTerminate( state->display );

   wl_display_flush(state->wl_display);

   // release texture buffers
   free(state->tex_buf1);
   free(state->tex_buf2);
   free(state->tex_buf3);

   printf("\ncube closed\n");
} // exit_func()

static int running = 1;

static void
signal_int(int signum)
{
	running = 0;
}

//==============================================================================

int main (int argc, char *argv[])
{
   struct sigaction sigint;
   CUBE_STATE_T state = {0,};
   int ret = 0;
   int i;

   for (i = 0; i < argc; i++) {
      if (strcmp(argv[i], "--kill-compositor") == 0)
         state.kill_compositor = 1;
      if (strcmp(argv[i], "--single-frame") == 0)
         state.single_frame = 1;
      if (strcmp(argv[i], "--terminate-abruptly") == 0)
         state.terminate_abruptly = 1;
      else if (strcmp(argv[i], "--help") == 0 ||
               strcmp(argv[i], "-h") == 0) {
         printf("Usage: hello_wayland.bin [OPTION]\n\n");
         printf("\t--kill-compositor\tkill the Wayland compositor after 100 frames\n");
         printf("\t-h, --help\t\tshow this text\n");
         printf("\t--single-frame\t\tupdate the display only once\n");
         printf("\t--terminate-abruptly\texit right after rendering the 100th frame\n");
         return 0;
      }
   }

   // Start OGLES
   init_ogl(&state);

   // Setup the model world
   init_model_proj(&state);

   // initialise the OGLES texture(s)
   init_textures(&state);

   sigint.sa_handler = signal_int;
   sigemptyset(&sigint.sa_mask);
   sigint.sa_flags = SA_RESETHAND;
   sigaction(SIGINT, &sigint, NULL);

   state.needs_update = 1;
   while (running && ret != -1) {
      if (state.needs_update) {
         update(&state);
         state.needs_update = 0;
      }

      ret = wl_display_dispatch(state.wl_display);
   }

   exit_func(&state);

   return 0;
}

