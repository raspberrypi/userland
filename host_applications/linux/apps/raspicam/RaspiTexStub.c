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

#include "RaspiTex.h"
#include "RaspiCLI.h"
#include "interface/vcos/vcos.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"

/**
 * \file RaspiTex.c
 *
 * A simple framework for extending a MMAL application to render buffers via
 * OpenGL.
 *
 * MMAL buffers are often in YUV colour space and in either a planar or
 * tile format which is not supported directly by V3D. Instead of copying
 * the buffer from the GPU and doing a colour space / pixel format conversion
 * the GL_OES_EGL_image_external is used. This allows an EGL image to be
 * created from GPU buffer handle (MMAL opaque buffer handle). The EGL image
 * may then be used to create a texture (glEGLImageTargetTexture2DOES) and
 * drawn by either OpenGL ES 1.0 or 2.0 contexts.
 *
 * Notes:
 * 1) GL_OES_EGL_image_external textures always return pixels in RGBA format.
 *    This is also the case when used from a fragment shader.
 *
 * 2) The driver implementation creates a new RGB_565 buffer and does the color
 *    space conversion from YUV. This happens in GPU memory using the vector
 *    processor.
 *
 * 3) Each EGL external image in use will consume GPU memory for the RGB 565
 *    buffer. In addition, the GL pipeline might require more than one EGL image
 *    to be retained in GPU memory until the drawing commands are flushed.
 *
 *    Typically 128 MB of GPU memory is sufficient for 720p viewfinder and 720p
 *    GL surface. If both the viewfinder and the GL surface are 1080p then
 *    256MB of GPU memory is recommended, otherwise, for non-trivial scenes
 *    the system can run out of GPU memory whilst the camera is running.
 *
 * 4) It is important to make sure that the MMAL opaque buffer is not returned
 *    to MMAL before the GL driver has completed the asynchronous call to
 *    glEGLImageTargetTexture2DOES. Deferring destruction of the EGL image and
 *    the buffer return to MMAL until after eglSwapBuffers is the recommended.
 *
 * See also: http://www.khronos.org/registry/gles/extensions/OES/OES_EGL_image_external.txt
 */

#define DEFAULT_WIDTH   640
#define DEFAULT_HEIGHT  480

enum
{
   CommandGLScene,
   CommandGLWin
};

static COMMAND_LIST cmdline_commands[] =
{
   { CommandGLScene, "-glscene",  "gs",  "GL scene square,teapot,mirror,yuv,sobel,vcsm_square", 1 },
   { CommandGLWin,   "-glwin",    "gw",  "GL window settings <'x,y,w,h'>", 1 },
};

static int cmdline_commands_size = sizeof(cmdline_commands) / sizeof(cmdline_commands[0]);

/**
 * Parse a possible command pair - command and parameter
 * @param arg1 Command
 * @param arg2 Parameter (could be NULL)
 * @return How many parameters were used, 0,1,2
 */
int raspitex_parse_cmdline(RASPITEX_STATE *state,
                           const char *arg1, const char *arg2)
{
   int command_id, used = 0, num_parameters;

   if (!arg1)
      return 0;

   command_id = raspicli_get_command_id(cmdline_commands,
                                        cmdline_commands_size, arg1, &num_parameters);

   // If invalid command, or we are missing a parameter, drop out
   if (command_id==-1 || (command_id != -1 && num_parameters > 0 && arg2 == NULL))
      return 0;

   switch (command_id)
   {
   case CommandGLWin: // Allows a GL window to be different to preview-res
   {
      int tmp;
      tmp = sscanf(arg2, "%d,%d,%d,%d",
                   &state->x, &state->y, &state->width, &state->height);
      if (tmp != 4)
      {
         // Default to safe size on parse error
         state->x = state->y = 0;
         state->width = DEFAULT_WIDTH;
         state->height = DEFAULT_HEIGHT;
      }
      else
      {
         state->gl_win_defined = 1;
      }

      used = 2;
      break;
   }

   case CommandGLScene: // Selects the GL scene
   {
      if (strcmp(arg2, "square") == 0)
         state->scene_id = RASPITEX_SCENE_SQUARE;
      else if (strcmp(arg2, "teapot") == 0)
         state->scene_id = RASPITEX_SCENE_TEAPOT;
      else if (strcmp(arg2, "mirror") == 0)
         state->scene_id = RASPITEX_SCENE_MIRROR;
      else if (strcmp(arg2, "yuv") == 0)
         state->scene_id = RASPITEX_SCENE_YUV;
      else if (strcmp(arg2, "sobel") == 0)
         state->scene_id = RASPITEX_SCENE_SOBEL;
      else if (strcmp(arg2, "vcsm_square") == 0)
         state->scene_id = RASPITEX_SCENE_VCSM_SQUARE;
      else
         fprintf(stderr, "Unknown scene %s", arg2);

      used = 2;
      break;
   }
   }
   return used;
}

/**
 * Display help for command line options
 */
void raspitex_display_help()
{
   fprintf(stdout, "\nGL parameter commands\n\n");
   raspicli_display_help(cmdline_commands, cmdline_commands_size);
}


/* Registers a callback on the camera preview port to receive
 * notifications of new frames.
 * This must be called before rapitex_start and may not be called again
 * without calling raspitex_destroy first.
 *
 * @param state Pointer to the GL preview state.
 * @param port  Pointer to the camera preview port
 * @return Zero if successful.
 */
int raspitex_configure_preview_port(RASPITEX_STATE *state,
                                    MMAL_PORT_T *preview_port)
{
   return -1;
}

/* Initialises GL preview state and creates the dispmanx native window.
 * @param state Pointer to the GL preview state.
 * @return Zero if successful.
 */
int raspitex_init(RASPITEX_STATE *state)
{
   return -1;
}

/* Destroys the pools of buffers used by the GL renderer.
 * @param  state Pointer to the GL preview state.
 */
void raspitex_destroy(RASPITEX_STATE *state)
{
}

/* Initialise the GL / window state to sensible defaults.
 * Also initialise any rendering parameters e.g. the scene
 *
 * @param state Pointer to the GL preview state.
 * @return Zero if successful.
 */
void raspitex_set_defaults(RASPITEX_STATE *state)
{
}

/* Stops the rendering loop and destroys MMAL resources
 * @param state  Pointer to the GL preview state.
 */
void raspitex_stop(RASPITEX_STATE *state)
{
}

/**
 * Starts the worker / GL renderer thread.
 * @pre raspitex_init was successful
 * @pre raspitex_configure_preview_port was successful
 * @param state Pointer to the GL preview state.
 * @return Zero on success, otherwise, -1 is returned
 * */
int raspitex_start(RASPITEX_STATE *state)
{
   return -1;
}

/**
 * Writes the next GL frame-buffer to a RAW .ppm formatted file
 * using the specified file-handle.
 * @param state Pointer to the GL preview state.
 * @param outpt_file Output file handle for the ppm image.
 * @return Zero on success.
 */
int raspitex_capture(RASPITEX_STATE *state, FILE *output_file)
{
   return -1;
}
