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

#ifndef PLATFORM_H
#define PLATFORM_H

#include "interface/vmcs_host/vchost.h"
#include "interface/vcos/vcos.h"

/******************************************************************************
Global typedefs, macros and constants
******************************************************************************/

/* the definition of all the message types that a
   platform can send to the message handler */

typedef enum
{
   /* A message sent at the start of the day

      Param 1 = 0
      Param 2 = 0 */
   PLATFORM_MSG_INIT,

   /*A message sent at the end of the day

      Param 1 = 0
      Param 2 = 0 */
   PLATFORM_MSG_END,

   /*A message sent when a button action occurs

      Param 1 = the button ID
      Param 2 = the hold time - only valid for repeat button presses */
   PLATFORM_MSG_BUTTON_PRESS,
   PLATFORM_MSG_BUTTON_REPEAT,
   PLATFORM_MSG_BUTTON_RELEASE,

   /*A message sent when a timer event occurs

      Param 1 = the timer ID
      Param 2 = 0 */
   PLATFORM_MSG_TIMER_TICK,

   /*A message sent when a hostreq notify is sent from VMCS

      Param 1 = the event id
      Param 2 = event param */
   PLATFORM_MSG_HOSTREQ_NOTIFY,

   /*A touchscreen event

      Param 1 = the event id
      Param 2 = event param */
   PLATFORM_MSG_TOUCHSCREEN_EVENT,

   /* user messages go here */

   PLATFORM_MSG_USER

} PLATFORM_MSG_T;


/* the definitions of the buttons in the system
   Current these are mapped to the VC02DK */

typedef enum
{
   //The push buttons
   PLATFORM_BUTTON_A = 0x0001,
   PLATFORM_BUTTON_B = 0x0002,
   PLATFORM_BUTTON_C = 0x0004,
   PLATFORM_BUTTON_D = 0x0008,
   PLATFORM_BUTTON_E = 0x0010,

   //The joystick moves
   PLATFORM_BUTTON_DOWN = 0x0020,
   PLATFORM_BUTTON_LEFT = 0x0040,
   PLATFORM_BUTTON_UP = 0x0080,
   PLATFORM_BUTTON_RIGHT = 0x0100,
   PLATFORM_BUTTON_Z = 0x0200,

   PLATFORM_BUTTON_PAGE_UP = 0x0400,
   PLATFORM_BUTTON_PAGE_DOWN = 0x0800,

   //Some extra buttons for HDMI/TV
   PLATFORM_BUTTON_HDMI0 = 0x1000,
   PLATFORM_BUTTON_HDMI1 = 0x2000,
   PLATFORM_BUTTON_HDMI2 = 0x4000,
   PLATFORM_BUTTON_TV    = 0x8000,

   //Some extra buttons for CEC
   PLATFORM_BUTTON_CEC0  = 0x10000,
   PLATFORM_BUTTON_CEC1  = 0x20000

   //UPDATE THE NUMBER OF BUTTONS BELOW IF YOU EDIT THESE

} PLATFORM_BUTTON_T;

//the max number of buttons definition
#define PLATFORM_NUMBER_BUTTONS  18

/* the touchscreen events */

typedef enum
{
   PLATFORM_TOUCHSCREEN_EVENT_MIN,

   PLATFORM_TOUCHSCREEN_EVENT_GESTURE_RIGHT,
   PLATFORM_TOUCHSCREEN_EVENT_GESTURE_LEFT,
   PLATFORM_TOUCHSCREEN_EVENT_GESTURE_DOWN,
   PLATFORM_TOUCHSCREEN_EVENT_GESTURE_UP,
   PLATFORM_TOUCHSCREEN_EVENT_PRESS,
   PLATFORM_TOUCHSCREEN_EVENT_PRESS_DOUBLE_TAP,
   PLATFORM_TOUCHSCREEN_EVENT_PRESS_AND_HOLD,
   PLATFORM_TOUCHSCREEN_EVENT_PINCH_IN,
   PLATFORM_TOUCHSCREEN_EVENT_SPREAD_OUT,
   PLATFORM_TOUCHSCREEN_EVENT_FINGER_DOWN,
   PLATFORM_TOUCHSCREEN_EVENT_FINGER_UP,

   PLATFORM_TOUCHSCREEN_EVENT_MAX

} PLATFORM_TOUCHSCREEN_EVENT_T;


//define the max file path
#ifndef MAX_PATH
#define MAX_PATH  260
#endif

//The default buffer size of the general command response
#define PLATFORM_DEFAULT_RESPONSE_BUF_SIZE   (uint32_t)(128)

#define PLATFORM_DEFAULT_REQUEST_BUF_SIZE    (uint32_t)(128)

//the max number of timers which an app can use
#define PLATFORM_MAX_NUM_TIMERS 6

//the starting id for the timers
#define PLATFORM_TIMER_START_ID 0


/* Definition for a platfor message handler - returning FALSE means that the app wishes to quit */
typedef int32_t (*PLATFORM_MESSAGE_HANDLER_T)(  const uint16_t msg,
                                                const uint32_t param1,
                                                const uint32_t param2 );

/******************************************************************************
Global Message funcs definitions
******************************************************************************/

// routines used to post a message to the platform message handle
VCHPRE_ int32_t VCHPOST_ platform_post_message( const uint16_t msg,
                                                const uint32_t param1,
                                                const uint32_t param2);

/******************************************************************************
Global Task funcs definitions
******************************************************************************/

// Definition for a platform thread function
typedef int32_t (*PLATFORM_THREAD_FUNC_T)( void *param );

// Definition for a platform thread handle
typedef void *PLATFORM_THREAD_T;

// routine used to create a new thread. This thread will be passed the void *param when created
// NOTE: the 'thread_func' MUST call the platform_thread_exit function before leaving the context of the function
VCHPRE_ PLATFORM_THREAD_T VCHPOST_ platform_thread_create(  PLATFORM_THREAD_FUNC_T thread_func,
                                                            void *param );

//Routine to set a threads priority
VCHPRE_ int32_t VCHPOST_ platform_thread_set_priority(PLATFORM_THREAD_T thread,
                                                      const uint32_t priority );

//Routine to exit a thead (called from within the thread)
//NOTE! You must call this function before exiting a thread.
VCHPRE_ void VCHPOST_ platform_thread_exit( void );

//Routine used to wait until a thread has exited
VCHPRE_ int32_t VCHPOST_ platform_thread_join( PLATFORM_THREAD_T thread );

/******************************************************************************
Semaphore funcs definitions
******************************************************************************/

//definition of a semaphore
struct PLATFORM_SEMAPHORE;
typedef struct PLATFORM_SEMAPHORE *PLATFORM_SEMAPHORE_T;

//Routine to create a semaphore. Seeds the semaphore with an initial count
VCHPRE_ int32_t VCHPOST_ platform_semaphore_create( PLATFORM_SEMAPHORE_T *semaphore, const uint32_t inital_count );

//Routine to delete a semaphore
VCHPRE_ int32_t VCHPOST_ platform_semaphore_delete( PLATFORM_SEMAPHORE_T *semaphore );

//Routine used to wait for a semaphore to be free (will suspend the calling thread if not available)
VCHPRE_ int32_t VCHPOST_ platform_semaphore_wait( PLATFORM_SEMAPHORE_T *semaphore );

//Routine used to post (increment) a semaphore. used to release any tasks waiting on the semaphore
VCHPRE_ int32_t VCHPOST_ platform_semaphore_post( PLATFORM_SEMAPHORE_T *semaphore );

/******************************************************************************
Global Event funcs definitions
******************************************************************************/

// Definition of an event group
struct PLATFORM_EVENTGROUP;
typedef struct PLATFORM_EVENTGROUP *PLATFORM_EVENTGROUP_T;

//Event group flags
typedef enum
{
   PLATFORM_EVENTGROUP_OPERATION_MIN,

   PLATFORM_EVENTGROUP_OPERATION_AND,
   PLATFORM_EVENTGROUP_OPERATION_OR,

   PLATFORM_EVENTGROUP_OPERATION_AND_CONSUME,
   PLATFORM_EVENTGROUP_OPERATION_OR_CONSUME,

   PLATFORM_EVENTGROUP_OPERATION_MAX

} PLATFORM_EVENTGROUP_OPERATION_T;

#define PLATFORM_EVENTGROUP_SUSPEND -1
#define PLATFORM_EVENTGROUP_NO_SUSPEND 0

//Routines used to create an event group
VCHPRE_ int32_t VCHPOST_ platform_eventgroup_create( PLATFORM_EVENTGROUP_T *eventgroup );

//Routines used to delete an event group
VCHPRE_ int32_t VCHPOST_ platform_eventgroup_delete( PLATFORM_EVENTGROUP_T *eventgroup );

//Routines used to set a bit in an eventgroup
VCHPRE_ int32_t VCHPOST_ platform_eventgroup_set(  PLATFORM_EVENTGROUP_T *eventgroup,
                                                   const uint32_t bitmask,
                                                   const PLATFORM_EVENTGROUP_OPERATION_T operation );

//Routine used to wait for bits in an eventgroup to be set
VCHPRE_ int32_t VCHPOST_ platform_eventgroup_get(  PLATFORM_EVENTGROUP_T *eventgroup,
                                                   const uint32_t bitmask,
                                                   const PLATFORM_EVENTGROUP_OPERATION_T operation,
                                                   const uint32_t suspend,
                                                   uint32_t *retrieved_bits );

/******************************************************************************
Global Timer func definitions
******************************************************************************/

// Routines used to start / stop a single timer for the host app platform
VCHPRE_ int32_t VCHPOST_ platform_timer_start(  const uint32_t timer_id,
                                                const uint32_t timer_period );

VCHPRE_ int32_t VCHPOST_ platform_timer_stop( const uint32_t timer_id );

// Routine to change the timer resolution
VCHPRE_ int32_t VCHPOST_ platform_timer_reschedule(const uint32_t timer_id,
                                                   const uint32_t timer_period );

/******************************************************************************
Global C library style func definitions
******************************************************************************/

// routine to set the button repeat rate
VCHPRE_ void VCHPOST_ platform_set_button_repeat_time( const uint32_t repeat_rate_in_ms );

// routine to enable / disable a host req message
VCHPRE_ int32_t VCHPOST_ platform_enable_hostreq_notify( const uint32_t notify_event, const uint32_t enable );

// routine to sleep the platform
VCHPRE_ void VCHPOST_ platform_sleep( const uint32_t sleep_time_in_ms );

// routine to get the local resource path
VCHPRE_ char * VCHPOST_ platform_get_local_resource_path( void );

// routine perform a str comparison with case sensitivity
VCHPRE_ int32_t VCHPOST_ platform_stricmp( const char *str1, const char *str2 );

// routine to return the number of ticks since the start of day, in ms
VCHPRE_ int32_t VCHPOST_ platform_get_ticks( void );

// routine to send out debug print messages to the platform host
VCHPRE_ void VCHPOST_ platform_debug_print( const char *string, ... );

//Routine to malloc a section of memory - has addition line and file parameters for debug
VCHPRE_ void * VCHPOST_ platform_malloc(  const uint32_t size_in_bytes,
                                          const int line,
                                          const char *file );

//Routine to malloc and clear a section of memory - has addition line and file parameters for debug
VCHPRE_ void * VCHPOST_ platform_calloc(  const uint32_t num,
                                          const uint32_t size_in_bytes,
                                          const int line,
                                          const char *file );

// Routine to free memory previously malloc'd
VCHPRE_ void VCHPOST_ platform_free( void *buffer );

//Routine to allocate memory on specific alignment
VCHPRE_ void *VCHPOST_ platform_malloc_aligned( const uint32_t size_in_bytes,
                                                unsigned int align,
                                                const char *description);

//Routine to query if we are connected to VMCS or not
VCHPRE_ uint32_t VCHPOST_ platform_get_vmcs_connected_status( void );

//Routine to create + return the hosts native debug window handle
VCHPRE_ uint32_t VCHPOST_ platform_create_and_get_debug_window_handle( void );

//Routine to get the platforms screen size
VCHPRE_ int32_t VCHPOST_ platform_get_screen_size( uint32_t *width,
                                                   uint32_t *height );

//Set the height of the buttons created along the bottom of the touchscreen. The
//format is Q8 fixed-point fraction of the screen - e.g. 0x80 will give you buttons
//half the height of the screen. If zero, no buttons are created.
VCHPRE_ int32_t VCHPOST_ platform_set_touchscreen_buttons_height(uint32_t xHeight);

//Translate gestures (left/right/up/down) into the joystick equivalents.
VCHPRE_ int32_t VCHPOST_ platform_translate_gestures_to_joystick(int);

//Set the size of the screen in an alternative coordinate system.
//    scaled_width - scaled X width of the touch screen area
//    scaled_height - scaled Y height of the touch screen area
VCHPRE_  int32_t VCHPOST_ platform_set_touchscreen_scale(int scaled_width, int scaled_height);
   
//Return the scaled location of the fingers currently in contact with the  
//touchscreen.
VCHPRE_ int32_t /*rc*/ VCHPOST_ platform_touch_finger_scaled_pos(
                                   int *out_x, int *out_y,
                                   int *out_pressure, int *out_width);

#endif /* PLATFORM_H */

/**************************** End of file ************************************/
