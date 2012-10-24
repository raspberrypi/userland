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


#ifndef _VC_TOUCH_API_TYPES_H
#define _VC_TOUCH_API_TYPES_H


/*===========================================================================*/
/*  Touch Service Errors (informational)                                     */
/*===========================================================================*/


#define TOUCH_RC_NONE          (0)
#define TOUCH_RC_BAD_HANDLE    (-1)
#define TOUCH_RC_UNIMPLEMENTED (-2)
#define TOUCH_RC_OPEN_INVAL    (-3)  /*< event open arguments invalid */
#define TOUCH_RC_RXLEN         (-13) /*< received length was unexpected */
#define TOUCH_RC_NOTRSP        (-14) /*< received msg not a response or event */
#define TOUCH_RC_NOTMINE       (-15) /*< received msg not my response */
#define TOUCH_RC_BADRC         (-16) /*< received with bad RC */


/*===========================================================================*/
/*  Touch Service Lifetime Control                                           */
/*===========================================================================*/


/*! Opaque type to hold per-client details of interface */
typedef uint32_t TOUCH_SERVICE_T[80];



/*===========================================================================*/
/*  Touchscreen Fingers                                                      */
/*===========================================================================*/

typedef struct vc_touch_point_s {
   int x;
   int y;
   int pressure;
   /* 'width' may join this list one day */
} vc_touch_point_t;


typedef struct vc_touch_properties_s {
   uint32_t screen_width;  /*< touch screen width in reported coordinates */
   uint32_t screen_height; /*< touch screen height in reported coordinates */
   uint8_t rightward_x;    /*< X increases in rightward direction (else left) */
   uint8_t rising_y;       /*< Y increases in updward direction (else down) */
   uint8_t finger_count;   /*< max number of fingers recorded simultaneously */
   unsigned max_pressure;  /*< max pressure that can be recorded (1 for a 
                               pressure-insensitve device)  */

   int32_t scaled_width;   /*< touch screen width in scaled coordinates */
   int32_t scaled_height;  /*< touch screen height in scaled coordinates */

   /* gesture box - movement further than these limits triggers a gesture */
   uint32_t gesture_box_width;
   uint32_t gesture_box_height;

} vc_touch_properties_t;


typedef enum {
   vc_touch_event_down,
   vc_touch_event_up,
   vc_touch_event_press,
   vc_touch_event_hold,
   vc_touch_event_press_tap_tap,
   vc_touch_event_gesture_right,
   vc_touch_event_gesture_left,
   vc_touch_event_gesture_up,
   vc_touch_event_gesture_down,
} vc_touch_event_t;


/*! Prototype for callback function for \c vc_touch_finger_events_open
 *  The positions of \c max_fingers fingers will reported (where
 *  the value of \c max_fingers is set in vc_touch_finger_events_open)
 */
typedef void
vc_touch_event_fn_t(void *open_arg, 
                    vc_touch_event_t action, uint32_t action_arg);

 
/*===========================================================================*/
/*  Buttons                                                                  */
/*===========================================================================*/


/*! This is an ennumeration of the different kinds of button supported
 */
typedef enum {
   vc_touch_button_default
   /* add new types here */
} vc_touch_button_type_t;
 
typedef uint8_t vc_button_id_t;    /*< an integer 1 .. 31 */
typedef uint32_t vc_button_set_t;  /*< bitmap of button IDs */

typedef struct vc_button_state_s {
   vc_button_set_t active;    /*< buttons that have been defined */
   vc_button_set_t depressed; /*< buttons that are currently pressed */
   /* buttons that are active but not depressed are released */
} vc_button_state_t;

typedef enum {
   vc_button_event_press,
   vc_button_event_repeat,
   vc_button_event_release
} vc_button_event_t;

typedef uint32_t vc_button_handle_t;
#define VC_BUTTON_HANDLE_BAD 0

/*! Names for buttons likely to be on the development card */
typedef enum
{
   /* The push button keys */
   vc_key_a,
   vc_key_b,
   vc_key_c,
   vc_key_d,
   vc_key_e,

   /* Joystick function keys */
   vc_key_down,
   vc_key_left,
   vc_key_up,
   vc_key_right,
   vc_key_z,

   /* Other functions */
   vc_key_pageup,
   vc_key_pagedown

   /*! update this define if you add further key names */
#define VC_KEY_NAME_MAX vc_key_pagedown

} vc_key_name_t;

/*! Prototype for callback function for \c vc_buttons_events_open
 */
typedef void
vc_button_event_fn_t(void *open_arg, vc_button_event_t action,
                     vc_button_id_t button, uint32_t action_arg);


#endif /* _VC_TOUCH_API_TYPES_H */
