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

#ifndef RASPITEX_UTIL_H
#define RASPITEX_UTIL_H

#define VCOS_LOG_CATEGORY (&raspitex_log_category)
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "RaspiTex.h"
#include "interface/vcos/vcos.h"

extern VCOS_LOG_CAT_T raspitex_log_category;

/* Uncomment to enable extra GL error checking */
//#define CHECK_GL_ERRORS
#if defined(CHECK_GL_ERRORS)
#define GLCHK(X) \
do { \
    GLenum err = GL_NO_ERROR; \
    X; \
   while ((err = glGetError())) \
   { \
      vcos_log_trace("GL error 0x%x in " #X "file %s line %d", err, __FILE__,__LINE__); \
      vcos_assert(err == GL_NO_ERROR); \
      exit(err); \
   } \
} \
while(0)
#else
#define GLCHK(X) X
#endif /* CHECK_GL_ERRORS */

/* Default GL scene ops functions */
int raspitexutil_create_native_window(RASPITEX_STATE *raspitex_state);
int raspitexutil_gl_init_1_0(RASPITEX_STATE *raspitex_state);
int raspitexutil_gl_init_2_0(RASPITEX_STATE *raspitex_state);
int raspitexutil_update_model(RASPITEX_STATE* raspitex_state);
int raspitexutil_redraw(RASPITEX_STATE* raspitex_state);
void raspitexutil_gl_term(RASPITEX_STATE *raspitex_state);
void raspitexutil_destroy_native_window(RASPITEX_STATE *raspitex_state);
int raspitexutil_update_texture(RASPITEX_STATE *raspitex_state,
      EGLClientBuffer mm_buf);
void raspitexutil_close(RASPITEX_STATE* raspitex_state);

#endif /* RASPITEX_UTIL_H */
