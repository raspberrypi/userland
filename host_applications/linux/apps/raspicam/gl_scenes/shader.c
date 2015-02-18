/*
Copyright (c) 2013, Broadcom Europe Ltd
Copyright (c) 2013, Tim Gover
Copyright (c) 2015, Antoine Villeret
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

#include <GLES/gl.h>
#include <GLES/glext.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include "RaspiTexUtil.h"

/* Vertex co-ordinates:
 *
 * v0----v1
 * |     |
 * |     |
 * |     |
 * v3----v2
 */

static const GLfloat vertices[] =
{
#define V0  -0.8,  0.8,  0.8,
#define V1   0.8,  0.8,  0.8,
#define V2   0.8, -0.8,  0.8,
#define V3  -0.8, -0.8,  0.8,
   V0 V3 V2 V2 V1 V0
};

/* Texture co-ordinates:
 *
 * (0,0) b--c
 *       |  |
 *       a--d
 *
 * b,a,d d,c,b
 */
static const GLfloat tex_coords[] =
{
   0, 0, 0, 1, 1, 1,
   1, 1, 1, 0, 0, 0
};

RASPITEXUTIL_SHADER_PROGRAM_T shader;

static int shader_init(RASPITEX_STATE *state)
{   
   int rc = raspitexutil_gl_init_2_0(state);
   if (rc != 0)
     goto end;

   rc = raspitexutil_build_shader_program(&shader);
end:
    return rc;
}

static int shader_update_shader(RASPITEX_STATE *state)
{
   int i;
   
   RASPITEXUTIL_SHADER_PARAMETER_T *array = shader.uniform_array;
   
   for(i=0; i<shader.uniform_count; i++)
   {
      if(array[i].flag > 0)
      {
         switch (array[i].type)
         {
           /* float vectors */
           case GL_FLOAT:
             glUniform1f( array[i].loc, array[i].param[0] );
             break;
           case GL_FLOAT_VEC2:
             glUniform2f( array[i].loc, (array[i].param[0]), (array[i].param[1]) );
             break;
           case GL_FLOAT_VEC3:
             glUniform3f( array[i].loc, (array[i].param[0]), (array[i].param[1]),
             (array[i].param[2]) );
           break;
           case GL_FLOAT_VEC4:
             glUniform4f( array[i].loc, (array[i].param[0]), (array[i].param[1]),
             (array[i].param[2]), (array[i].param[3]) );
           break;
           /* int vectors */
           case GL_INT:
             glUniform1i( array[i].loc, (array[i].param[0]) );
             break;
           case GL_INT_VEC2:
             glUniform2i( array[i].loc, (array[i].param[0]), (array[i].param[1]) );
             break;
           case GL_INT_VEC3:
             glUniform3i(array[i].loc,
             (array[i].param[0]), (array[i].param[1]), (array[i].param[2]) );
             break;
           case GL_INT_VEC4:
             glUniform4i(array[i].loc,
             (array[i].param[0]), (array[i].param[1]),
             (array[i].param[2]), (array[i].param[3]) );
             break;
           /* bool vectors */
           case GL_BOOL:
             glUniform1f( array[i].loc, (array[i].param[0]) );
             break;
           case GL_BOOL_VEC2:
             glUniform2f( array[i].loc, (array[i].param[0]), (array[i].param[1]) );
             break;
           case GL_BOOL_VEC3:
             glUniform3f( array[i].loc,
             (array[i].param[0]), (array[i].param[1]),
             (array[i].param[2]) );
             break;
           case GL_BOOL_VEC4:
             glUniform4f( array[i].loc,
             (array[i].param[0]), (array[i].param[1]),
             (array[i].param[2]), (array[i].param[3]) );
             break;

           /* float matrices */
           case GL_FLOAT_MAT2:
             // GL_TRUE = row major order, GL_FALSE = column major
             glUniformMatrix2fv( array[i].loc, 1, GL_FALSE, array[i].param );
             break;
           case GL_FLOAT_MAT3:
             glUniformMatrix3fv( array[i].loc, 1, GL_FALSE, array[i].param );
             break;
           case GL_FLOAT_MAT4:
             glUniformMatrix4fv( array[i].loc, 1, GL_FALSE, array[i].param );
             break;

           /* textures */
           case GL_SAMPLER_2D:
             glUniform1i(array[i].loc, array[i].param[0]);
             break;
           case GL_SAMPLER_CUBE: break;
             glUniform1i(array[i].loc, (array[i].param[0]));
             break;
           default:
           ;
         }
      // remove flag because the value is in GL's state now...
      array[i].flag=0;  

    }
  }
  return 0;
}

static int shader_redraw(RASPITEX_STATE *state)
{
   // Start with a clear screen
   //~ static float offset = 0.0;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Bind the OES texture which is used to render the camera preview
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, state->texture);

    //~ offset += 0.05;
    GLCHK(glUseProgram(shader.program));
    GLCHK(glEnableVertexAttribArray(shader.attribute_locations[0]));
    GLfloat varray[] = {
        -1.0f, -1.0f,
        1.0f,  1.0f,
        1.0f, -1.0f,

        -1.0f,  1.0f,
        1.0f,  1.0f,
        -1.0f, -1.0f,
    };
    GLCHK(glVertexAttribPointer(shader.attribute_locations[0], 2, GL_FLOAT, GL_FALSE, 0, varray));
    
    shader_update_shader(state);

    //GLCHK(glUniform1f(shader.uniform_locations[1], offset));
    GLCHK(glDrawArrays(GL_TRIANGLES, 0, 6));

    GLCHK(glDisableVertexAttribArray(shader.attribute_locations[0]));
    GLCHK(glUseProgram(0));
    return 0;
}

int shader_open(RASPITEX_STATE *state)
{
   const GLchar *defaultVertexShader = 
    "attribute vec2 vertex;\n"
    "varying vec2 texcoord;"
    "void main(void) {\n"
    "   texcoord = 0.5 * (vertex + 1.0);\n"
    "   gl_Position = vec4(vertex, 0.0, 1.0);\n"
    "}\n";

   const GLchar *defaultFragmentShader =  
      "#extension GL_OES_EGL_image_external : require\n"
      "varying vec2 texcoord;"
      "uniform samplerExternalOES tex;"
      "void main() \n"
      "{ \n"
      " gl_FragColor = texture2D(tex, texcoord); \n"
      "} \n";

   FILE * pfFile=NULL;
   FILE * pvFile=NULL;
   long lSize;
   int result;

   pfFile = fopen (state->fragment_shader_filename,"rb");
   if (pfFile==NULL) {
      printf("No or invalid fragment file provided, used the default one\n");
      shader.fragment_source=defaultFragmentShader;
   } else {

   // obtain file size:
   fseek (pfFile , 0 , SEEK_END);
   lSize = ftell (pfFile);
   rewind (pfFile);

   // allocate memory to contain the whole file:
   shader.fragment_source = (GLchar*) malloc (sizeof(GLchar)*lSize);
   if (shader.fragment_source == NULL) {fputs ("Memory error",stderr); exit (2);}

   // copy the file into the buffer:
   result = fread (shader.fragment_source,1,lSize,pfFile);
   if (result != lSize) {fputs ("Reading error",stderr); exit (3);}
   
   printf("fragment shader file : %s\n", state->fragment_shader_filename);

   /* the whole file is now loaded in the memory buffer. */
   }

   pvFile = fopen (state->vertex_shader_filename,"rb");
   if (pvFile==NULL) {
      printf("No or invalid vertex file provided, used the default one\n");
      shader.vertex_source=defaultVertexShader;
   } else {

   // obtain file size:
   fseek (pvFile , 0 , SEEK_END);
   lSize = ftell (pvFile);
   rewind (pvFile);

   // allocate memory to contain the whole file:
   shader.vertex_source = (GLchar*) malloc (sizeof(GLchar)*lSize);
   if (shader.vertex_source == NULL) {fputs ("Memory error",stderr); exit (2);}

   // copy the file into the buffer:
   result = fread (shader.vertex_source,1,lSize,pvFile);
   if (result != lSize) {fputs ("Reading error",stderr); exit (3);}
   
   printf("vertex shader file : %s\n", state->vertex_shader_filename);

   /* the whole file is now loaded in the memory buffer. */
   }
  
   state->ops.gl_init = shader_init;
   state->ops.redraw = shader_redraw;
   state->ops.update_texture = raspitexutil_update_texture;
   return 0;
}

RASPITEXUTIL_SHADER_PROGRAM_T* shader_get_shader()
{
   return &shader;
}
