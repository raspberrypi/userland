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



#ifndef _VC_TOUCHSERV_DEFS_H
#define _VC_TOUCHSERV_DEFS_H

#define VC_TOUCHSERV_VER   1
  

/*! Message header */
typedef struct {
   uint16_t id;         /*< identify the type of this message */
   uint16_t rc;         /*< return code in responses */
} vc_touch_msg_hdr_t;


/*! bits for inclusion in the an message ID */
#define VC_TOUCH_REPLY   0x1000  /*< response in synchronous transacton */
#define VC_TOUCH_EVENT   0x2000  /*< asynchronous response */


/* PROPERTIES transaction */

#define VC_TOUCH_ID_PROP_REQ 1

typedef struct {
   vc_touch_msg_hdr_t hdr;
} vc_touch_msg_prop_req_t;

#define VC_TOUCH_ID_PROP_RSP (VC_TOUCH_ID_PROP_REQ | VC_TOUCH_REPLY)

typedef struct {
   vc_touch_msg_hdr_t hdr;
   uint32_t screen_width;  /*< touch screen width in reported coordinates */
   uint32_t screen_height; /*< touch screen height in reported coordinates */
   uint32_t max_pressure;  /*< max pressure that can be recorded (1 for a 
                                pressure-insensitve device)  */
   uint32_t gesture_box_width; /*< distance for gesture_right/left */
   uint32_t gesture_box_height; /*< distance for gesture up/down */
   int32_t scaled_width;   /*< touch screen width in scaled coordinates */
   int32_t scaled_height;  /*< touch screen height in scaled coordinates */
   uint8_t rightward_x;    /*< X increases in rightward direction (else left) */
   uint8_t rising_y;       /*< Y increases in updward direction (else down) */
   uint8_t finger_count;   /*< max number of fingers recorded simultaneously */
} vc_touch_msg_prop_rsp_t;

typedef union {
   vc_touch_msg_prop_req_t req;
   vc_touch_msg_prop_rsp_t rsp;
} VC_TOUCH_MSG_PROP_T;


/* FINGERPOS transaction */

#define VC_TOUCHSERV_FINGERS_MAX 10

#define VC_TOUCH_ID_FINGERPOS_REQ 2

typedef struct {
   vc_touch_msg_hdr_t hdr;
   uint32_t fingers;
} vc_touch_msg_fingerpos_req_t;

#define VC_TOUCH_ID_FINGERPOS_RSP (VC_TOUCH_ID_FINGERPOS_REQ | VC_TOUCH_REPLY)

typedef struct {
   uint32_t x;
   uint32_t y;
   uint32_t pressure;
} VC_TOUCH_POSN_T;
   
typedef struct {
   vc_touch_msg_hdr_t hdr;
   VC_TOUCH_POSN_T posn[VC_TOUCHSERV_FINGERS_MAX];
} vc_touch_msg_fingerpos_rsp_t;

typedef union {
   vc_touch_msg_fingerpos_req_t req;
   vc_touch_msg_fingerpos_rsp_t rsp;
} VC_TOUCH_MSG_FINGERPOS_T;

/* FINGEROPEN transaction */

#define VC_TOUCH_ID_FINGEROPEN_REQ 3

typedef struct {
   vc_touch_msg_hdr_t hdr;
   uint32_t period_ms;
   uint32_t max_fingers;
} vc_touch_msg_fingeropen_req_t;

#define VC_TOUCH_ID_FINGEROPEN_RSP (VC_TOUCH_ID_FINGEROPEN_REQ | VC_TOUCH_REPLY)

typedef struct {
   vc_touch_msg_hdr_t hdr;
} vc_touch_msg_fingeropen_rsp_t;


#define VC_TOUCH_ID_FINGER_EVENT (VC_TOUCH_ID_FINGEROPEN_REQ | VC_TOUCH_EVENT)

typedef struct {
   vc_touch_msg_hdr_t hdr;
   uint32_t type;       /*< type of finger event */
   uint32_t arg;        /*< type-dependent argument for button event */
} VC_TOUCH_MSG_FINGER_EVENT_T;

typedef union {
   vc_touch_msg_fingeropen_req_t req;
   vc_touch_msg_fingeropen_rsp_t rsp;
} VC_TOUCH_MSG_FINGEROPEN_T;


/* BUTTNEW_TOUCH transaction */

#define VC_TOUCH_ID_BUTTNEW_TOUCH_REQ 4

typedef struct {
   vc_touch_msg_hdr_t hdr;
   uint32_t button_id;
   uint32_t kind;
   int32_t bottom_y;
   int32_t left_x;
   int32_t width;
   int32_t height;
} vc_touch_msg_buttnew_touch_req_t;

#define VC_TOUCH_ID_BUTTNEW_TOUCH_RSP \
        (VC_TOUCH_ID_BUTTNEW_TOUCH_REQ | VC_TOUCH_REPLY)

typedef struct {
   vc_touch_msg_hdr_t hdr;
   uint32_t button_handle;
} vc_touch_msg_buttnew_touch_rsp_t;

typedef union {
   vc_touch_msg_buttnew_touch_req_t req;
   vc_touch_msg_buttnew_touch_rsp_t rsp;
} VC_TOUCH_MSG_BUTTNEW_TOUCH_T;


/* BUTTNEW_KEY transaction */

#define VC_TOUCH_ID_BUTTNEW_KEY_REQ 5

typedef struct {
   vc_touch_msg_hdr_t hdr;
   uint32_t button_id;
   uint32_t key_name;
} vc_touch_msg_buttnew_key_req_t;

#define VC_TOUCH_ID_BUTTNEW_KEY_RSP \
        (VC_TOUCH_ID_BUTTNEW_KEY_REQ | VC_TOUCH_REPLY)

typedef struct {
   vc_touch_msg_hdr_t hdr;
   uint32_t button_handle;
} vc_touch_msg_buttnew_key_rsp_t;

typedef union {
   vc_touch_msg_buttnew_key_req_t req;
   vc_touch_msg_buttnew_key_rsp_t rsp;
} VC_TOUCH_MSG_BUTTNEW_KEY_T;


/* KEYRATE transaction */

#define VC_TOUCH_ID_KEYRATE_REQ 6

typedef struct {
   vc_touch_msg_hdr_t hdr;
   uint32_t period_ms;
} vc_touch_msg_keyrate_req_t;

#define VC_TOUCH_ID_KEYRATE_RSP (VC_TOUCH_ID_KEYRATE_REQ | VC_TOUCH_REPLY)

typedef struct {
   vc_touch_msg_hdr_t hdr;
   uint32_t period_ms;
} vc_touch_msg_keyrate_rsp_t;


typedef union {
   vc_touch_msg_keyrate_req_t req;
   vc_touch_msg_keyrate_rsp_t rsp;
} VC_TOUCH_MSG_KEYRATE_T;


/* BUTTNEW_IR transaction */

#define VC_TOUCH_ID_BUTTNEW_IR_REQ 7

typedef struct {
   vc_touch_msg_hdr_t hdr;
   uint32_t button_id;
} vc_touch_msg_buttnew_ir_req_t;

#define VC_TOUCH_ID_BUTTNEW_IR_RSP \
        (VC_TOUCH_ID_BUTTNEW_IR_REQ | VC_TOUCH_REPLY)

typedef struct {
   vc_touch_msg_hdr_t hdr;
   uint32_t button_handle;
} vc_touch_msg_buttnew_ir_rsp_t;

typedef union {
   vc_touch_msg_buttnew_ir_req_t req;
   vc_touch_msg_buttnew_ir_rsp_t rsp;
} VC_TOUCH_MSG_BUTTNEW_IR_T;


/* BUTTDEL transaction */

#define VC_TOUCH_ID_BUTTDEL_REQ 8

typedef struct {
   vc_touch_msg_hdr_t hdr;
   uint32_t button_handle;
} vc_touch_msg_buttdel_req_t;

#define VC_TOUCH_ID_BUTTDEL_RSP (VC_TOUCH_ID_BUTTDEL_REQ | VC_TOUCH_REPLY)

typedef struct {
   vc_touch_msg_hdr_t hdr;
} vc_touch_msg_buttdel_rsp_t;

typedef union {
   vc_touch_msg_buttdel_req_t req;
   vc_touch_msg_buttdel_rsp_t rsp;
} VC_TOUCH_MSG_BUTTDEL_T;


/* BUTTSTATE transaction */

#define VC_TOUCH_ID_BUTTSTATE_REQ 9

typedef struct {
   vc_touch_msg_hdr_t hdr;
} vc_touch_msg_buttstate_req_t;

#define VC_TOUCH_ID_BUTTSTATE_RSP (VC_TOUCH_ID_BUTTSTATE_REQ | VC_TOUCH_REPLY)

typedef struct {
   vc_touch_msg_hdr_t hdr;
   uint32_t active_button_set;
   uint32_t depressed_button_set;
} vc_touch_msg_buttstate_rsp_t;

typedef union {
   vc_touch_msg_buttstate_req_t req;
   vc_touch_msg_buttstate_rsp_t rsp;
} VC_TOUCH_MSG_BUTTSTATE_T;


/* BUTTOPEN transaction */

#define VC_TOUCH_ID_BUTTOPEN_REQ 10

typedef struct {
   vc_touch_msg_hdr_t hdr;
   uint32_t period_ms;
   uint32_t arg;
} vc_touch_msg_buttopen_req_t;

#define VC_TOUCH_ID_BUTTOPEN_RSP (VC_TOUCH_ID_BUTTOPEN_REQ | VC_TOUCH_REPLY)

typedef struct {
   vc_touch_msg_hdr_t hdr;
} vc_touch_msg_buttopen_rsp_t;

#define VC_TOUCH_ID_BUTTON_EVENT (VC_TOUCH_ID_BUTTOPEN_REQ | VC_TOUCH_EVENT)

typedef struct {
   vc_touch_msg_hdr_t hdr;
   uint32_t type;       /*< type of button event */
   uint32_t button_id;  /*< button ID the event refers to */
   uint32_t arg;        /*< type-dependent argument for button event */
} VC_TOUCH_MSG_BUTTON_EVENT_T;

typedef union {
   vc_touch_msg_buttopen_req_t req;
   vc_touch_msg_buttopen_rsp_t rsp;
} VC_TOUCH_MSG_BUTTOPEN_T;


/* Generic service transaction */


typedef union {
   VC_TOUCH_MSG_PROP_T          prop;
   VC_TOUCH_MSG_FINGERPOS_T     fingerpos;
   VC_TOUCH_MSG_FINGEROPEN_T    fingeropen;
   VC_TOUCH_MSG_FINGER_EVENT_T  finger_ev;
   VC_TOUCH_MSG_BUTTNEW_KEY_T   buttnew_key;
   VC_TOUCH_MSG_KEYRATE_T       keyrate;
   VC_TOUCH_MSG_BUTTNEW_TOUCH_T buttnew_touch;
   VC_TOUCH_MSG_BUTTNEW_IR_T    buttnew_ir;
   VC_TOUCH_MSG_BUTTDEL_T       buttdel;
   VC_TOUCH_MSG_BUTTSTATE_T     buttstate;
   VC_TOUCH_MSG_BUTTOPEN_T      buttopen;
   VC_TOUCH_MSG_BUTTON_EVENT_T  button_ev;
} VC_TOUCH_MSG_T;

typedef union {
   VC_TOUCH_MSG_FINGER_EVENT_T finger_ev;
   VC_TOUCH_MSG_BUTTON_EVENT_T button_ev;
} VC_TOUCH_EVENT_MSG_T;

   
#endif /* _VC_TOUCHSERV_DEFS_H */

