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
#include "RaspiTexUtil.h"
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#define SHADER_MAX_ATTRIBUTES 16
#define SHADER_MAX_UNIFORMS   16

/**
 * Container for a GL texture.
 */
struct TEXTURE_T {
    GLuint name;
    GLuint width;
    GLuint height;
};

/**
 * Container for a simple vertex, fragment share with the names and locations.
 */
struct SHADER_PROGRAM_T {
    const char *vertex_source;
    const char *fragment_source;
    const char *uniform_names[SHADER_MAX_UNIFORMS];
    const char *attribute_names[SHADER_MAX_ATTRIBUTES];
    GLint vs;
    GLint fs;
    GLint program;

    /// The locations for uniforms defined in uniform_names
    GLint uniform_locations[SHADER_MAX_UNIFORMS];

    /// The locations for attributes defined in attribute_names
    GLint attribute_locations[SHADER_MAX_ATTRIBUTES];

    /// Optional texture information
    struct TEXTURE_T tex;
};

/**
 * Draws an external EGL image and applies a sine wave distortion to create
 * a hall of mirrors effect.
 */
struct SHADER_PROGRAM_T picture_shader = {
    .vertex_source =
    "attribute vec2 vertex;\n"
    "varying vec2 texcoord;"
    "void main(void) {\n"
    "   texcoord = 0.5 * (vertex + 1.0);\n"
    "   gl_Position = vec4(vertex, 0.0, 1.0);\n"
    "}\n",

    .fragment_source =
    "#extension GL_OES_EGL_image_external : require\n"
    "uniform samplerExternalOES tex;\n"
    "uniform float offset;\n"
    "const float waves = 2.0;\n"
    "varying vec2 texcoord;\n"
    "void main(void) {\n"
    "    float x = texcoord.x + 0.05 * sin(offset + (texcoord.y * waves * 2.0 * 3.141592));\n"
    "    float y = texcoord.y + 0.05 * sin(offset + (texcoord.x * waves * 2.0 * 3.141592));\n"
    "    if (y < 1.0 && y > 0.0 && x < 1.0 && x > 0.0) {\n"
    "       vec2 pos = vec2(x, y);\n"
    "       gl_FragColor = texture2D(tex, pos);\n"
    "    }\n"
    "    else {\n"
    "       gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
    "    }\n"
    "}\n",
    .uniform_names = {"tex", "offset"},
    .attribute_names = {"vertex"},

};

/**
 * Utility for building shaders and configuring the attribute and locations.
 * @return Zero if successful.
 */
static int buildShaderProgram(struct SHADER_PROGRAM_T *p)
{
    GLint status;
    int i = 0;
    char log[1024];
    int logLen = 0;
    assert(p);
    assert(p->vertex_source);
    assert(p->fragment_source);

    if (! (p && p->vertex_source && p->fragment_source))
        goto fail;

    p->vs = p->fs = 0;

    GLCHK(p->vs = glCreateShader(GL_VERTEX_SHADER));
    GLCHK(glShaderSource(p->vs, 1, &p->vertex_source, NULL));
    GLCHK(glCompileShader(p->vs));
    GLCHK(glGetShaderiv(p->vs, GL_COMPILE_STATUS, &status));
    if (! status) {
        glGetShaderInfoLog(p->vs, sizeof(log), &logLen, log);
        vcos_log_trace("Program info log %s", log);
        goto fail;
    }

    GLCHK(p->fs = glCreateShader(GL_FRAGMENT_SHADER));
    GLCHK(glShaderSource(p->fs, 1, &p->fragment_source, NULL));
    GLCHK(glCompileShader(p->fs));
    GLCHK(glGetShaderiv(p->fs, GL_COMPILE_STATUS, &status));
    if (! status) {
        glGetShaderInfoLog(p->fs, sizeof(log), &logLen, log);
        vcos_log_trace("Program info log %s", log);
        goto fail;
    }

    GLCHK(p->program = glCreateProgram());
    GLCHK(glAttachShader(p->program, p->vs));
    GLCHK(glAttachShader(p->program, p->fs));
    GLCHK(glLinkProgram(p->program));

    GLCHK(glGetProgramiv(p->program, GL_LINK_STATUS, &status));
    if (! status)
    {
        vcos_log_trace("Failed to link shader program");
        glGetProgramInfoLog(p->program, sizeof(log), &logLen, log);
        vcos_log_trace("Program info log %s", log);
        goto fail;
    }

    for (i = 0; i < SHADER_MAX_ATTRIBUTES; ++i)
    {
        if (! p->attribute_names[i])
            break;
        GLCHK(p->attribute_locations[i] = glGetAttribLocation(p->program, p->attribute_names[i]));
        if (p->attribute_locations[i] == -1)
        {
            vcos_log_trace("Failed to get location for attribute %s", p->attribute_names[i]);
            goto fail;
        }
        else {
            vcos_log_trace("Attribute for %s is %d", p->attribute_names[i], p->attribute_locations[i]);
        }
    }

    for (i = 0; i < SHADER_MAX_UNIFORMS; ++i)
    {
        if (! p->uniform_names[i])
            break;
        GLCHK(p->uniform_locations[i] = glGetUniformLocation(p->program, p->uniform_names[i]));
        if (p->uniform_locations[i] == -1)
        {
            vcos_log_trace("Failed to get location for uniform %s", p->uniform_names[i]);
            goto fail;
        }
        else {
            vcos_log_trace("Uniform for %s is %d", p->uniform_names[i], p->uniform_locations[i]);
        }
    }

    return 0;
fail:
    vcos_log_trace("%s: Failed to build shader program", __func__);
    if (p)
    {
        glDeleteProgram(p->program);
        glDeleteShader(p->fs);
        glDeleteShader(p->vs);
    }
    return -1;
}

/**
 * Creates the OpenGL ES 2.X context and builds the shaders.
 * @param raspitex_state A pointer to the GL preview state.
 * @return Zero if successful.
 */
static int mirror_init(RASPITEX_STATE *state)
{
    int rc = raspitexutil_gl_init_2_0(state);
    if (rc != 0)
       goto end;

    rc = buildShaderProgram(&picture_shader);
end:
    return rc;
}

static int mirror_redraw(RASPITEX_STATE *raspitex_state) {
    static float offset = 0.0;

    // Start with a clear screen
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Bind the OES texture which is used to render the camera preview
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, raspitex_state->texture);

    offset += 0.05;
    GLCHK(glUseProgram(picture_shader.program));
    GLCHK(glEnableVertexAttribArray(picture_shader.attribute_locations[0]));
    GLfloat varray[] = {
        -1.0f, -1.0f,
        1.0f,  1.0f,
        1.0f, -1.0f,

        -1.0f,  1.0f,
        1.0f,  1.0f,
        -1.0f, -1.0f,
    };
    GLCHK(glVertexAttribPointer(picture_shader.attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, varray));
    GLCHK(glUniform1f(picture_shader.uniform_locations[1], offset));
    GLCHK(glDrawArrays(GL_TRIANGLES, 0, 6));

    GLCHK(glDisableVertexAttribArray(picture_shader.attribute_locations[0]));
    GLCHK(glUseProgram(0));
    return 0;
}

int mirror_open(RASPITEX_STATE *state)
{
   state->ops.gl_init = mirror_init;
   state->ops.redraw = mirror_redraw;
   return 0;
}
