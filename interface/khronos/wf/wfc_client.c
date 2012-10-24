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

#define VCOS_VERIFY_BKPTS 1

#define VCOS_LOG_CATEGORY (&log_cat)

#include <stdlib.h>

#include "interface/khronos/common/khrn_client_rpc.h"

#include "interface/khronos/include/WF/wfc.h"

#include "interface/khronos/wf/wfc_client_stream.h"
#include "interface/khronos/wf/wfc_server_api.h"

//#define WFC_FULL_LOGGING
#ifdef WFC_FULL_LOGGING
#define WFC_LOG_LEVEL VCOS_LOG_TRACE
#else
#define WFC_LOG_LEVEL VCOS_LOG_WARN
#endif

//==============================================================================

//!@name
//! Values used in wfcEnumerateDevices().
//!@{
#define WFC_NUM_OF_DEVICES 1
#define WFC_OUR_DEVICE_ID  1
//!@}

//!@name
//! Global mutex
//!@{
#define WFC_LOCK()      (vcos_mutex_lock(&wfc_client_state.mutex))
#define WFC_UNLOCK()    (vcos_mutex_unlock(&wfc_client_state.mutex))
//!@}

//!@name
//! Values for wfc_source_or_mask_create()
//!@{
#define WFC_IS_SOURCE   1
#define WFC_IS_MASK     0
//!@}

//==============================================================================

//! Simple doubly-linked list.
typedef struct _WFC_LINK
{
   struct _WFC_LINK *prev;
   struct _WFC_LINK *next;
} WFC_LINK_T;

//! Function pointer type, used when iterating through the linked list in wfc_link_iterate().
typedef void (*WFC_LINK_CALLBACK_T)(WFC_LINK_T *link, void *arg);

//------------------------------------------------------------------------------

//! WF-C device
typedef struct
{
   WFCErrorCode error;  //!< Error code returned by wfcGetError().

   WFC_LINK_T contexts; //!< Contexts belonging to this device.
} WFC_DEVICE_T;

//! WF-C context
typedef struct
{
   WFC_LINK_T link;           //!< Handle of this context within the contexts list.

   WFC_DEVICE_T *device_ptr;  //!< Parent device
   WFC_LINK_T sources;        //!< List of sources belonging to this context
   WFC_LINK_T masks;          //!< List of masks belonging to this context
   WFCNativeStreamType output_stream; //!< For off-screen contexts only, stream to compose into.

   WFC_LINK_T elements_not_in_scene;   //!< List of elements belonging to this context, but not on screen.
   WFC_LINK_T elements_in_scene;       //!< List of elements belonging to this context and on screen.
   bool active;                        //!< True if this context has had autonomous composition enabled with wfcActivate().

   WFC_CONTEXT_STATIC_ATTRIB_T static_attributes;  //!< Attributes associated with the context which are fixed on creation.
   WFC_CONTEXT_DYNAMIC_ATTRIB_T dynamic_attributes;//!< Attributes associated with the context which may change.

   //! Asynchronous (KHAN) semaphore for waiting for compositor to take scene.
   PLATFORM_SEMAPHORE_T wait_sem;

   WFC_SCENE_T committed_scene;     //!< Last committed scene
} WFC_CONTEXT_T;

//! WF-C image provider (source or mask)
typedef struct
{
   WFC_LINK_T link;              //!< Handle of this source/mask within the source/mask list.
   bool is_source;               //!< Indicates if this is a source or a mask.

   WFC_CONTEXT_T *context_ptr;   //!< Parent context

   //!@brief Number of elements using this source/mask.
   //! Required to ensure that it won't be destroyed while it's still in use.
   uint32_t refcount;

   WFCNativeStreamType stream;   //!< Stream associated with this source/mask.

   //!@brief Destruction requested.
   //! Indicates that the source/mask will be destroyed at the earliest opportunity.
   bool destroy_pending;
} WFC_SOURCE_OR_MASK_T;

//! WF-C element
typedef struct
{
   WFC_LINK_T link;                    //!< Handle of this element within the element list.
   WFC_CONTEXT_T *context_ptr;         //!< Parent context.

   WFC_SOURCE_OR_MASK_T *source_ptr;   //!< Source associated with this element.
   WFC_SOURCE_OR_MASK_T *mask_ptr;     //!< Mask associated with this element.

   bool is_in_scene;                   //!< Indicates if this element is within the scene.

   WFC_ELEMENT_ATTRIB_T attributes;    //!< Element attributes.
} WFC_ELEMENT_T;

//! Global state
typedef struct
{
   bool is_initialised; //!< Set the first time wfcCreateDevice() is called.
   VCOS_MUTEX_T mutex;  //!< Global mutex.
   VCOS_BLOCKPOOL_T source_pool;
} WFC_CLIENT_STATE_T;

//==============================================================================

static WFC_CLIENT_STATE_T wfc_client_state;  //!< Global state

static VCOS_LOG_CAT_T log_cat = VCOS_LOG_INIT("wfc_client_func", WFC_LOG_LEVEL);

//==============================================================================
//!@name Static functions
//!@{

static WFC_CONTEXT_T *wfc_context_create
   (WFC_DEVICE_T *device_ptr, WFCContextType context_type,
      uint32_t screen_or_stream_num, WFCErrorCode *error);
static void wfc_context_destroy(WFC_CONTEXT_T *context_ptr);

static WFCHandle wfc_source_or_mask_create(bool is_source, WFCDevice dev, WFCContext ctx,
      WFCNativeStreamType stream, const WFCint *attribList);
static void wfc_source_or_mask_destroy(WFCDevice dev, WFCHandle source_or_mask);
static void wfc_source_or_mask_acquire(WFC_SOURCE_OR_MASK_T *source_or_mask_ptr);
static void wfc_source_or_mask_release(WFC_SOURCE_OR_MASK_T *source_or_mask_ptr);

static void wfc_element_destroy(WFC_ELEMENT_T *element_ptr, void *unused);

static void wfc_commit_iterator(WFC_ELEMENT_T *element_ptr, WFC_SCENE_T *scene);

static void wfc_set_error(WFC_DEVICE_T *device, WFCErrorCode error);

static bool wfc_check_no_attribs(const WFCint *attribList);
static bool wfc_is_rotation(WFCint value);
static bool wfc_is_scale_filter(WFCint value);
static bool wfc_are_transparency_types(WFCint value);

static int32_t wfc_round(float f);

static void wfc_link_detach(WFC_LINK_T *link);
static void wfc_link_attach(WFC_LINK_T *link, WFC_LINK_T *prev);
static void wfc_link_init_null(WFC_LINK_T *link);
static void wfc_link_init_empty(WFC_LINK_T *link);
static void wfc_link_iterate(WFC_LINK_T *link, WFC_LINK_CALLBACK_T func, void *arg);

static void wfc_source_or_mask_destroy_actual
   (WFC_SOURCE_OR_MASK_T *source_or_mask_ptr, void *unused);

//!@} // (Static functions)

//!@name OpenWF-C API functions
//! Refer to the <a href="http://www.khronos.org/registry/wf/">OpenWF-C specification</a> for details.
//!@{
//==============================================================================
// Device functions

WFC_API_CALL WFCint WFC_APIENTRY
    wfcEnumerateDevices(WFCint *deviceIds, WFCint deviceIdsCount,
        const WFCint *filterList) WFC_APIEXIT
{
   bool filters_valid = true;

   // Check for valid filter list. Somewhat redundant, as there is only one device,
   // and it supports all screens.
   if(filterList != NULL)
   {
      filters_valid &= (*filterList == WFC_DEVICE_FILTER_SCREEN_NUMBER);
      filterList++;
      filters_valid &= ((*filterList >= 0) && (*filterList <= WFC_ID_MAX_SCREENS));
      filterList++;
      filters_valid &= (*filterList == WFC_NONE);
   } // if

   if(vcos_verify(filters_valid))
   {
      if(deviceIds != NULL)
      {
         if(deviceIdsCount > 0)
         {
            *deviceIds = WFC_OUR_DEVICE_ID;
            return WFC_NUM_OF_DEVICES;
         } // if
         else
            {return 0;}
      } // if
      else
         {return WFC_NUM_OF_DEVICES;}
   } // if

   return 0;
} // wfcEnumerateDevices()

//------------------------------------------------------------------------------

WFC_API_CALL WFCDevice WFC_APIENTRY
    wfcCreateDevice(WFCint deviceId, const WFCint *attribList) WFC_APIEXIT
{
   WFCDevice result = WFC_INVALID_HANDLE;

   // This function will be called before anything else can be created, so is
   // a good place to initialise the state.
   if(!wfc_client_state.is_initialised)
   {
      if (vcos_blockpool_create_on_heap(&wfc_client_state.source_pool,
         8, sizeof(WFC_SOURCE_OR_MASK_T), VCOS_BLOCKPOOL_ALIGN_DEFAULT, 0,
         "WFC source pool") != VCOS_SUCCESS)
      {
         return WFC_INVALID_HANDLE;
      }
      vcos_blockpool_extend(&wfc_client_state.source_pool, VCOS_BLOCKPOOL_MAX_SUBPOOLS-1, 8);

      wfc_client_state.is_initialised = true;
      vcos_mutex_create(&wfc_client_state.mutex, NULL);
      CLIENT_GET_THREAD_STATE();

      // Logging
      vcos_log_set_level(&log_cat, WFC_LOG_LEVEL);
      vcos_log_register("wfc_client_func", &log_cat);
   } // if

   WFC_LOCK();

   if ((deviceId == WFC_DEFAULT_DEVICE_ID || deviceId == WFC_OUR_DEVICE_ID)
      && wfc_check_no_attribs(attribList))
   {
      WFC_DEVICE_T *device = khrn_platform_malloc(sizeof(WFC_DEVICE_T), "WFC_DEVICE_T");

      if(vcos_verify(device != NULL))
      {
         if (wfc_server_connect() != VCOS_SUCCESS)
         {
            khrn_platform_free(device);
            vcos_log_error("%s: failed to connect to server", VCOS_FUNCTION);
         }
         else
         {
            device->error = WFC_ERROR_NONE;
            wfc_link_init_empty(&device->contexts);
            result = (WFCDevice) device;
         }
      } // if
   } // if

   WFC_UNLOCK();

   return result;
} // wfcCreateDevice()

//------------------------------------------------------------------------------

WFC_API_CALL WFCErrorCode WFC_APIENTRY
    wfcGetError(WFCDevice dev) WFC_APIEXIT
{
   WFCErrorCode result;

   WFC_LOCK();

   if(vcos_verify(dev != WFC_INVALID_HANDLE))
   {
      WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *) dev;

      result = device_ptr->error;
      device_ptr->error = WFC_ERROR_NONE;
   } // if
   else
      {result = WFC_ERROR_BAD_DEVICE;}

   WFC_UNLOCK();

   return result;
} // wfcGetError()

//------------------------------------------------------------------------------

WFC_API_CALL WFCint WFC_APIENTRY
    wfcGetDeviceAttribi(WFCDevice dev, WFCDeviceAttrib attrib) WFC_APIEXIT
{
   // Behaviour is unspecified if dev is null, so do something sensible.
   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
      {return 0;}

   WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *) dev;
   WFCint result = 0;

   WFC_LOCK();

   switch (attrib)
   {
      case WFC_DEVICE_CLASS:
         result = WFC_DEVICE_CLASS_FULLY_CAPABLE;
         break;
      case WFC_DEVICE_ID:
         result = WFC_OUR_DEVICE_ID;
         break;
      default:
         wfc_set_error(device_ptr, WFC_ERROR_BAD_ATTRIBUTE);
   } // switch

   WFC_UNLOCK();

   return result;
} // wfcGetDeviceAttribi()

//------------------------------------------------------------------------------

WFC_API_CALL WFCErrorCode WFC_APIENTRY
    wfcDestroyDevice(WFCDevice dev) WFC_APIEXIT
{
   WFCErrorCode result;

   WFC_LOCK();

   if(vcos_verify(dev != WFC_INVALID_HANDLE))
   {
      WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *) dev;

      // Destroy all of the contexts associated with the device. This will in turn
      // destroy all of the sources, masks and elements associated with each context.
      wfc_link_iterate(&device_ptr->contexts, (WFC_LINK_CALLBACK_T) wfc_context_destroy, NULL);

      khrn_platform_free(device_ptr);

      wfc_server_disconnect();

      result = WFC_ERROR_NONE;
   } // if
   else
      {result = WFC_ERROR_BAD_DEVICE;}

   WFC_UNLOCK();

   return result;
} // wfcDestroyDevice()

//==============================================================================
// Context functions

WFC_API_CALL WFCContext WFC_APIENTRY
    wfcCreateOnScreenContext(WFCDevice dev,
        WFCint screenNumber,
        const WFCint *attribList) WFC_APIEXIT
{
   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
      {return WFC_INVALID_HANDLE;}

   WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *) dev;

   WFCContext context = WFC_INVALID_HANDLE;

   WFC_LOCK();

   if (screenNumber < 0 || screenNumber >= WFC_ID_MAX_SCREENS)
      {wfc_set_error(device_ptr, WFC_ERROR_UNSUPPORTED);}
   else if (!wfc_check_no_attribs(attribList))
      {wfc_set_error(device_ptr, WFC_ERROR_BAD_ATTRIBUTE);}
   else
   {
      WFC_CONTEXT_T *context_ptr;

      WFCErrorCode error;

      // Create on-screen context_ptr
      context_ptr = wfc_context_create
         (device_ptr, WFC_CONTEXT_TYPE_ON_SCREEN, screenNumber, &error);

      // Insert new context_ptr into list of contexts
      if(context_ptr)
      {
         wfc_link_attach(&context_ptr->link, &device_ptr->contexts);

         context = (WFCContext) context_ptr;
      } // if
      else
         {wfc_set_error(device_ptr, error);}
   } // else

   WFC_UNLOCK();

   return context;
} // wfcCreateOnScreenContext()

//------------------------------------------------------------------------------

WFC_API_CALL WFCContext WFC_APIENTRY
    wfcCreateOffScreenContext(WFCDevice dev,
        WFCNativeStreamType stream,
        const WFCint *attribList) WFC_APIEXIT
{
   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
      {return WFC_INVALID_HANDLE;}

   WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *) dev;

   WFCContext context = WFC_INVALID_HANDLE;

   WFC_LOCK();

   if(stream == WFC_INVALID_HANDLE)
      {wfc_set_error(device_ptr, WFC_ERROR_ILLEGAL_ARGUMENT);}
   else if(wfc_stream_used_for_off_screen(stream))
      {wfc_set_error(device_ptr, WFC_ERROR_IN_USE);}
   else if (!wfc_check_no_attribs(attribList))
      {wfc_set_error(device_ptr, WFC_ERROR_BAD_ATTRIBUTE);}
   else
   {
      WFC_CONTEXT_T *context_ptr;

      WFCErrorCode error;

      // Create on-screen context_ptr
      context_ptr = wfc_context_create
         (device_ptr, WFC_CONTEXT_TYPE_OFF_SCREEN, stream, &error);

      // Insert new context_ptr into list of contexts
      if(context_ptr)
      {
         wfc_link_attach(&context_ptr->link, &device_ptr->contexts);

         context = (WFCContext) context_ptr;

         wfc_stream_register_off_screen(stream, true);
      } // if
      else
         {wfc_set_error(device_ptr, error);}
   } // else

   WFC_UNLOCK();

   return context;
} // wfcCreateOffScreenContext()

//------------------------------------------------------------------------------

/** Called when a scene for composition has been taken
 *
 * @param cb_data Callback data
 */
static void wfc_client_scene_taken_cb(void *cb_data)
{
   VCOS_SEMAPHORE_T *wait_sem = (VCOS_SEMAPHORE_T *)cb_data;

   vcos_assert(wait_sem != NULL);
   vcos_semaphore_post(wait_sem);
}

//------------------------------------------------------------------------------

/** Wait for the scene taken callback to have been called. Deletes the semaphore
 * and releases the VideoCore keep alive.
 *
 * @param wait_sem The wait semaphore.
 * @param ctx The handle for the context receiving the scene, used in logging.
 * @param calling_function The calling function name, used in logging.
 */
static void wfc_client_wait_for_scene_taken(VCOS_SEMAPHORE_T *wait_sem, WFCContext ctx,
      const char *calling_function)
{
   VCOS_STATUS_T status;

#if defined(VCOS_LOGGING_ENABLED)
   uint64_t pid = vcos_process_id_current();

   vcos_log_trace("%s: wait for compositor to take scene, context 0x%x pid 0x%x%08x",
         calling_function, ctx, (uint32_t)(pid >> 32), (uint32_t)pid);
#else
   vcos_unused(ctx);
   vcos_unused(calling_function);
#endif

   status = vcos_semaphore_wait(wait_sem);
   vcos_assert(status == VCOS_SUCCESS);
   vcos_unused(status);

   vcos_semaphore_delete(wait_sem);
   wfc_server_release_keep_alive();

   vcos_log_trace("%s: wait completed", calling_function);
}

//------------------------------------------------------------------------------

WFC_API_CALL void WFC_APIENTRY
    wfcCommit(WFCDevice dev, WFCContext ctx, WFCboolean wait) WFC_APIEXIT
{
   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
      {return;}

   WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *) dev;
   WFC_CONTEXT_T *context_ptr = (WFC_CONTEXT_T *) ctx;
   VCOS_STATUS_T status = VCOS_ENOSYS;
   VCOS_SEMAPHORE_T wait_sem;

   WFC_LOCK();

   vcos_log_trace("wfc_commit: dev 0x%X, ctx 0x%X", dev, ctx);

   // Send data for all elements
   if(vcos_verify((context_ptr != NULL) && (context_ptr->device_ptr == device_ptr)))
   {
      WFC_SCENE_T *scene = &context_ptr->committed_scene;

      memset(scene, 0, sizeof(*scene));
      // Store scene in committed_scene structure
      memcpy(&scene->context, &context_ptr->dynamic_attributes, sizeof(WFC_CONTEXT_DYNAMIC_ATTRIB_T));
      scene->element_count = 0;
      wfc_link_iterate(&context_ptr->elements_in_scene,
         (WFC_LINK_CALLBACK_T) wfc_commit_iterator, scene);

      if (context_ptr->active)
      {
         context_ptr->committed_scene.wait = (uint32_t)wait;

         if (wait)
         {
            // Long running operation, so keep VC alive until it completes.
            wfc_server_use_keep_alive();

            status = vcos_semaphore_create(&wait_sem, "WFC commit", 0);
            vcos_assert(status == VCOS_SUCCESS);   // On platforms we care about.

            do
            {
               status = wfc_server_compose_scene(ctx, &context_ptr->committed_scene,
                     wfc_client_scene_taken_cb, &wait_sem);

               if (status == VCOS_EAGAIN)
               {
                  // Another thread is competing for access to the context, so
                  // wait a little and try again.
                  vcos_sleep(1);
               }
            }
            while (status == VCOS_EAGAIN);

            if (status != VCOS_SUCCESS)
            {
               wfc_server_release_keep_alive();
               vcos_semaphore_delete(&wait_sem);
            }
         }
         else
         {
            status = wfc_server_compose_scene(ctx, &context_ptr->committed_scene,
                  NULL, NULL);
         }

         if (status != VCOS_SUCCESS)
         {
            vcos_log_info("%s: failed to compose scene: %d", VCOS_FUNCTION, status);
            wfc_set_error(device_ptr, WFC_ERROR_BUSY);
         }
      }
   } // if
   else
      {wfc_set_error(device_ptr, WFC_ERROR_BAD_HANDLE);}

   WFC_UNLOCK();

   // Wait for the scene to be taken outside the lock
   if (wait && status == VCOS_SUCCESS)
   {
      wfc_client_wait_for_scene_taken(&wait_sem, ctx, VCOS_FUNCTION);
   }
} // wfcCommit()

//------------------------------------------------------------------------------

WFC_API_CALL WFCint WFC_APIENTRY
    wfcGetContextAttribi(WFCDevice dev, WFCContext ctx,
        WFCContextAttrib attrib) WFC_APIEXIT
{
   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
      {return 0;}

   WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *) dev;
   WFC_CONTEXT_T *context_ptr = (WFC_CONTEXT_T *) ctx;

   WFCint result = 0;

   WFC_LOCK();

   if(vcos_verify((context_ptr != NULL) && (context_ptr->device_ptr == device_ptr)))
   {
      switch (attrib)
      {
         case WFC_CONTEXT_TYPE:
            result = context_ptr->static_attributes.type;
            break;
         case WFC_CONTEXT_TARGET_WIDTH:
            result = context_ptr->static_attributes.width;
            break;
         case WFC_CONTEXT_TARGET_HEIGHT:
            result = context_ptr->static_attributes.height;
            break;
         case WFC_CONTEXT_LOWEST_ELEMENT:
         {
            WFC_LINK_T *first = &context_ptr->elements_in_scene;
            WFC_LINK_T *current = first;

            if(first->next == first)
            {
               // List is empty.
               result = WFC_INVALID_HANDLE;
            } // if
            else
            {
               // Move to last element in list.
               do
                  {current = current->next;}
               while(current->next != first);
               result = (WFCint) current;
            } // else
            break;
         }
         case WFC_CONTEXT_ROTATION:
            result = context_ptr->dynamic_attributes.rotation;
            break;
         case WFC_CONTEXT_BG_COLOR:
            result = (WFCint) (context_ptr->dynamic_attributes.background_clr[WFC_BG_CLR_RED]    * 255.0f) << 24 |
                     (WFCint) (context_ptr->dynamic_attributes.background_clr[WFC_BG_CLR_GREEN]  * 255.0f) << 16 |
                     (WFCint) (context_ptr->dynamic_attributes.background_clr[WFC_BG_CLR_BLUE]   * 255.0f) << 8  |
                     (WFCint) (context_ptr->dynamic_attributes.background_clr[WFC_BG_CLR_ALPHA]  * 255.0f);
            break;
         default:
            wfc_set_error(device_ptr, WFC_ERROR_BAD_ATTRIBUTE);
            break;
      } // switch
   } // if
   else
      {wfc_set_error(device_ptr, WFC_ERROR_BAD_HANDLE);}

   WFC_UNLOCK();

   return result;
} // wfcGetContextAttribi()

//------------------------------------------------------------------------------

WFC_API_CALL void WFC_APIENTRY
    wfcGetContextAttribfv(WFCDevice dev, WFCContext ctx,
        WFCContextAttrib attrib, WFCint count, WFCfloat *values) WFC_APIEXIT
{
   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
      {return;}

   WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *) dev;
   WFC_CONTEXT_T *context_ptr = (WFC_CONTEXT_T *) ctx;

   WFC_LOCK();

   if(vcos_verify((context_ptr != NULL) && (context_ptr->device_ptr == device_ptr)))
   {
      uint32_t i;

      switch (attrib)
      {
         case WFC_CONTEXT_BG_COLOR:
            if(vcos_verify((values != NULL)
               && (((uint32_t) values && 0x3) == 0) && (count == WFC_BG_CLR_SIZE)))
            {
               for (i = 0; i < WFC_BG_CLR_SIZE; i++)
                  {values[i] = context_ptr->dynamic_attributes.background_clr[i];}
            } // if
            else
               {wfc_set_error(device_ptr, WFC_ERROR_ILLEGAL_ARGUMENT);}
            break;
         default:
            wfc_set_error(device_ptr, WFC_ERROR_BAD_ATTRIBUTE);
            break;
      } // switch
   }
   else
      {wfc_set_error(device_ptr, WFC_ERROR_BAD_HANDLE);}

   WFC_UNLOCK();
} // wfcGetContextAttribfv()

//------------------------------------------------------------------------------

WFC_API_CALL void WFC_APIENTRY
    wfcSetContextAttribi(WFCDevice dev, WFCContext ctx,
        WFCContextAttrib attrib, WFCint value) WFC_APIEXIT
{
   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
      {return;}

   WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *) dev;
   WFC_CONTEXT_T *context_ptr = (WFC_CONTEXT_T *) ctx;

   WFC_LOCK();

   if(vcos_verify((context_ptr != NULL) && (context_ptr->device_ptr == device_ptr)))
   {
      switch(attrib)
      {
         case WFC_CONTEXT_ROTATION:
            if(wfc_is_rotation(value))
               {context_ptr->dynamic_attributes.rotation = value;}
            else
               {wfc_set_error(device_ptr, WFC_ERROR_ILLEGAL_ARGUMENT);}
            break;
         case WFC_CONTEXT_BG_COLOR:
         {
            int32_t i;
            for(i = WFC_BG_CLR_SIZE - 1; i >= 0; i--)
            {
               context_ptr->dynamic_attributes.background_clr[i]
                  = ((float) (value & 0xff)) / 255.0f;
               value >>= 8;
            } // for
            break;
         }
         default:
            wfc_set_error(device_ptr, WFC_ERROR_BAD_ATTRIBUTE);
            break;
      } // switch
   } // if
   else
      {wfc_set_error(device_ptr, WFC_ERROR_BAD_HANDLE);}

   WFC_UNLOCK();
} // wfcSetContextAttribi()

//------------------------------------------------------------------------------

WFC_API_CALL void WFC_APIENTRY
    wfcSetContextAttribfv(WFCDevice dev, WFCContext ctx,
        WFCContextAttrib attrib,
        WFCint count, const WFCfloat *values) WFC_APIEXIT
{
   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
      {return;}

   WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *)dev;
   WFC_CONTEXT_T *context_ptr = (WFC_CONTEXT_T *)ctx;

   WFC_LOCK();

   if(vcos_verify((context_ptr != NULL) && (context_ptr->device_ptr == device_ptr)))
   {
      int i;

      switch (attrib)
      {
         case WFC_CONTEXT_BG_COLOR:
            if(vcos_verify((values != NULL) && (((uint32_t) values & 0x3) == 0)
               && (count == WFC_BG_CLR_SIZE)))
            {
               for (i = 0; i < WFC_BG_CLR_SIZE; i++)
                  {context_ptr->dynamic_attributes.background_clr[i] = values[i];}
            } // if
            else
               {wfc_set_error(device_ptr, WFC_ERROR_ILLEGAL_ARGUMENT);}
            break;
         default:
            wfc_set_error(device_ptr, WFC_ERROR_BAD_ATTRIBUTE);
            break;
      } // switch
   } // if
   else
      {wfc_set_error(device_ptr, WFC_ERROR_BAD_HANDLE);}

   WFC_UNLOCK();
} // wfcSetContextAttribfv()

//------------------------------------------------------------------------------

WFC_API_CALL void WFC_APIENTRY
    wfcDestroyContext(WFCDevice dev, WFCContext ctx) WFC_APIEXIT
{
   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
      {return;}

   WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *) dev;
   WFC_CONTEXT_T *context_ptr = (WFC_CONTEXT_T *) ctx;

   vcos_log_trace("wfc_destroy_context: context = 0x%X", ctx);

   WFC_LOCK();

   if(vcos_verify((context_ptr != NULL) && (context_ptr->device_ptr == device_ptr)))
   {
      wfc_context_destroy(context_ptr);
   }
   else
      {wfc_set_error(device_ptr, WFC_ERROR_BAD_HANDLE);}

   WFC_UNLOCK();
} // wfcDestroyContext()

//==============================================================================
// Source functions

WFC_API_CALL WFCSource WFC_APIENTRY wfcCreateSourceFromStream(WFCDevice dev, WFCContext ctx,
        WFCNativeStreamType stream,
        const WFCint *attribList) WFC_APIEXIT
{
   WFCSource source;

   WFC_LOCK();

   source = (WFCSource) wfc_source_or_mask_create(WFC_IS_SOURCE, dev, ctx, stream, attribList);

   WFC_UNLOCK();

   return source;
} // wfcCreateSourceFromStream()

//------------------------------------------------------------------------------

WFC_API_CALL void WFC_APIENTRY
    wfcDestroySource(WFCDevice dev, WFCSource src) WFC_APIEXIT
{
   vcos_log_trace("wfc_destroy_source: source = 0x%X", src);

   WFC_LOCK();
   wfc_source_or_mask_destroy(dev, (WFCHandle) src);
   WFC_UNLOCK();
} // wfcDestroySource()

//==============================================================================
// Mask functions

WFC_API_CALL WFCMask WFC_APIENTRY
    wfcCreateMaskFromStream(WFCDevice dev, WFCContext ctx,
        WFCNativeStreamType stream,
        const WFCint *attribList) WFC_APIEXIT
{
   WFCMask mask;

   WFC_LOCK();

   mask = (WFCMask) wfc_source_or_mask_create(WFC_IS_MASK, dev, ctx, stream, attribList);

   WFC_UNLOCK();

   return mask;
} // wfcCreateMaskFromStream()

//------------------------------------------------------------------------------

WFC_API_CALL void WFC_APIENTRY
    wfcDestroyMask(WFCDevice dev, WFCMask msk) WFC_APIEXIT
{
   WFC_LOCK();
   wfc_source_or_mask_destroy(dev, (WFCHandle) msk);
   WFC_UNLOCK();
} // wfcDestroyMask()

//==============================================================================
// Element functions

WFC_API_CALL WFCElement WFC_APIENTRY
    wfcCreateElement(WFCDevice dev, WFCContext ctx,
        const WFCint *attribList) WFC_APIEXIT
{
   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
      {return WFC_INVALID_HANDLE;}

   WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *) dev;
   WFC_CONTEXT_T *context_ptr = (WFC_CONTEXT_T *) ctx;

   WFCElement element = WFC_INVALID_HANDLE;

   WFC_LOCK();

   if(vcos_verify((context_ptr != NULL) && (context_ptr->device_ptr == device_ptr)))
   {
      if(wfc_check_no_attribs(attribList))
      {
         WFC_ELEMENT_T *element_ptr =
            khrn_platform_malloc(sizeof(WFC_ELEMENT_T), "WFC_ELEMENT_T");
         const WFC_ELEMENT_ATTRIB_T element_attrib_default = WFC_ELEMENT_ATTRIB_DEFAULT;

         if(element_ptr != NULL)
         {
            memset(element_ptr, 0, sizeof(WFC_ELEMENT_T));

            wfc_link_init_null(&element_ptr->link);
            element_ptr->context_ptr = context_ptr;
            element_ptr->attributes = element_attrib_default;

            wfc_link_attach(&element_ptr->link, &context_ptr->elements_not_in_scene);

            element = (WFCElement) element_ptr;
         } // if
         else
            {wfc_set_error(device_ptr, WFC_ERROR_OUT_OF_MEMORY);}
      } // if
      else
         {wfc_set_error(device_ptr, WFC_ERROR_BAD_ATTRIBUTE);}
   } // if
   else
      {wfc_set_error(device_ptr, WFC_ERROR_BAD_HANDLE);}

   WFC_UNLOCK();

   return element;
} // wfcCreateElement()

//------------------------------------------------------------------------------

WFC_API_CALL WFCint WFC_APIENTRY
    wfcGetElementAttribi(WFCDevice dev, WFCElement elm,
        WFCElementAttrib attrib) WFC_APIEXIT
{
   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
      {return 0;}

   WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *)dev;
   WFC_ELEMENT_T *element_ptr = (WFC_ELEMENT_T *)elm;

   WFCint result = 0;

   WFC_LOCK();

   if(vcos_verify((element_ptr != NULL) && (element_ptr->context_ptr != NULL)
      && (element_ptr->context_ptr->device_ptr == device_ptr)))
   {
      switch (attrib)
      {
         case WFC_ELEMENT_SOURCE:
            result = (WFCint)element_ptr->source_ptr;
            break;
         case WFC_ELEMENT_SOURCE_FLIP:
            result = element_ptr->attributes.flip;
            break;
         case WFC_ELEMENT_SOURCE_ROTATION:
            result = element_ptr->attributes.rotation;
            break;
         case WFC_ELEMENT_SOURCE_SCALE_FILTER:
            result = element_ptr->attributes.scale_filter;
            break;
         case WFC_ELEMENT_TRANSPARENCY_TYPES:
            result = element_ptr->attributes.transparency_types;
            break;
         case WFC_ELEMENT_GLOBAL_ALPHA:
            result = wfc_round(element_ptr->attributes.global_alpha * 255.0f);
            break;
         case WFC_ELEMENT_MASK:
            result = (WFCint)element_ptr->mask_ptr;
            break;
         default:
            wfc_set_error(device_ptr, WFC_ERROR_BAD_ATTRIBUTE);
            break;
      } // switch
   } // if
   else
      {wfc_set_error(device_ptr, WFC_ERROR_BAD_HANDLE);}

   WFC_UNLOCK();

   return result;
} // wfcGetElementAttribi()

//------------------------------------------------------------------------------

WFC_API_CALL WFCfloat WFC_APIENTRY
    wfcGetElementAttribf(WFCDevice dev, WFCElement elm,
        WFCElementAttrib attrib) WFC_APIEXIT
{
   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
      {return 0;}

   WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *)dev;
   WFC_ELEMENT_T *element_ptr = (WFC_ELEMENT_T *)elm;

   WFCfloat result = 0;

   WFC_LOCK();

   if(vcos_verify((element_ptr != NULL) && (element_ptr->context_ptr != NULL)
      && (element_ptr->context_ptr->device_ptr == device_ptr)))
   {
      switch (attrib) {
      case WFC_ELEMENT_GLOBAL_ALPHA:
         result = element_ptr->attributes.global_alpha;
         break;
      default:
         wfc_set_error(device_ptr, WFC_ERROR_BAD_ATTRIBUTE);
         break;
      }
   }
   else
      {wfc_set_error(device_ptr, WFC_ERROR_BAD_HANDLE);}

   WFC_UNLOCK();

   return result;
} // wfcGetElementAttribf()

//------------------------------------------------------------------------------

WFC_API_CALL void WFC_APIENTRY
    wfcGetElementAttribiv(WFCDevice dev, WFCElement elm,
        WFCElementAttrib attrib, WFCint count, WFCint *values) WFC_APIEXIT
{
   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
      {return;}

   WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *)dev;
   WFC_ELEMENT_T *element_ptr = (WFC_ELEMENT_T *)elm;

   WFC_LOCK();

   if(vcos_verify((element_ptr != NULL) && (element_ptr->context_ptr != NULL)
      && (element_ptr->context_ptr->device_ptr == device_ptr)))
   {
      int i;

      switch (attrib) {
      case WFC_ELEMENT_DESTINATION_RECTANGLE:
         if (values && count == WFC_RECT_SIZE)
            for (i = 0; i < WFC_RECT_SIZE; i++)
               values[i] = element_ptr->attributes.dest_rect[i];
         else
            wfc_set_error(device_ptr, WFC_ERROR_ILLEGAL_ARGUMENT);
         break;
      case WFC_ELEMENT_SOURCE_RECTANGLE:
         if (values && !((size_t)values & 3) && count == WFC_RECT_SIZE)
         {
            for (i = 0; i < WFC_RECT_SIZE; i++)
               values[i] = (WFCint) element_ptr->attributes.src_rect[i];
         } // if
         else
            wfc_set_error(device_ptr, WFC_ERROR_ILLEGAL_ARGUMENT);
         break;
      default:
         wfc_set_error(device_ptr, WFC_ERROR_BAD_ATTRIBUTE);
         break;
      }
   }
   else
      {wfc_set_error(device_ptr, WFC_ERROR_BAD_HANDLE);}

   WFC_UNLOCK();
} // wfcGetElementAttribiv()

//------------------------------------------------------------------------------

WFC_API_CALL void WFC_APIENTRY
    wfcGetElementAttribfv(WFCDevice dev, WFCElement elm,
        WFCElementAttrib attrib, WFCint count, WFCfloat *values) WFC_APIEXIT
{
   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
      {return;}

   WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *)dev;
   WFC_ELEMENT_T *element_ptr = (WFC_ELEMENT_T *)elm;

   WFC_LOCK();

   if(vcos_verify((element_ptr != NULL) && (element_ptr->context_ptr != NULL)
      && (element_ptr->context_ptr->device_ptr == device_ptr)))
   {
      uint32_t i;

      switch(attrib)
      {
         case WFC_ELEMENT_DESTINATION_RECTANGLE:
            if(values && !((size_t)values & 3) && (count == WFC_RECT_SIZE))
            {
               for (i = 0; i < WFC_RECT_SIZE; i++)
                  {values[i] = (WFCfloat) element_ptr->attributes.dest_rect[i];}
            } // if
            else
               {wfc_set_error(device_ptr, WFC_ERROR_ILLEGAL_ARGUMENT);}
            break;
         case WFC_ELEMENT_SOURCE_RECTANGLE:
            if(values && !((size_t)values & 3) && (count == WFC_RECT_SIZE))
            {
               for (i = 0; i < WFC_RECT_SIZE; i++)
                  {values[i] = element_ptr->attributes.src_rect[i];}
            } // if
            else
               {wfc_set_error(device_ptr, WFC_ERROR_ILLEGAL_ARGUMENT);}
            break;
         default:
            wfc_set_error(device_ptr, WFC_ERROR_BAD_ATTRIBUTE);
            break;
      } // switch
   } // if
   else
      {wfc_set_error(device_ptr, WFC_ERROR_BAD_HANDLE);}

   WFC_UNLOCK();
} // wfcGetElementAttribfv()

//------------------------------------------------------------------------------

WFC_API_CALL void WFC_APIENTRY
    wfcSetElementAttribi(WFCDevice dev, WFCElement elm,
        WFCElementAttrib attrib, WFCint value) WFC_APIEXIT
{
   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
      {return;}

   WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *)dev;
   WFC_ELEMENT_T *element_ptr = (WFC_ELEMENT_T *)elm;

   WFC_LOCK();

   if(vcos_verify((element_ptr != NULL) && (element_ptr->context_ptr != NULL)
      && (element_ptr->context_ptr->device_ptr == device_ptr)))
   {
      switch (attrib)
      {
         case WFC_ELEMENT_SOURCE:
         {
            WFC_SOURCE_OR_MASK_T *new_source_ptr = (WFC_SOURCE_OR_MASK_T *) value;

            if
            (
               ((new_source_ptr != NULL) && (element_ptr->context_ptr == new_source_ptr->context_ptr))
               || (new_source_ptr == NULL)
            )
            {
               wfc_source_or_mask_release(element_ptr->source_ptr);
               element_ptr->source_ptr = new_source_ptr;
               wfc_source_or_mask_acquire(element_ptr->source_ptr);

               element_ptr->attributes.source_stream =
                  element_ptr->source_ptr ? element_ptr->source_ptr->stream : WFC_INVALID_HANDLE;
            } // if
            else
               {wfc_set_error(device_ptr, WFC_ERROR_ILLEGAL_ARGUMENT);}
            break;
         }
         case WFC_ELEMENT_SOURCE_FLIP:
         {
            if (value == WFC_FALSE || value == WFC_TRUE)
               element_ptr->attributes.flip = value;
            else
               wfc_set_error(device_ptr, WFC_ERROR_ILLEGAL_ARGUMENT);

            break;
         }
         case WFC_ELEMENT_SOURCE_ROTATION:
         {
            if (wfc_is_rotation(value))
               element_ptr->attributes.rotation = value;
            else
               wfc_set_error(device_ptr, WFC_ERROR_ILLEGAL_ARGUMENT);

            break;
         }
         case WFC_ELEMENT_SOURCE_SCALE_FILTER:
         {
            if (wfc_is_scale_filter(value))
               element_ptr->attributes.scale_filter = value;
            else
               wfc_set_error(device_ptr, WFC_ERROR_ILLEGAL_ARGUMENT);

            break;
         }
         case WFC_ELEMENT_TRANSPARENCY_TYPES:
         {
            if (wfc_are_transparency_types(value))
               element_ptr->attributes.transparency_types = value;
            else
               wfc_set_error(device_ptr, WFC_ERROR_ILLEGAL_ARGUMENT);

            break;
         }
         case WFC_ELEMENT_GLOBAL_ALPHA:
         {
            if (value >= 0 && value <= 255)
               element_ptr->attributes.global_alpha = (float)value / 255.0f;
            else
               wfc_set_error(device_ptr, WFC_ERROR_ILLEGAL_ARGUMENT);

            break;
         }
         case WFC_ELEMENT_MASK:
         {
            WFC_SOURCE_OR_MASK_T *new_mask_ptr = (WFC_SOURCE_OR_MASK_T *) value;

            if
            (
               ((new_mask_ptr != NULL) && (element_ptr->context_ptr == new_mask_ptr->context_ptr))
               || (new_mask_ptr == NULL)
            )
            {
               wfc_source_or_mask_release(element_ptr->mask_ptr);
               element_ptr->mask_ptr = new_mask_ptr;
               wfc_source_or_mask_acquire(element_ptr->mask_ptr);

               element_ptr->attributes.mask_stream =
                  element_ptr->mask_ptr ? element_ptr->mask_ptr->stream : WFC_INVALID_HANDLE;
            } // if
            else
               {wfc_set_error(device_ptr, WFC_ERROR_ILLEGAL_ARGUMENT);}
            break;
         }
         default:
            wfc_set_error(device_ptr, WFC_ERROR_BAD_ATTRIBUTE);
            break;
      } // switch
   } // if
   else
      {wfc_set_error(device_ptr, WFC_ERROR_BAD_HANDLE);}

   WFC_UNLOCK();
} // wfcSetElementAttribi()

//------------------------------------------------------------------------------

WFC_API_CALL void WFC_APIENTRY
    wfcSetElementAttribf(WFCDevice dev, WFCElement elm,
        WFCElementAttrib attrib, WFCfloat value) WFC_APIEXIT
{
   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
      {return;}

   WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *)dev;
   WFC_ELEMENT_T *element_ptr = (WFC_ELEMENT_T *)elm;

   WFC_LOCK();

   if(vcos_verify((element_ptr != NULL) && (element_ptr->context_ptr != NULL)
      && (element_ptr->context_ptr->device_ptr == device_ptr)))
   {
      switch(attrib)
      {
         case WFC_ELEMENT_GLOBAL_ALPHA:
            if (value >= 0.0f && value <= 1.0f)
               element_ptr->attributes.global_alpha = value;
            else
               wfc_set_error(device_ptr, WFC_ERROR_ILLEGAL_ARGUMENT);
            break;
         default:
            wfc_set_error(device_ptr, WFC_ERROR_BAD_ATTRIBUTE);
            break;
      } // switch
   } // if
   else
      wfc_set_error(device_ptr, WFC_ERROR_BAD_HANDLE);

   WFC_UNLOCK();
} // wfcSetElementAttribf()

//------------------------------------------------------------------------------

WFC_API_CALL void WFC_APIENTRY
    wfcSetElementAttribiv(WFCDevice dev, WFCElement elm,
        WFCElementAttrib attrib,
        WFCint count, const WFCint *values) WFC_APIEXIT
{
   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
      {return;}

   WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *)dev;
   WFC_ELEMENT_T *element_ptr = (WFC_ELEMENT_T *)elm;

   WFC_LOCK();

   if(vcos_verify((element_ptr != NULL) && (element_ptr->context_ptr != NULL)
      && (element_ptr->context_ptr->device_ptr == device_ptr)))
   {
      uint32_t i;

      switch (attrib) {
      case WFC_ELEMENT_DESTINATION_RECTANGLE:
         if (values && !((size_t)values & 3) && count == WFC_RECT_SIZE)
         {
            for (i = 0; i < WFC_RECT_SIZE; i++)
               {element_ptr->attributes.dest_rect[i] = values[i];}
         } // if
         else
            {wfc_set_error(device_ptr, WFC_ERROR_ILLEGAL_ARGUMENT);}
         break;
      case WFC_ELEMENT_SOURCE_RECTANGLE:
         if (values && !((size_t)values & 3) && count == WFC_RECT_SIZE)
         {
            // TODO verify that source rectangle is within the source image.
            for (i = 0; i < WFC_RECT_SIZE; i++)
               {element_ptr->attributes.src_rect[i] = (float)values[i];}
         } // if
         else
            {wfc_set_error(device_ptr, WFC_ERROR_ILLEGAL_ARGUMENT);}
         break;
      default:
         wfc_set_error(device_ptr, WFC_ERROR_BAD_ATTRIBUTE);
         break;
      } // switch
   } // if
   else
      {wfc_set_error(device_ptr, WFC_ERROR_BAD_HANDLE);}

   WFC_UNLOCK();
} // wfcSetElementAttribiv()

//------------------------------------------------------------------------------

WFC_API_CALL void WFC_APIENTRY
    wfcSetElementAttribfv(WFCDevice dev, WFCElement elm,
        WFCElementAttrib attrib,
        WFCint count, const WFCfloat *values) WFC_APIEXIT
{
   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
      {return;}

   WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *)dev;
   WFC_ELEMENT_T *element_ptr = (WFC_ELEMENT_T *)elm;

   WFC_LOCK();

   if(vcos_verify((element_ptr != NULL) && (element_ptr->context_ptr != NULL)
      && (element_ptr->context_ptr->device_ptr == device_ptr)))
   {
      uint32_t i;

      switch (attrib)
      {
         case WFC_ELEMENT_DESTINATION_RECTANGLE:
            if (values && !((size_t)values & 3) && count == WFC_RECT_SIZE)
            {
               for (i = 0; i < WFC_RECT_SIZE; i++)
                  {element_ptr->attributes.dest_rect[i] = (int)values[i];}
            } // if
            else
               {wfc_set_error(device_ptr, WFC_ERROR_ILLEGAL_ARGUMENT);}
            break;
         case WFC_ELEMENT_SOURCE_RECTANGLE:
            if (values && !((size_t)values & 3) && count == WFC_RECT_SIZE)
            {
               // TODO verify that source rectangle is within the source image.
               // Note that these are floats, so check if difference is < 0.00001,
               // as per spec, p37 (PDF p42)
               for (i = 0; i < WFC_RECT_SIZE; i++)
                  {element_ptr->attributes.src_rect[i] = values[i];}
            } // if
            else
               {wfc_set_error(device_ptr, WFC_ERROR_ILLEGAL_ARGUMENT);}
            break;
         default:
            wfc_set_error(device_ptr, WFC_ERROR_BAD_ATTRIBUTE);
            break;
      } // switch
   } // if
   else
      {wfc_set_error(device_ptr, WFC_ERROR_BAD_HANDLE);}

   WFC_UNLOCK();
} // wfcSetElementAttribfv()

//------------------------------------------------------------------------------

WFC_API_CALL void WFC_APIENTRY
    wfcInsertElement(WFCDevice dev, WFCElement elm, WFCElement subordinate) WFC_APIEXIT
{
   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
      {return;}

   WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *) dev;
   WFC_ELEMENT_T *element_ptr = (WFC_ELEMENT_T *) elm;
   WFC_ELEMENT_T *subordinate_ptr = (WFC_ELEMENT_T *) subordinate;

   WFC_LOCK();

   if(vcos_verify
   (
      (element_ptr != NULL) && (element_ptr->context_ptr != NULL)
         && (element_ptr->context_ptr->device_ptr == device_ptr)
      &&
      (
         (subordinate_ptr == NULL)
         || ((subordinate_ptr->context_ptr != NULL)
            && (subordinate_ptr->context_ptr->device_ptr == device_ptr))
      )
   ))
   {
      if(subordinate_ptr != NULL)
      {
         // Insert element after subordinate.
         if((element_ptr->context_ptr == subordinate_ptr->context_ptr) && subordinate_ptr->is_in_scene)
         {
            wfc_link_attach(&element_ptr->link, &subordinate_ptr->link);
            element_ptr->is_in_scene = true;
         }
         else
            {wfc_set_error(device_ptr, WFC_ERROR_ILLEGAL_ARGUMENT);}
      } // if
      else
      {
         // Insert at the "bottom of the scene" - which is the end of the list.
         wfc_link_attach(&element_ptr->link, &element_ptr->context_ptr->elements_in_scene);
         element_ptr->is_in_scene = true;
      } // else
   }
   else
      {wfc_set_error(device_ptr, WFC_ERROR_BAD_HANDLE);}

   WFC_UNLOCK();
} // wfcInsertElement()

//------------------------------------------------------------------------------

WFC_API_CALL void WFC_APIENTRY
    wfcRemoveElement(WFCDevice dev, WFCElement elm) WFC_APIEXIT
{
   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
      {return;}

   WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *) dev;
   WFC_ELEMENT_T *element_ptr = (WFC_ELEMENT_T *) elm;

   WFC_LOCK();

   if(vcos_verify((element_ptr != NULL) && (element_ptr->context_ptr != NULL)
      && (element_ptr->context_ptr->device_ptr == device_ptr)))
   {
      wfc_link_attach(&element_ptr->link, &element_ptr->context_ptr->elements_not_in_scene);
      element_ptr->is_in_scene = false;
   }
   else
      {wfc_set_error(device_ptr, WFC_ERROR_BAD_HANDLE);}

   WFC_UNLOCK();
} // wfcRemoveElement()

//------------------------------------------------------------------------------

WFC_API_CALL WFCElement WFC_APIENTRY
    wfcGetElementAbove(WFCDevice dev, WFCElement elm) WFC_APIEXIT
{
   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
      {return WFC_INVALID_HANDLE;}

   WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *)dev;
   WFC_ELEMENT_T *element_ptr = (WFC_ELEMENT_T *)elm;

   WFCElement result = WFC_INVALID_HANDLE;

   WFC_LOCK();

   if(vcos_verify((element_ptr != NULL) && (element_ptr->context_ptr != NULL)
      && (element_ptr->context_ptr->device_ptr == device_ptr)))
   {
      if (element_ptr->is_in_scene)
      {
         if (element_ptr->link.next != &element_ptr->context_ptr->elements_in_scene)
            {result = (WFCElement)element_ptr->link.next;}
      }
      else
         {wfc_set_error(device_ptr, WFC_ERROR_ILLEGAL_ARGUMENT);}
   }
   else
      {wfc_set_error(device_ptr, WFC_ERROR_BAD_HANDLE);}

   WFC_UNLOCK();

   return result;
} // wfcGetElementAbove()

//------------------------------------------------------------------------------

WFC_API_CALL WFCElement WFC_APIENTRY
    wfcGetElementBelow(WFCDevice dev, WFCElement elm) WFC_APIEXIT
{
   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
      {return WFC_INVALID_HANDLE;}

   WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *) dev;
   WFC_ELEMENT_T *element_ptr = (WFC_ELEMENT_T *) elm;

   WFCElement result = WFC_INVALID_HANDLE;

   WFC_LOCK();

   if(vcos_verify((element_ptr != NULL) && (element_ptr->context_ptr != NULL)
      && (element_ptr->context_ptr->device_ptr == device_ptr)))
   {
      if (element_ptr->is_in_scene)
      {
         if (element_ptr->link.prev != &element_ptr->context_ptr->elements_in_scene)
            {result = (WFCElement) element_ptr->link.prev;}
      }
      else
         {wfc_set_error(device_ptr, WFC_ERROR_ILLEGAL_ARGUMENT);}
   }
   else
      {wfc_set_error(device_ptr, WFC_ERROR_BAD_HANDLE);}

   WFC_UNLOCK();

   return result;
} // wfcGetElementBelow()

//------------------------------------------------------------------------------

WFC_API_CALL void WFC_APIENTRY
    wfcDestroyElement(WFCDevice dev, WFCElement elm) WFC_APIEXIT
{
   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
      {return;}

   vcos_log_trace("wfc_destroy_element: element = 0x%X", elm);

   WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *) dev;
   WFC_ELEMENT_T *element_ptr = (WFC_ELEMENT_T *) elm;

   WFC_LOCK();

   if(vcos_verify((element_ptr != NULL) && (element_ptr->context_ptr != NULL)
      && (element_ptr->context_ptr->device_ptr == device_ptr)))
      {wfc_element_destroy(element_ptr, NULL);}
   else
      {wfc_set_error(device_ptr, WFC_ERROR_BAD_HANDLE);}

   WFC_UNLOCK();
} // wfcDestroyElement()

//==============================================================================
// Rendering

WFC_API_CALL void WFC_APIENTRY
    wfcActivate(WFCDevice dev, WFCContext ctx) WFC_APIEXIT
{
   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
      {return;}

   WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *)dev;
   WFC_CONTEXT_T *context_ptr = (WFC_CONTEXT_T *)ctx;
   VCOS_STATUS_T status = VCOS_ENOSYS;
   VCOS_SEMAPHORE_T wait_sem;

   WFC_LOCK();

   if(vcos_verify((context_ptr != NULL) && (context_ptr->device_ptr == device_ptr)))
   {
      wfc_server_activate(ctx);

      context_ptr->active = true;
      context_ptr->committed_scene.wait = 1;    // Wait for scene to be taken

      // Long running operation, so keep VC alive until it completes.
      wfc_server_use_keep_alive();

      status = vcos_semaphore_create(&wait_sem, "WFC activate", 0);
      vcos_assert(status == VCOS_SUCCESS);      // On platforms we care about.

      do
      {
         status = wfc_server_compose_scene(ctx, &context_ptr->committed_scene,
               wfc_client_scene_taken_cb, &wait_sem);

         if (status == VCOS_EAGAIN)
         {
            // Another thread is competing for access to the context, so
            // wait a little and try again.
            vcos_sleep(1);
         }
      }
      while (status == VCOS_EAGAIN);

      if (status != VCOS_SUCCESS)
      {
         vcos_semaphore_delete(&wait_sem);
         wfc_server_release_keep_alive();
         wfc_set_error(device_ptr, WFC_ERROR_BAD_HANDLE);
      }
   } //if
   else
      {wfc_set_error(device_ptr, WFC_ERROR_BAD_HANDLE);}

   WFC_UNLOCK();

   // Wait for the scene to be taken outside the lock
   if (status == VCOS_SUCCESS)
   {
      wfc_client_wait_for_scene_taken(&wait_sem, ctx, VCOS_FUNCTION);
   }
} // wfcActivate()

//------------------------------------------------------------------------------

WFC_API_CALL void WFC_APIENTRY
    wfcDeactivate(WFCDevice dev, WFCContext ctx) WFC_APIEXIT
{
   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
      {return;}

   WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *)dev;
   WFC_CONTEXT_T *context_ptr = (WFC_CONTEXT_T *)ctx;

   WFC_LOCK();

   if(vcos_verify((context_ptr != NULL) && (context_ptr->device_ptr == device_ptr)))
   {
      wfc_server_deactivate(ctx);

      context_ptr->active = false;
   } else
      wfc_set_error(device_ptr, WFC_ERROR_BAD_HANDLE);

   WFC_UNLOCK();
} // wfcDeactivate()

//------------------------------------------------------------------------------

WFC_API_CALL void WFC_APIENTRY
    wfcCompose(WFCDevice dev, WFCContext ctx, WFCboolean wait) WFC_APIEXIT
{
   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
      {return;}

   WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *) dev;
   WFC_CONTEXT_T *context_ptr = (WFC_CONTEXT_T *) ctx;
   VCOS_STATUS_T status = VCOS_ENOSYS;
   VCOS_SEMAPHORE_T wait_sem;

   WFC_LOCK();

   if(vcos_verify((context_ptr != NULL) && (context_ptr->device_ptr == device_ptr)))
   {
      // Only tell the server to compose if the context is not active, since it will
      // be automatically composed on commit when active
      if(!context_ptr->active)
      {
         context_ptr->committed_scene.wait = (uint32_t)wait;

         if (wait)
         {
            // Long running operation, so keep VC alive until it completes.
            wfc_server_use_keep_alive();

            status = vcos_semaphore_create(&wait_sem, "WFC compose", 0);
            vcos_assert(status == VCOS_SUCCESS);   // On platforms we care about.

            do
            {
               status = wfc_server_compose_scene(ctx, &context_ptr->committed_scene,
                     wfc_client_scene_taken_cb, &wait_sem);

               if (status == VCOS_EAGAIN)
               {
                  // Another thread is competing for access to the context, so
                  // wait a little and try again.
                  vcos_sleep(1);
               }
            }
            while (status == VCOS_EAGAIN);

            if (status != VCOS_SUCCESS)
            {
               vcos_semaphore_delete(&wait_sem);
               wfc_server_release_keep_alive();
            }
         }
         else
         {
            status = wfc_server_compose_scene(ctx, &context_ptr->committed_scene,
                  NULL, NULL);
         }

         if (status != VCOS_SUCCESS)
         {
            vcos_log_info("%s: failed to compose scene: %d", VCOS_FUNCTION, status);
            wfc_set_error(device_ptr, WFC_ERROR_BUSY);
         }
      } // if
      else
         {wfc_set_error(device_ptr, WFC_ERROR_UNSUPPORTED);}
   }
   else
      {wfc_set_error(device_ptr, WFC_ERROR_BAD_HANDLE);}

   WFC_UNLOCK();

   // Wait for the scene to be taken outside the lock
   if (wait && status == VCOS_SUCCESS)
   {
      wfc_client_wait_for_scene_taken(&wait_sem, ctx, VCOS_FUNCTION);
   }
} // wfcCompose()

//------------------------------------------------------------------------------

WFC_API_CALL void WFC_APIENTRY
    wfcFence(WFCDevice dev, WFCContext ctx, WFCEGLDisplay dpy,
        WFCEGLSync sync) WFC_APIEXIT
{
   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
      {return;}

   WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *) dev;
   WFC_CONTEXT_T *context_ptr = (WFC_CONTEXT_T *) ctx;

   WFC_LOCK();

   // TODO wfcFence()
   vcos_assert(0);

   if(vcos_verify((context_ptr != NULL) && (context_ptr->device_ptr == device_ptr)))
   {
      /* Set error as WFC_ERROR_ILLEGAL_ARGUMENT
         - if dpy is not a valid EGLDisplay
         - if sync is not a valid sync object
         - if sync’s EGL_SYNC_TYPE_KHR is not EGL_SYNC_REUSABLE_KHR
       */
   } // if
   else
      {wfc_set_error(device_ptr, WFC_ERROR_BAD_HANDLE);}

   WFC_UNLOCK();
} // wfcFence()

//==============================================================================
// Renderer and extension information

WFC_API_CALL WFCint WFC_APIENTRY
    wfcGetStrings(WFCDevice dev,
        WFCStringID name,
        const char **strings,
        WFCint stringsCount) WFC_APIEXIT
{
   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
      {return 0;}

   WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *) dev;
   const char *name_string = NULL;

   WFC_LOCK();

   switch(name)
   {
      case WFC_VENDOR:
         name_string = "Broadcom";
         break;
      case WFC_RENDERER:
         name_string = "VideoCore IV HW";
         break;
      case WFC_VERSION:
         name_string = "1.0";
         break;
      case WFC_EXTENSIONS:
         name_string = "";
         break;
      default:
         wfc_set_error(device_ptr, WFC_ERROR_ILLEGAL_ARGUMENT);
         return 0;
   } // switch

   WFCint num_of_strings = 0;

   if(stringsCount >= 0)
   {
      num_of_strings = 1;
      if((strings != NULL) && (stringsCount > 0))
      {
         *strings = name_string;
      } // if
   } // if
   else
   {
      wfc_set_error(device_ptr, WFC_ERROR_ILLEGAL_ARGUMENT);
   } // ekse

   WFC_UNLOCK();

   return num_of_strings;
} // wfcGetStrings()

//------------------------------------------------------------------------------

WFC_API_CALL WFCboolean WFC_APIENTRY
    wfcIsExtensionSupported(WFCDevice dev, const char *string) WFC_APIEXIT
{
   // TODO wfcIsExtensionSupported()
   vcos_assert(0);

   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
      {return WFC_FALSE;}

   return WFC_FALSE;
}

//!@} // (OpenWF-C API functions)
//==============================================================================

//!@name
//! Macros used in wfc_context_create() to extract data returned from wfc_server_create_context().
//!@{
#define WFC_CONTEXT_WIDTH(data)  ((data) >> 16)
#define WFC_CONTEXT_HEIGHT_OR_ERR(data) ((data) & 0xFFFF)
//!@}

static WFC_CONTEXT_T *wfc_context_create
   (WFC_DEVICE_T *device_ptr, WFCContextType context_type,
      uint32_t screen_or_stream_num, WFCErrorCode *error)
{
   WFC_CONTEXT_T *context_ptr =
      khrn_platform_malloc(sizeof(WFC_CONTEXT_T), "WFC_CONTEXT_T");
   const WFC_CONTEXT_DYNAMIC_ATTRIB_T ctx_dyn_attrib_default = WFC_CONTEXT_DYNAMIC_ATTRIB_DEFAULT;

   if(context_ptr != NULL)
   {
      CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
      uint64_t pid = vcos_process_id_current();
      uint32_t pid_lo = (uint32_t) pid;
      uint32_t pid_hi = (uint32_t) (pid >> 32);
      int sem_name[3] = {pid_lo, pid_hi, (uint32_t)context_ptr};
      VCOS_STATUS_T status;

      memset(context_ptr, 0, sizeof(WFC_CONTEXT_T));
      status = khronos_platform_semaphore_create(&context_ptr->wait_sem, sem_name, 0);

      if (status == VCOS_SUCCESS)
      {
         uint32_t response = wfc_server_create_context((WFCContext)context_ptr,
               context_type, screen_or_stream_num, pid_lo, pid_hi);

         uint32_t height_or_err = WFC_CONTEXT_HEIGHT_OR_ERR(response);
         uint32_t width = WFC_CONTEXT_WIDTH(response);

         if(width != 0)
         {
            wfc_link_init_null(&context_ptr->link);

            context_ptr->device_ptr = device_ptr;
            wfc_link_init_empty(&context_ptr->sources);
            wfc_link_init_empty(&context_ptr->masks);

            wfc_link_init_empty(&context_ptr->elements_not_in_scene);
            wfc_link_init_empty(&context_ptr->elements_in_scene);
            context_ptr->active = false;

            context_ptr->dynamic_attributes = ctx_dyn_attrib_default;
            context_ptr->static_attributes.type = context_type;
            context_ptr->static_attributes.height = height_or_err;
            context_ptr->static_attributes.width = width;

            if(context_type == WFC_CONTEXT_TYPE_OFF_SCREEN)
            {
               context_ptr->output_stream = screen_or_stream_num;
            }
         }
         else
         {
            khronos_platform_semaphore_destroy(&context_ptr->wait_sem);
            khrn_platform_free(context_ptr);
            context_ptr = NULL;
            *error = (WFCErrorCode) response;
         }
      } // if
      else
      {
         khrn_platform_free(context_ptr);

         context_ptr = NULL;

         *error = WFC_ERROR_OUT_OF_MEMORY;
      } // else
   } // if
   else
      {*error = WFC_ERROR_OUT_OF_MEMORY;}

   return context_ptr;
} // wfc_context_create()

//------------------------------------------------------------------------------

static void wfc_context_destroy(WFC_CONTEXT_T *context_ptr)
{
   // Destroy compositor wait semaphore
   khronos_platform_semaphore_destroy(&context_ptr->wait_sem);

   // Detach output stream for off-screen contexts.
   wfc_stream_register_off_screen(context_ptr->output_stream, false);

   // Remove from parent device's list of contexts.
   wfc_link_detach(&context_ptr->link);

   // Destroy all components
   wfc_link_iterate(&context_ptr->elements_in_scene, (WFC_LINK_CALLBACK_T) wfc_element_destroy, NULL);
   wfc_link_iterate(&context_ptr->elements_not_in_scene, (WFC_LINK_CALLBACK_T) wfc_element_destroy, NULL);
   wfc_link_iterate(&context_ptr->sources, (WFC_LINK_CALLBACK_T) wfc_source_or_mask_destroy_actual, NULL);
   wfc_link_iterate(&context_ptr->masks, (WFC_LINK_CALLBACK_T) wfc_source_or_mask_destroy_actual, NULL);

   wfc_server_destroy_context((WFCContext)context_ptr);

   khrn_platform_free(context_ptr);
} // wfc_context_destroy()

//==============================================================================

static WFCHandle wfc_source_or_mask_create(
      bool is_source, WFCDevice dev, WFCContext ctx,
      WFCNativeStreamType stream, const WFCint *attribList)
//!@brief Create a new image provider and associate it with a stream.
//!
//! wfcCreateSourceFromStream() and wfcCreateMaskFromStream() are essentially
//! wrappers for this function.
{
   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
      {return WFC_INVALID_HANDLE;}

   WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *) dev;
   WFC_CONTEXT_T *context_ptr = (WFC_CONTEXT_T *) ctx;

   WFCHandle handle = WFC_INVALID_HANDLE;

   if(vcos_verify((context_ptr != NULL) && (context_ptr->device_ptr == device_ptr)))
   {
      if(!wfc_check_no_attribs(attribList))
      {
         wfc_set_error(device_ptr, WFC_ERROR_BAD_ATTRIBUTE);
      }
      else if(context_ptr->output_stream == stream)
      {
         // Verify that context isn't an input for this stream
         wfc_set_error(device_ptr, WFC_ERROR_IN_USE);
      } // else if
      else
      {
         WFC_SOURCE_OR_MASK_T *source_or_mask_ptr = vcos_blockpool_calloc(&wfc_client_state.source_pool);

         if(source_or_mask_ptr != NULL)
         {
            // Note that refcount is initialised to zero here, as a source or mask is
            // only in use when it is linked to an element.

            wfc_link_init_null(&source_or_mask_ptr->link);

            source_or_mask_ptr->is_source = is_source;
            source_or_mask_ptr->context_ptr = context_ptr;
            source_or_mask_ptr->stream = stream;

            if(is_source)
            {
               wfc_link_attach(&source_or_mask_ptr->link, &context_ptr->sources);
            }
            else
            {
               wfc_link_attach(&source_or_mask_ptr->link, &context_ptr->masks);
            }
            handle = (WFCHandle) source_or_mask_ptr;

            wfc_stream_register_source_or_mask(stream, true);
         }
         else
         {
            wfc_set_error(device_ptr, WFC_ERROR_OUT_OF_MEMORY);
         }
      }
   }
   else
   {
      wfc_set_error(device_ptr, WFC_ERROR_BAD_HANDLE);
   }

   return handle;
}

//------------------------------------------------------------------------------

static void wfc_source_or_mask_destroy(WFCDevice dev, WFCHandle source_or_mask)
//!@brief Destroy an image provider and dissociate its stream.
//!
//! wfcDestroySource() and wfcDestroyMask() are essentially wrappers for this function.
{
   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
   {
      return;
   }

   WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *) dev;
   WFC_SOURCE_OR_MASK_T *source_or_mask_ptr = (WFC_SOURCE_OR_MASK_T *) source_or_mask;

   if((source_or_mask_ptr != NULL) && (source_or_mask_ptr->context_ptr != NULL)
      && (source_or_mask_ptr->context_ptr->device_ptr == device_ptr))
   {
      wfc_source_or_mask_destroy_actual(source_or_mask_ptr, NULL);
   }
   else
   {
      wfc_set_error(device_ptr, WFC_ERROR_BAD_HANDLE);
   }
}

//------------------------------------------------------------------------------

static void wfc_source_or_mask_acquire(WFC_SOURCE_OR_MASK_T *source_or_mask_ptr)
//! Indicate that the image provider is now linked to an element.
{
   vcos_log_trace("%s: %X refcount %d", VCOS_FUNCTION, (uint32_t)source_or_mask_ptr,
         source_or_mask_ptr ? source_or_mask_ptr->refcount : 0);
   if(source_or_mask_ptr != NULL)
      {source_or_mask_ptr->refcount++;}
} // wfc_source_or_mask_acquire()

//------------------------------------------------------------------------------

static void wfc_source_or_mask_release(WFC_SOURCE_OR_MASK_T *source_or_mask_ptr)
//! Indicate the the image provider is no longer linked to an element; destroy if previously requested.
{
   vcos_log_trace("%s: %X refcount %d", VCOS_FUNCTION, (uint32_t)source_or_mask_ptr,
         source_or_mask_ptr ? source_or_mask_ptr->refcount : 0);
   if(source_or_mask_ptr != NULL)
   {
      if(source_or_mask_ptr->refcount > 0)
      {
         source_or_mask_ptr->refcount--;
      }

      // If no-one is using this source or mask, and a request has previously
      // been made to destroy it, do so now.
      if((source_or_mask_ptr->refcount == 0) && source_or_mask_ptr->destroy_pending)
      {
         wfc_source_or_mask_destroy_actual(source_or_mask_ptr, NULL);
      }
   }
}

//------------------------------------------------------------------------------

static void wfc_source_or_mask_destroy_actual(WFC_SOURCE_OR_MASK_T *source_or_mask_ptr, void *unused)
{
   source_or_mask_ptr->destroy_pending = true;

   if(source_or_mask_ptr->refcount == 0)
   {
      vcos_log_info("wfc_source_or_mask_destroy_actual: %X",
         (uint32_t) source_or_mask_ptr);

      wfc_stream_register_source_or_mask(source_or_mask_ptr->stream, false);

      // Remove from parent context's list of sources or masks.
      wfc_link_detach(&source_or_mask_ptr->link);

      // Destroy.
      vcos_blockpool_free(source_or_mask_ptr);
   }
   else
   {
      vcos_log_info("wfc_source_or_mask_destroy_actual: pending: %X refcount: %d",
         (uint32_t) source_or_mask_ptr, source_or_mask_ptr->refcount);
   }
}

//==============================================================================

static void wfc_element_destroy(WFC_ELEMENT_T *element_ptr, void *unused)
{
   vcos_log_info("wfc_element_destroy: %X", (uint32_t) element_ptr);

   // Release source and mask (if present); destroy if previously requested.
   wfc_source_or_mask_release(element_ptr->source_ptr);
   wfc_source_or_mask_release(element_ptr->mask_ptr);

   element_ptr->source_ptr = NULL;
   element_ptr->mask_ptr = NULL;

   wfc_link_detach(&element_ptr->link);

   khrn_platform_free(element_ptr);
} // wfc_element_destroy()

//==============================================================================

static void wfc_commit_iterator(WFC_ELEMENT_T *element_ptr, WFC_SCENE_T *scene)
{
   // Elements with source or destination rectangles having zero width or height
   // must not displayed
   if
   (
      (element_ptr->attributes.dest_rect[WFC_RECT_WIDTH] == 0)
      || (element_ptr->attributes.dest_rect[WFC_RECT_HEIGHT] == 0)
      || (element_ptr->attributes.src_rect[WFC_RECT_WIDTH] < 0.00001)
      || (element_ptr->attributes.src_rect[WFC_RECT_HEIGHT] < 0.00001)
   )
   {return;}

   // Elements with a (near-)zero global alpha are transparent, so ignore them
   if
   (
      (element_ptr->attributes.transparency_types & WFC_TRANSPARENCY_ELEMENT_GLOBAL_ALPHA)
      && (element_ptr->attributes.global_alpha < 0.001)
   )
   {return;}

   // Copy element attributes into scene and increment count
   memcpy(&scene->elements[scene->element_count++], &element_ptr->attributes, sizeof(WFC_ELEMENT_ATTRIB_T));
} // wfc_commit_iterator()

//==============================================================================

static void wfc_set_error(WFC_DEVICE_T *device, WFCErrorCode error)
//! Update device's error code (but only if previously cleared).
{
   if((device != NULL) && (device->error == WFC_ERROR_NONE))
      {device->error = error;}
} // wfc_set_error()

//==============================================================================

static bool wfc_check_no_attribs(const WFCint *attribList)
//! Returns true if the attribute list is empty.
{
   return !attribList || (*attribList == WFC_NONE);
}

//------------------------------------------------------------------------------

static bool wfc_is_rotation(WFCint value)
{
   return value == WFC_ROTATION_0   ||
          value == WFC_ROTATION_90  ||
          value == WFC_ROTATION_180 ||
          value == WFC_ROTATION_270;
}

//------------------------------------------------------------------------------

static bool wfc_is_scale_filter(WFCint value)
{
   return value == WFC_SCALE_FILTER_NONE   ||
          value == WFC_SCALE_FILTER_FASTER ||
          value == WFC_SCALE_FILTER_BETTER;
}

//------------------------------------------------------------------------------

static bool wfc_are_transparency_types(WFCint value)
{
   return value == WFC_TRANSPARENCY_NONE ||
          value == WFC_TRANSPARENCY_ELEMENT_GLOBAL_ALPHA ||
          value == WFC_TRANSPARENCY_SOURCE ||
          value == WFC_TRANSPARENCY_SOURCE_VC_NON_PRE_MULT ||
          value == WFC_TRANSPARENCY_MASK ||
          value == (WFC_TRANSPARENCY_ELEMENT_GLOBAL_ALPHA | WFC_TRANSPARENCY_SOURCE) ||
          value == (WFC_TRANSPARENCY_ELEMENT_GLOBAL_ALPHA | WFC_TRANSPARENCY_SOURCE_VC_NON_PRE_MULT) ||
          value == (WFC_TRANSPARENCY_ELEMENT_GLOBAL_ALPHA | WFC_TRANSPARENCY_MASK);
}

//------------------------------------------------------------------------------

static int32_t wfc_round(float f)
{
   int result = (int)f;
   if (f>=0)
      if ((f-result)>=0.5) return ++result; else return result;
   else
      if ((f-result)<=-0.5) return --result; else return result;
}

//==============================================================================

static void wfc_link_detach(WFC_LINK_T *link)
{
   vcos_assert(link != NULL);
   if (link->next) {
      /*
         never unlink a base link
      */

      vcos_assert(link->next != link);
      vcos_assert(link->prev != link);

      link->next->prev = link->prev;
      link->prev->next = link->next;

      link->prev = NULL;
      link->next = NULL;
   }
}

//------------------------------------------------------------------------------

static void wfc_link_attach(WFC_LINK_T *link, WFC_LINK_T *prev)
{
   wfc_link_detach(link);

   link->prev = prev;
   link->next = prev->next;

   link->prev->next = link;
   link->next->prev = link;
}

//------------------------------------------------------------------------------

static void wfc_link_init_null(WFC_LINK_T *link)
//!@brief Initialise a link when the link will be used to keep track of structures
//! of the kind which contain the link.
{
   link->prev = NULL;
   link->next = NULL;
}

//------------------------------------------------------------------------------

static void wfc_link_init_empty(WFC_LINK_T *link)
//!@brief Initialise a link which will contain structures which are children of the
//! structure containing the link.
{
   link->prev = link;
   link->next = link;
}

//------------------------------------------------------------------------------

static void wfc_link_iterate(WFC_LINK_T *link, WFC_LINK_CALLBACK_T func, void *arg)
{
   WFC_LINK_T *curr = link;
   WFC_LINK_T *next = curr->next;

   while (next != link) {
      curr = next;
      next = curr->next;

      func(curr, arg);
   }
}

void wfc_set_deferral_stream(WFCDevice dev, WFCContext ctx, WFCNativeStreamType stream)
{
   if(!vcos_verify(dev != WFC_INVALID_HANDLE))
      {return;}

   WFC_DEVICE_T *device_ptr = (WFC_DEVICE_T *)dev;
   WFC_CONTEXT_T *context_ptr = (WFC_CONTEXT_T *)ctx;

   WFC_LOCK();

   if(vcos_verify((context_ptr != NULL) && (context_ptr->device_ptr == device_ptr)))
   {
      wfc_server_set_deferral_stream(ctx, stream);
   } //if
   else
      {wfc_set_error(device_ptr, WFC_ERROR_BAD_HANDLE);}

   WFC_UNLOCK();
}

//==============================================================================
