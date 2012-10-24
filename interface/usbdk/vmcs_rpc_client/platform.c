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
#include "platform.h"
#include "message_dispatch.h"

#define PLATFORM_MAX_NUM_TIMERS 6

typedef struct {
  uint32_t     timer_id;
  uint32_t     timer_period;
} PLATFORM_TIMER_INFO_T;

VCOS_TIMER_T vcos_timer[PLATFORM_MAX_NUM_TIMERS];

#ifdef _MINGW_
#include "windows.h"
HWND console;
#define _WIN32_WINNT 0x0502

VOID CALLBACK
timer_callback (HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime)
{
  KillTimer (NULL, timers[current_id].timer_id);
  add_message (PLATFORM_MSG_TIMER_TICK, current_id, 0);
}
#endif

void vcos_timer_callback (void *context)
{
  add_message (PLATFORM_MSG_TIMER_TICK,
               (VCOS_TIMER_T *)context - vcos_timer,
               0);
}

int32_t
platform_timer_start (const uint32_t timer_id,
                      const uint32_t timer_period)
{
  int32_t success = -1;
  if (timer_id < PLATFORM_MAX_NUM_TIMERS)
    {
      //      UINT_PTR id = SetTimer (NULL, 0, timer_period, &timer_callback);
      //if (id == 0)
      //  printf ("error = %d", GetLastError());

      vcos_timer_create (&vcos_timer[timer_id], "timer", vcos_timer_callback,
                         &vcos_timer[timer_id]);

      vcos_timer_set (&vcos_timer[timer_id], timer_period);

      //      timers[timer_id].timer_period = timer_period;
      //      timers[timer_id].timer_id = id;
      //      current_id = timer_id;
      success = 0;
   }
   return success;
}

int32_t
platform_timer_reschedule (const uint32_t timer_id,
                           const uint32_t timer_period)
{
  //  platform_timer_start (timer_id, timer_period);
  vcos_timer_reset (&vcos_timer[timer_id], timer_period);  
  return 0;
}

int32_t
platform_post_message (const uint16_t msg, const uint32_t param1,
                       const uint32_t param2)
{
  /* simply add it to client's message queue */
  add_message (msg, param1, param2);
  return 0;
}


int32_t
platform_timer_stop (const uint32_t timer_id)
{
  int32_t success = 0;
  //  KillTimer (NULL, timer_id );
  vcos_timer_cancel (&vcos_timer[timer_id]);
  return success;
}

uint32_t
platform_get_vmcs_connected_status ()
{
  /* Check if we have a connection to VC?? */
  // return host_attached_to_vmcs;
  
  /* For now, assume always connected */
  return 1;
}

static uint32_t screen_width = 800;
static uint32_t screen_height = 480;
int32_t
platform_get_screen_size (uint32_t *width, uint32_t *height)
{
   int32_t success = -1;

   if( NULL != width )
   {
      *width = screen_width;

      //success!
      success = 0;
   }

   if( NULL != height )
   {
      *height = screen_height;

      //success!
      success = 0;
   }

   return success;
}

#ifdef WIN32

#include "interface/vmcs_host/vchostreq.h"
static void platform_hostreq_notify_callback( const VC_HRNOTIFY_T notify_event, const uint32_t param )
{
   //post a message to the queue notifying the app of the hostreq
   platform_post_message( PLATFORM_MSG_HOSTREQ_NOTIFY, (uint32_t)notify_event, (uint32_t)param );
}

int32_t platform_enable_hostreq_notify( const uint32_t notify_event, const uint32_t enable )
{
   int32_t success = -1;

   if ( (notify_event > VC_HRNOTIFY_START) && (notify_event < VC_HRNOTIFY_END) )
   {
      if ( enable )
      {
         //add in the hostreq notify callback
         success = vc_hostreq_set_notify( notify_event, platform_hostreq_notify_callback );
      }
      else
      {
         //remove the hostreq notify callback
         success = vc_hostreq_set_notify( notify_event, NULL );
      }
   }

   return success;
}


int32_t vmcs_framework_create_hdmi_buttons( void )
{
  return 0;  // FIXME
}

#endif
