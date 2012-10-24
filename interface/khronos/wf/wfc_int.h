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

#ifndef WFC_INT_H
#define WFC_INT_H

#include "interface/khronos/include/WF/wfc.h"

//!@file wfc.h
//!@brief Standard OpenWF-C header file. Available from the
//! <a href="http://www.khronos.org/registry/wf/">Khronos OpenWF API Registry</a>.

//==============================================================================

//! Only valid value for attribute list (as of WF-C v1.0)
#define NO_ATTRIBUTES   NULL

//!@name
//! Values for rectangle types
//!@{
#define WFC_RECT_X      0
#define WFC_RECT_Y      1
#define WFC_RECT_WIDTH  2
#define WFC_RECT_HEIGHT 3

#define WFC_RECT_SIZE   4
//!@}

//!@name
//! Values for context background colour
//!@{
#define WFC_BG_CLR_RED     0
#define WFC_BG_CLR_GREEN   1
#define WFC_BG_CLR_BLUE    2
#define WFC_BG_CLR_ALPHA   3

#define WFC_BG_CLR_SIZE    4
//!@}

//==============================================================================

//! Attributes associated with each element
typedef struct
{
   WFCint         dest_rect[WFC_RECT_SIZE];//< The destination region of the element in the context
   WFCfloat       src_rect[WFC_RECT_SIZE];//< The region of the source to be mapped to the destination
   WFCboolean     flip;                   //< If WFC_TRUE, the source is flipped about a horizontal axis through its centre
   WFCRotation    rotation;               //< The rotation of the source, see WFCRotation
   WFCScaleFilter scale_filter;           //< The scaling filter, see WFCScaleFilter
   WFCbitfield    transparency_types;     //< A combination of transparency types, see WFCTransparencyType
   WFCfloat       global_alpha;           //< The global alpha for the element, if WFC_TRANSPARENCY_ELEMENT_GLOBAL_ALPHA is set
   WFCNativeStreamType source_stream;     //< The source stream handle or WFC_INVALID_HANDLE
   WFCNativeStreamType mask_stream;       //< The mask stream handle or WFC_INVALID_HANDLE
} WFC_ELEMENT_ATTRIB_T;

//! Default values for elements
#define WFC_ELEMENT_ATTRIB_DEFAULT \
{ \
   {0, 0, 0, 0}, \
   {0.0, 0.0, 0.0, 0.0}, \
   WFC_FALSE, \
   WFC_ROTATION_0, \
   WFC_SCALE_FILTER_NONE, \
   WFC_TRANSPARENCY_NONE, \
   1.0, \
   WFC_INVALID_HANDLE, \
   WFC_INVALID_HANDLE \
}

//! Attributes associated with a context which are fixed on creation
typedef struct
{
   WFCContextType type;                   //!< The context type, either on- or off-screen
   WFCint         width;                  //!< The width of the context
   WFCint         height;                 //!< The height of the context
} WFC_CONTEXT_STATIC_ATTRIB_T;

//! Default static values for contexts
#define WFC_CONTEXT_STATIC_ATTRIB_DEFAULT \
{ \
   WFC_CONTEXT_TYPE_ON_SCREEN, \
   0, 0, \
}

// Attributes associated with a context that may change
typedef struct
{
   WFCRotation    rotation;               //< The rotation to be applied to the whole context
   WFCfloat       background_clr[WFC_BG_CLR_SIZE]; //< The background colour of the context, in order RGBA, scaled from 0 to 1
} WFC_CONTEXT_DYNAMIC_ATTRIB_T;

//! Default dynamic values for contexts
#define WFC_CONTEXT_DYNAMIC_ATTRIB_DEFAULT \
{ \
   WFC_ROTATION_0, \
   {0.0, 0.0, 0.0, 1.0} \
}

//! Arbitrary maximum number of elements per scene
#define WFC_MAX_ELEMENTS_IN_SCENE 8

//! Data for a "scene" (i.e. context and element data associated with a commit).
typedef struct
{
    WFC_CONTEXT_DYNAMIC_ATTRIB_T context;    //!< Dynamic attributes for this scene's context
    uint32_t wait;                           //!< When true, signal when scene has been sent to compositor
    uint32_t element_count;                  //!< Number of elements to be committed
    WFC_ELEMENT_ATTRIB_T elements[WFC_MAX_ELEMENTS_IN_SCENE];  //!< Attributes of committed elements
} WFC_SCENE_T;

//==============================================================================
// VideoCore-specific definitions
//==============================================================================

//!@name
//! VideoCore standard display identifiers (c.f. vc_dispmanx_types.h).
//!@{
#define WFC_ID_MAIN_LCD    0
#define WFC_ID_AUX_LCD     1
#define WFC_ID_HDMI        2
#define WFC_ID_SDTV        3

#define WFC_ID_MAX_SCREENS 3
//!@}

//------------------------------------------------------------------------------
// Non-standard definitions

//!@brief WF-C assumes that images are pre-multiplied prior to blending. However, we
//! need to handle non-pre-multiplied ones.
#define WFC_TRANSPARENCY_SOURCE_VC_NON_PRE_MULT    (1 << 31)

//!@brief Raw pixel formats supported by the server
typedef enum
{
   WFC_PIXEL_FORMAT_NONE,
   WFC_PIXEL_FORMAT_YVU420PLANAR,
   WFC_PIXEL_FORMAT_FORCE_32BIT   = 0x7FFFFFFF
} WFC_PIXEL_FORMAT_T;

//------------------------------------------------------------------------------
// Non-standard functions

void wfc_set_deferral_stream(WFCDevice dev, WFCContext ctx, WFCNativeStreamType stream);

//==============================================================================
#endif /* WFC_INT_H */
