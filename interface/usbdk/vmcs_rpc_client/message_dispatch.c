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

#include <termios.h>

#include "vmcs_config.h"

#if !defined(VMCS_USE_ONCRPC)
typedef struct {
  long msg;
  long param1;
  long param2;
} PLT_msg;
#else
 #include <rpc/rpc.h>
 #include "oncrpc_vmcs.h"
#endif

#include <stdlib.h>
#include "platform.h"
#include "message_dispatch.h"
#include "host_applications/framework/host_app.h"

#ifndef countof
#define countof(x) (sizeof(x)/sizeof(*x))
#endif


#include "interface/vchi/vchi.h"
#include "interface/vchi/message_drivers/message.h"

#include "interface/peer/vc_touchsrv_defs.h"
#include "interface/peer/vc_touch_api_types.h"
#include "interface/vcos/vcos.h"


#define DO(_X) _X
#define IGNORE(_X)


#define DEBUG_TBUT   IGNORE
#define DEBUG_TEVENT IGNORE



#if 1
#define DPRINTF printf
#define DPRINTF_INIT()
#else

/* Don't use logging if it's not being used by some debug output */
#if 0 == DEBUG_TBUT(1+)DEBUG_TEVENT(1+)0
/* shouldn't use DPRINTF outside DEBUG statements */
#define DPRINTF()
#define DPRINTF_INIT()
#else
#define DPRINTF(arglist...) logging_message(LOGGING_GENERAL, arglist)
#define DPRINTF_INIT() \
   {  logging_init();  \
      logging_level(LOGGING_GENERAL); \
   }
#endif

#endif

#define CODEID "touchserv"

/* redefine as nothing to ensure debugger has access to function names */
#define TS_INLINE inline




/*****************************************************************************
 *                                                                           *
 *    Configuration                                                          *
 *                                                                           * 
 *****************************************************************************/



#define CREATE_DEFAULT_BUTTONS 1   /*< put 5 buttons along the bottom */

#define TOUCH_REPEAT_MS        1000 /*< also press-and-hold time */
#define TOUCH_DOUBLE_TAP_MS    800

#define MAX_BUTTON_COUNT       20  /*< maximum number of soft touch buttons */
#define MAX_FINGER_COUNT       10  /*< number of fingers we can cope with */
#define MAX_PRODEE_COUNT       32  /*< number of event dests. we can handle */

#define BUTTON_POLL_US         (1000000 / 150) /*< 1/150s in microseconds */


/*****************************************************************************
 *                                                                           *
 *    Service RPC Return Codes                                               *
 *                                                                           * 
 *****************************************************************************/




#define VC_TOUCH_RC_OK        (0) /*< Successfully executed */
#define VC_TOUCH_RC_MSG_SMALL (1) /*< Message too small for header! */
#define VC_TOUCH_RC_BADCMD    (2) /*< Message identifies unknown command */
#define VC_TOUCH_RC_MSG_SHORT (3) /*< Message too small for its type */
#define VC_TOUCH_RC_NOTIMPL   (4) /*< Message type not supported */
#define VC_TOUCH_RC_EINVAL    (5) /*< Message type argument is invalid */
#define VC_TOUCH_RC_FAIL      (6) /*< Message type execution failed */
#define VC_TOUCH_RC_NODEV     (7) /*< No device available for message type
                                      execution */




/*****************************************************************************
 *                                                                           *
 *    Prod Lists                                                             *
 *                                                                           * 
 *****************************************************************************/



/* Prod Lists are lists of things that can be prodded by methods of their
 * own choosing.  When a list is prodded a set of callbacks are invoked - each
 * registered by its own prodee.
 */


typedef void uncast_function_t(void);

typedef struct prodee_s {
   struct prodee_s *next;
   uncast_function_t *prod_fn;
   void *prod_arg;
} prodee_t;



typedef prodee_t prodee_pool_t[MAX_PRODEE_COUNT];


typedef enum
{
   BUTTON_KEY_NUM1     = 1<<0,
   BUTTON_KEY_NUM2     = 1<<1,
   BUTTON_KEY_NUM3     = 1<<2,
   BUTTON_KEY_NUM4     = 1<<3,
   BUTTON_KEY_NUM5     = 1<<4,
   BUTTON_KEY_NUM6     = 1<<5,
   BUTTON_KEY_NUM7     = 1<<6,
   BUTTON_KEY_NUM8     = 1<<7,
   BUTTON_KEY_NUM9     = 1<<8,
   BUTTON_KEY_STAR     = 1<<9,
   BUTTON_KEY_NUM0     = 1<<10,
   BUTTON_KEY_HASH     = 1<<11,

   BUTTON_KEY_UP       = 1<<12,
   BUTTON_KEY_DOWN     = 1<<13,
   BUTTON_KEY_LEFT     = 1<<14,
   BUTTON_KEY_RIGHT    = 1<<15,

   BUTTON_KEY_ENTER    = 1<<16,
   BUTTON_KEY_SEND     = 1<<17,
   BUTTON_KEY_END      = 1<<18,
   BUTTON_KEY_MENU     = 1<<19,
   BUTTON_KEY_CANCEL   = 1<<20,

   BUTTON_KEY_CAMERA   = 1<<21,
   BUTTON_KEY_SHLEFT   = 1<<22,
   BUTTON_KEY_SHRIGHT  = 1<<23,
   BUTTON_KEY_SPARE1   = 1<<24,
   BUTTON_KEY_SPARE2   = 1<<25,
   BUTTON_KEY_SPARE3   = 1<<26,

   BUTTON_KEY_SCROLL   = 1<<27,
   BUTTON_KEY_SCROLL_C = 1<<28,
   BUTTON_KEY_SCROLL_A = 1<<29,
   BUTTON_KEY_ROTATED  = 1<<30,
   BUTTON_KEY_EXIT     = 1<<31,
   BUTTON_KEY_ANY      = 0xFFFFFFFF

} BUTTON_T;

#define BUTTONS_NUM_OF_KEYS      32


typedef struct {
   uint32_t current;
   uint32_t total;
} button_time_t;

typedef button_time_t buttons_held_time_t[BUTTONS_NUM_OF_KEYS];


typedef struct
{  prodee_pool_t pool;
   prodee_t *touch_event_prodees;
   prodee_t *button_event_prodees;
} touchserv_prodlists_t;

typedef struct {
   enum {TOUCHSCREEN_EVENT_TYPE_ABS_X, TOUCHSCREEN_EVENT_TYPE_ABS_Y, TOUCHSCREEN_EVENT_TYPE_FINGER} type;
   unsigned param;
} TOUCHSCREEN_EVENT_T;




   
/*****************************************************************************
 *                                                                           *
 *    Task Signal Events                                                     *
 *                                                                           * 
 *****************************************************************************/




/* Event group indeces used to indicate type of work to main server task */
enum {
   TOUCHSRV_EVENT_MSG=1,
   TOUCHSRV_EVENT_TOUCH,
   TOUCHSRV_EVENT_BUTTON
};


   
/*****************************************************************************
 *                                                                           *
 *    Touchscreen Server Events                                              *
 *                                                                           * 
 *****************************************************************************/





   
typedef struct touch_button_s
{
   uint8_t /*bool*/ in_use;             /*< TRUE iff this button is active */
   uint8_t depressed;                   /*< bitmap of fingers in contact */
   vc_button_id_t button_id;            /*< button ID to pretend to be */
   vc_touch_button_type_t kind;         /*< kind of button */
   int16_t left_x;                      /*< coordinates of bounding box */
   int16_t bottom_y;                    
   int16_t width;
   int16_t height;
} touch_button_t;





typedef struct {
   vc_touch_point_t start_coord;
   /*< the starting coords of the finger 0 down */
   uint32_t down;       /*< finger is down */
   uint32_t down_time;  /*< finger down time */
   uint32_t held_down;  /*< TRUE if the finger is held down */
   uint32_t last_pressed_release_time;
   /*< the last time release of the previous press -
      used for double-tap recognition */
   int press_belongs_to_button; /*< when down the press is for a button */
} finger_state_t;
 



/* struct to store the buttons in */
typedef struct
{
   /* pointer to the buttons driver that we read */
   touchserv_prodlists_t        *prodlists;  /*< server's event generators */
   /* the current (or last received) coords */
   vc_touch_point_t              cur_coord;

   finger_state_t finger[MAX_FINGER_COUNT];


   /* soft buttons */
   vc_touch_properties_t        prop;
   touch_button_t               button[MAX_BUTTON_COUNT];

} touchscreen_t;



/* struct to store the button state in */
typedef struct
{
   touchserv_prodlists_t    *prodlists;  /*< server's event generators */

   uint32_t eds_ren;
   uint32_t eds_fen;
   uint32_t eds_lev;
   uint32_t eds_current;
   uint32_t last_lev;
   uint32_t last_last_lev;

   uint32_t repeat_rate_ms; /*< the repeat rate for the buttons in msec */

   /* the last time one of the buttons was pressed */
   buttons_held_time_t button_held_time;

} buttons_t;




/*! We need our own type for TOUCH_SERVICE_T because it is given no structure
 *  (i.e. it is opaque) in the vc_vchi_touch.h header
 */
typedef struct {
   touchserv_prodlists_t prodlists;
   touchscreen_t screen;
   buttons_t buttons;
} LOCAL_TOUCH_SERVICE_T;




typedef struct {
   int32_t num_connections;
   VCHI_SERVICE_HANDLE_T open_handle;
   int initialised;
   VCOS_THREAD_T touchserv_task_id;
   VC_TOUCH_MSG_T msg;
   VCOS_EVENT_FLAGS_T eventgroup;
   LOCAL_TOUCH_SERVICE_T touch;
} TOUCHSERV_STATE_T;




static TOUCHSERV_STATE_T touchserv_state;


static int touchscreen_handle_events(touchscreen_t *touchscreen, TOUCHSCREEN_EVENT_T event);
static void buttons_handle_events(buttons_t *buttons, uint32_t keys_pressed, uint32_t keys_held, uint32_t keys_released);

static void touchserv_task_main(unsigned argc, void *argv);
static int /*rc*/ local_touch_finger_ask_events_open(TOUCH_SERVICE_T *touchserv,
                                   VCHI_SERVICE_HANDLE_T transport,
                                   int period_ms, int max_fingers);
static int /*rc*/ local_buttons_ask_events_open(TOUCH_SERVICE_T *touchserv,
                              VCHI_SERVICE_HANDLE_T transport, int period_ms);
static int buttons_poll_keypad(buttons_t *buttons,
                    uint32_t *keys_pressed, uint32_t *keys_held,
                    uint32_t *keys_released );


static PLT_msg message_q[256];
static int start_p = 0, end_p = 0;
static VCOS_MUTEX_T msg_latch;
static VCOS_SEMAPHORE_T msg_semaphore;
#ifndef WIN32
static VCOS_THREAD_T button_thread, touch_thread;
#endif

#ifndef WIN32
#define info_printf if (1) {} else printf

#include <linux/input.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

static int fd_buttons=-1, fd_touch=-1;
char *events[EV_MAX + 1] = {
	[0 ... EV_MAX] = NULL,
	[EV_SYN] = "Sync",			[EV_KEY] = "Key",
	[EV_REL] = "Relative",			[EV_ABS] = "Absolute",
	[EV_MSC] = "Misc",			[EV_LED] = "LED",
	[EV_SND] = "Sound",			[EV_REP] = "Repeat",
	[EV_FF] = "ForceFeedback",		[EV_PWR] = "Power",
	[EV_FF_STATUS] = "ForceFeedbackStatus",
};

char *keys[KEY_MAX + 1] = {
	[0 ... KEY_MAX] = NULL,
	[KEY_RESERVED] = "Reserved",		[KEY_ESC] = "Esc",
	[KEY_1] = "1",				[KEY_2] = "2",
	[KEY_3] = "3",				[KEY_4] = "4",
	[KEY_5] = "5",				[KEY_6] = "6",
	[KEY_7] = "7",				[KEY_8] = "8",
	[KEY_9] = "9",				[KEY_0] = "0",
	[KEY_MINUS] = "Minus",			[KEY_EQUAL] = "Equal",
	[KEY_BACKSPACE] = "Backspace",		[KEY_TAB] = "Tab",
	[KEY_Q] = "Q",				[KEY_W] = "W",
	[KEY_E] = "E",				[KEY_R] = "R",
	[KEY_T] = "T",				[KEY_Y] = "Y",
	[KEY_U] = "U",				[KEY_I] = "I",
	[KEY_O] = "O",				[KEY_P] = "P",
	[KEY_LEFTBRACE] = "LeftBrace",		[KEY_RIGHTBRACE] = "RightBrace",
	[KEY_ENTER] = "Enter",			[KEY_LEFTCTRL] = "LeftControl",
	[KEY_A] = "A",				[KEY_S] = "S",
	[KEY_D] = "D",				[KEY_F] = "F",
	[KEY_G] = "G",				[KEY_H] = "H",
	[KEY_J] = "J",				[KEY_K] = "K",
	[KEY_L] = "L",				[KEY_SEMICOLON] = "Semicolon",
	[KEY_APOSTROPHE] = "Apostrophe",	[KEY_GRAVE] = "Grave",
	[KEY_LEFTSHIFT] = "LeftShift",		[KEY_BACKSLASH] = "BackSlash",
	[KEY_Z] = "Z",				[KEY_X] = "X",
	[KEY_C] = "C",				[KEY_V] = "V",
	[KEY_B] = "B",				[KEY_N] = "N",
	[KEY_M] = "M",				[KEY_COMMA] = "Comma",
	[KEY_DOT] = "Dot",			[KEY_SLASH] = "Slash",
	[KEY_RIGHTSHIFT] = "RightShift",	[KEY_KPASTERISK] = "KPAsterisk",
	[KEY_LEFTALT] = "LeftAlt",		[KEY_SPACE] = "Space",
	[KEY_CAPSLOCK] = "CapsLock",		[KEY_F1] = "F1",
	[KEY_F2] = "F2",			[KEY_F3] = "F3",
	[KEY_F4] = "F4",			[KEY_F5] = "F5",
	[KEY_F6] = "F6",			[KEY_F7] = "F7",
	[KEY_F8] = "F8",			[KEY_F9] = "F9",
	[KEY_F10] = "F10",			[KEY_NUMLOCK] = "NumLock",
	[KEY_SCROLLLOCK] = "ScrollLock",	[KEY_KP7] = "KP7",
	[KEY_KP8] = "KP8",			[KEY_KP9] = "KP9",
	[KEY_KPMINUS] = "KPMinus",		[KEY_KP4] = "KP4",
	[KEY_KP5] = "KP5",			[KEY_KP6] = "KP6",
	[KEY_KPPLUS] = "KPPlus",		[KEY_KP1] = "KP1",
	[KEY_KP2] = "KP2",			[KEY_KP3] = "KP3",
	[KEY_KP0] = "KP0",			[KEY_KPDOT] = "KPDot",
	[KEY_ZENKAKUHANKAKU] = "Zenkaku/Hankaku", [KEY_102ND] = "102nd",
	[KEY_F11] = "F11",			[KEY_F12] = "F12",
	[KEY_RO] = "RO",			[KEY_KATAKANA] = "Katakana",
	[KEY_HIRAGANA] = "HIRAGANA",		[KEY_HENKAN] = "Henkan",
	[KEY_KATAKANAHIRAGANA] = "Katakana/Hiragana", [KEY_MUHENKAN] = "Muhenkan",
	[KEY_KPJPCOMMA] = "KPJpComma",		[KEY_KPENTER] = "KPEnter",
	[KEY_RIGHTCTRL] = "RightCtrl",		[KEY_KPSLASH] = "KPSlash",
	[KEY_SYSRQ] = "SysRq",			[KEY_RIGHTALT] = "RightAlt",
	[KEY_LINEFEED] = "LineFeed",		[KEY_HOME] = "Home",
	[KEY_UP] = "Up",			[KEY_PAGEUP] = "PageUp",
	[KEY_LEFT] = "Left",			[KEY_RIGHT] = "Right",
	[KEY_END] = "End",			[KEY_DOWN] = "Down",
	[KEY_PAGEDOWN] = "PageDown",		[KEY_INSERT] = "Insert",
	[KEY_DELETE] = "Delete",		[KEY_MACRO] = "Macro",
	[KEY_MUTE] = "Mute",			[KEY_VOLUMEDOWN] = "VolumeDown",
	[KEY_VOLUMEUP] = "VolumeUp",		[KEY_POWER] = "Power",
	[KEY_KPEQUAL] = "KPEqual",		[KEY_KPPLUSMINUS] = "KPPlusMinus",
	[KEY_PAUSE] = "Pause",			[KEY_KPCOMMA] = "KPComma",
	[KEY_HANGUEL] = "Hanguel",		[KEY_HANJA] = "Hanja",
	[KEY_YEN] = "Yen",			[KEY_LEFTMETA] = "LeftMeta",
	[KEY_RIGHTMETA] = "RightMeta",		[KEY_COMPOSE] = "Compose",
	[KEY_STOP] = "Stop",			[KEY_AGAIN] = "Again",
	[KEY_PROPS] = "Props",			[KEY_UNDO] = "Undo",
	[KEY_FRONT] = "Front",			[KEY_COPY] = "Copy",
	[KEY_OPEN] = "Open",			[KEY_PASTE] = "Paste",
	[KEY_FIND] = "Find",			[KEY_CUT] = "Cut",
	[KEY_HELP] = "Help",			[KEY_MENU] = "Menu",
	[KEY_CALC] = "Calc",			[KEY_SETUP] = "Setup",
	[KEY_SLEEP] = "Sleep",			[KEY_WAKEUP] = "WakeUp",
	[KEY_FILE] = "File",			[KEY_SENDFILE] = "SendFile",
	[KEY_DELETEFILE] = "DeleteFile",	[KEY_XFER] = "X-fer",
	[KEY_PROG1] = "Prog1",			[KEY_PROG2] = "Prog2",
	[KEY_WWW] = "WWW",			[KEY_MSDOS] = "MSDOS",
	[KEY_COFFEE] = "Coffee",		[KEY_DIRECTION] = "Direction",
	[KEY_CYCLEWINDOWS] = "CycleWindows",	[KEY_MAIL] = "Mail",
	[KEY_BOOKMARKS] = "Bookmarks",		[KEY_COMPUTER] = "Computer",
	[KEY_BACK] = "Back",			[KEY_FORWARD] = "Forward",
	[KEY_CLOSECD] = "CloseCD",		[KEY_EJECTCD] = "EjectCD",
	[KEY_EJECTCLOSECD] = "EjectCloseCD",	[KEY_NEXTSONG] = "NextSong",
	[KEY_PLAYPAUSE] = "PlayPause",		[KEY_PREVIOUSSONG] = "PreviousSong",
	[KEY_STOPCD] = "StopCD",		[KEY_RECORD] = "Record",
	[KEY_REWIND] = "Rewind",		[KEY_PHONE] = "Phone",
	[KEY_ISO] = "ISOKey",			[KEY_CONFIG] = "Config",
	[KEY_HOMEPAGE] = "HomePage",		[KEY_REFRESH] = "Refresh",
	[KEY_EXIT] = "Exit",			[KEY_MOVE] = "Move",
	[KEY_EDIT] = "Edit",			[KEY_SCROLLUP] = "ScrollUp",
	[KEY_SCROLLDOWN] = "ScrollDown",	[KEY_KPLEFTPAREN] = "KPLeftParenthesis",
	[KEY_KPRIGHTPAREN] = "KPRightParenthesis", [KEY_F13] = "F13",
	[KEY_F14] = "F14",			[KEY_F15] = "F15",
	[KEY_F16] = "F16",			[KEY_F17] = "F17",
	[KEY_F18] = "F18",			[KEY_F19] = "F19",
	[KEY_F20] = "F20",			[KEY_F21] = "F21",
	[KEY_F22] = "F22",			[KEY_F23] = "F23",
	[KEY_F24] = "F24",			[KEY_PLAYCD] = "PlayCD",
	[KEY_PAUSECD] = "PauseCD",		[KEY_PROG3] = "Prog3",
	[KEY_PROG4] = "Prog4",			[KEY_SUSPEND] = "Suspend",
	[KEY_CLOSE] = "Close",			[KEY_PLAY] = "Play",
	[KEY_FASTFORWARD] = "Fast Forward",	[KEY_BASSBOOST] = "Bass Boost",
	[KEY_PRINT] = "Print",			[KEY_HP] = "HP",
	[KEY_CAMERA] = "Camera",		[KEY_SOUND] = "Sound",
	[KEY_QUESTION] = "Question",		[KEY_EMAIL] = "Email",
	[KEY_CHAT] = "Chat",			[KEY_SEARCH] = "Search",
	[KEY_CONNECT] = "Connect",		[KEY_FINANCE] = "Finance",
	[KEY_SPORT] = "Sport",			[KEY_SHOP] = "Shop",
	[KEY_ALTERASE] = "Alternate Erase",	[KEY_CANCEL] = "Cancel",
	[KEY_BRIGHTNESSDOWN] = "Brightness down", [KEY_BRIGHTNESSUP] = "Brightness up",
	[KEY_MEDIA] = "Media",			[KEY_UNKNOWN] = "Unknown",
	[BTN_0] = "Btn0",			[BTN_1] = "Btn1",
	[BTN_2] = "Btn2",			[BTN_3] = "Btn3",
	[BTN_4] = "Btn4",			[BTN_5] = "Btn5",
	[BTN_6] = "Btn6",			[BTN_7] = "Btn7",
	[BTN_8] = "Btn8",			[BTN_9] = "Btn9",
	[BTN_LEFT] = "LeftBtn",			[BTN_RIGHT] = "RightBtn",
	[BTN_MIDDLE] = "MiddleBtn",		[BTN_SIDE] = "SideBtn",
	[BTN_EXTRA] = "ExtraBtn",		[BTN_FORWARD] = "ForwardBtn",
	[BTN_BACK] = "BackBtn",			[BTN_TASK] = "TaskBtn",
	[BTN_TRIGGER] = "Trigger",		[BTN_THUMB] = "ThumbBtn",
	[BTN_THUMB2] = "ThumbBtn2",		[BTN_TOP] = "TopBtn",
	[BTN_TOP2] = "TopBtn2",			[BTN_PINKIE] = "PinkieBtn",
	[BTN_BASE] = "BaseBtn",			[BTN_BASE2] = "BaseBtn2",
	[BTN_BASE3] = "BaseBtn3",		[BTN_BASE4] = "BaseBtn4",
	[BTN_BASE5] = "BaseBtn5",		[BTN_BASE6] = "BaseBtn6",
	[BTN_DEAD] = "BtnDead",			[BTN_A] = "BtnA",
	[BTN_B] = "BtnB",			[BTN_C] = "BtnC",
	[BTN_X] = "BtnX",			[BTN_Y] = "BtnY",
	[BTN_Z] = "BtnZ",			[BTN_TL] = "BtnTL",
	[BTN_TR] = "BtnTR",			[BTN_TL2] = "BtnTL2",
	[BTN_TR2] = "BtnTR2",			[BTN_SELECT] = "BtnSelect",
	[BTN_START] = "BtnStart",		[BTN_MODE] = "BtnMode",
	[BTN_THUMBL] = "BtnThumbL",		[BTN_THUMBR] = "BtnThumbR",
	[BTN_TOOL_PEN] = "ToolPen",		[BTN_TOOL_RUBBER] = "ToolRubber",
	[BTN_TOOL_BRUSH] = "ToolBrush",		[BTN_TOOL_PENCIL] = "ToolPencil",
	[BTN_TOOL_AIRBRUSH] = "ToolAirbrush",	[BTN_TOOL_FINGER] = "ToolFinger",
	[BTN_TOOL_MOUSE] = "ToolMouse",		[BTN_TOOL_LENS] = "ToolLens",
	[BTN_TOUCH] = "Touch",			[BTN_STYLUS] = "Stylus",
	[BTN_STYLUS2] = "Stylus2",		[BTN_TOOL_DOUBLETAP] = "Tool Doubletap",
	[BTN_TOOL_TRIPLETAP] = "Tool Tripletap", [BTN_GEAR_DOWN] = "WheelBtn",
	[BTN_GEAR_UP] = "Gear up",		[KEY_OK] = "Ok",
	[KEY_SELECT] = "Select",		[KEY_GOTO] = "Goto",
	[KEY_CLEAR] = "Clear",			[KEY_POWER2] = "Power2",
	[KEY_OPTION] = "Option",		[KEY_INFO] = "Info",
	[KEY_TIME] = "Time",			[KEY_VENDOR] = "Vendor",
	[KEY_ARCHIVE] = "Archive",		[KEY_PROGRAM] = "Program",
	[KEY_CHANNEL] = "Channel",		[KEY_FAVORITES] = "Favorites",
	[KEY_EPG] = "EPG",			[KEY_PVR] = "PVR",
	[KEY_MHP] = "MHP",			[KEY_LANGUAGE] = "Language",
	[KEY_TITLE] = "Title",			[KEY_SUBTITLE] = "Subtitle",
	[KEY_ANGLE] = "Angle",			[KEY_ZOOM] = "Zoom",
	[KEY_MODE] = "Mode",			[KEY_KEYBOARD] = "Keyboard",
	[KEY_SCREEN] = "Screen",		[KEY_PC] = "PC",
	[KEY_TV] = "TV",			[KEY_TV2] = "TV2",
	[KEY_VCR] = "VCR",			[KEY_VCR2] = "VCR2",
	[KEY_SAT] = "Sat",			[KEY_SAT2] = "Sat2",
	[KEY_CD] = "CD",			[KEY_TAPE] = "Tape",
	[KEY_RADIO] = "Radio",			[KEY_TUNER] = "Tuner",
	[KEY_PLAYER] = "Player",		[KEY_TEXT] = "Text",
	[KEY_DVD] = "DVD",			[KEY_AUX] = "Aux",
	[KEY_MP3] = "MP3",			[KEY_AUDIO] = "Audio",
	[KEY_VIDEO] = "Video",			[KEY_DIRECTORY] = "Directory",
	[KEY_LIST] = "List",			[KEY_MEMO] = "Memo",
	[KEY_CALENDAR] = "Calendar",		[KEY_RED] = "Red",
	[KEY_GREEN] = "Green",			[KEY_YELLOW] = "Yellow",
	[KEY_BLUE] = "Blue",			[KEY_CHANNELUP] = "ChannelUp",
	[KEY_CHANNELDOWN] = "ChannelDown",	[KEY_FIRST] = "First",
	[KEY_LAST] = "Last",			[KEY_AB] = "AB",
	[KEY_NEXT] = "Next",			[KEY_RESTART] = "Restart",
	[KEY_SLOW] = "Slow",			[KEY_SHUFFLE] = "Shuffle",
	[KEY_BREAK] = "Break",			[KEY_PREVIOUS] = "Previous",
	[KEY_DIGITS] = "Digits",		[KEY_TEEN] = "TEEN",
	[KEY_TWEN] = "TWEN",			[KEY_DEL_EOL] = "Delete EOL",
	[KEY_DEL_EOS] = "Delete EOS",		[KEY_INS_LINE] = "Insert line",
	[KEY_DEL_LINE] = "Delete line",
};

char *absval[5] = { "Value", "Min  ", "Max  ", "Fuzz ", "Flat " };

char *relatives[REL_MAX + 1] = {
	[0 ... REL_MAX] = NULL,
	[REL_X] = "X",			[REL_Y] = "Y",
	[REL_Z] = "Z",			[REL_HWHEEL] = "HWheel",
	[REL_DIAL] = "Dial",		[REL_WHEEL] = "Wheel", 
	[REL_MISC] = "Misc",	
};

char *absolutes[ABS_MAX + 1] = {
	[0 ... ABS_MAX] = NULL,
	[ABS_X] = "X",			[ABS_Y] = "Y",
	[ABS_Z] = "Z",			[ABS_RX] = "Rx",
	[ABS_RY] = "Ry",		[ABS_RZ] = "Rz",
	[ABS_THROTTLE] = "Throttle",	[ABS_RUDDER] = "Rudder",
	[ABS_WHEEL] = "Wheel",		[ABS_GAS] = "Gas",
	[ABS_BRAKE] = "Brake",		[ABS_HAT0X] = "Hat0X",
	[ABS_HAT0Y] = "Hat0Y",		[ABS_HAT1X] = "Hat1X",
	[ABS_HAT1Y] = "Hat1Y",		[ABS_HAT2X] = "Hat2X",
	[ABS_HAT2Y] = "Hat2Y",		[ABS_HAT3X] = "Hat3X",
	[ABS_HAT3Y] = "Hat 3Y",		[ABS_PRESSURE] = "Pressure",
	[ABS_DISTANCE] = "Distance",	[ABS_TILT_X] = "XTilt",
	[ABS_TILT_Y] = "YTilt",		[ABS_TOOL_WIDTH] = "Tool Width",
	[ABS_VOLUME] = "Volume",	[ABS_MISC] = "Misc",
};

char *misc[MSC_MAX + 1] = {
	[ 0 ... MSC_MAX] = NULL,
	[MSC_SERIAL] = "Serial",	[MSC_PULSELED] = "Pulseled",
	[MSC_GESTURE] = "Gesture",	[MSC_RAW] = "RawData",
	[MSC_SCAN] = "ScanCode",
};

char *leds[LED_MAX + 1] = {
	[0 ... LED_MAX] = NULL,
	[LED_NUML] = "NumLock",		[LED_CAPSL] = "CapsLock", 
	[LED_SCROLLL] = "ScrollLock",	[LED_COMPOSE] = "Compose",
	[LED_KANA] = "Kana",		[LED_SLEEP] = "Sleep", 
	[LED_SUSPEND] = "Suspend",	[LED_MUTE] = "Mute",
	[LED_MISC] = "Misc",
};

char *repeats[REP_MAX + 1] = {
	[0 ... REP_MAX] = NULL,
	[REP_DELAY] = "Delay",		[REP_PERIOD] = "Period"
};

char *sounds[SND_MAX + 1] = {
	[0 ... SND_MAX] = NULL,
	[SND_CLICK] = "Click",		[SND_BELL] = "Bell",
	[SND_TONE] = "Tone"
};

char **names[EV_MAX + 1] = {
	[0 ... EV_MAX] = NULL,
	[EV_SYN] = events,			[EV_KEY] = keys,
	[EV_REL] = relatives,			[EV_ABS] = absolutes,
	[EV_MSC] = misc,			[EV_LED] = leds,
	[EV_SND] = sounds,			[EV_REP] = repeats,
};




#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x)-1)/BITS_PER_LONG)+1)
#define OFF(x)  ((x)%BITS_PER_LONG)
#define BIT(x)  (1UL<<OFF(x))
#define LONG(x) ((x)/BITS_PER_LONG)
#define test_bit(bit, array)	((array[LONG(bit)] >> OFF(bit)) & 1)

//bcm2708_vcbuttons
//bcm2708_vctouch
//Dell Dell USB Entry Keyboard

static char *get_event_for(const char *name)
{
	char buf[256];
	static char fname[256];
	int i;
	int fd;
	for (i=0; i<8; i++)
	{
		sprintf(fname, "/dev/input/event%d", i);
		fd = open(fname, O_RDONLY|O_NONBLOCK);
      if (fd>=0) {
			ioctl(fd, EVIOCGNAME(sizeof(buf)), buf);
			close (fd);
			if (strstr(buf, name)) {
				 return fname;
			}
		}
	}
	return NULL;
}

static int button_read_init()
{
	int i, j, k;
	//struct input_event ev[64];
	int version;
	unsigned short id[4];
	unsigned long bit[EV_MAX][NBITS(KEY_MAX)];
	char name[256] = "Unknown";
	int abs[5];
	const char *fname = get_event_for("vcbuttons");

	if (!fname) {
		//perror("evtest: No vcbuttons found");
		return 1;
	}

	if ((fd_buttons = open(fname, O_RDONLY|O_NONBLOCK)) < 0) {
		perror("evtest: could not open /dev/input/event");
		return 1;
	}

	if (ioctl(fd_buttons, EVIOCGVERSION, &version)) {
		perror("evtest: can't get version");
		return 1;
	}

	info_printf("Input driver version is %d.%d.%d\n",
		version >> 16, (version >> 8) & 0xff, version & 0xff);

	ioctl(fd_buttons, EVIOCGID, id);
	info_printf("Input device ID: bus 0x%x vendor 0x%x product 0x%x version 0x%x\n",
		id[ID_BUS], id[ID_VENDOR], id[ID_PRODUCT], id[ID_VERSION]);

	ioctl(fd_buttons, EVIOCGNAME(sizeof(name)), name);
	info_printf("Input device name: \"%s\"\n", name);

	memset(bit, 0, sizeof(bit));
	ioctl(fd_buttons, EVIOCGBIT(0, EV_MAX), bit[0]);
	info_printf("Supported events:\n");

	for (i = 0; i < EV_MAX; i++)
		if (test_bit(i, bit[0])) {
			info_printf("  Event type %d (%s)\n", i, events[i] ? events[i] : "?");
			if (!i) continue;
			ioctl(fd_buttons, EVIOCGBIT(i, KEY_MAX), bit[i]);
			for (j = 0; j < KEY_MAX; j++) 
				if (test_bit(j, bit[i])) {
					info_printf("    Event code %d (%s)\n", j, names[i] ? (names[i][j] ? names[i][j] : "?") : "?");
					if (i == EV_ABS) {
						ioctl(fd_buttons, EVIOCGABS(j), abs);
						for (k = 0; k < 5; k++) {
							if ((k < 3) || abs[k]) {
								info_printf("      %s %6d\n", absval[k], abs[k]);
                                                        }
                                                }
					}
				}
		}

	return 0;
}



static int keyboard_read_init()
{
	return 0;
}


static int touch_read_init()
{
	int i, j, k;
	//struct input_event ev[64];
	int version;
	unsigned short id[4];
	unsigned long bit[EV_MAX][NBITS(KEY_MAX)];
	char name[256] = "Unknown";
	int abs[5];
	const char *fname = get_event_for("vctouch");

	if (!fname) {
		//perror("evtest: No vctouch found");
		return 1;
	}

	if ((fd_touch = open(fname, O_RDONLY)) < 0) {
		perror("evtest: could not open /dev/input/event");
		return 1;
	}

	if (ioctl(fd_touch, EVIOCGVERSION, &version)) {
		perror("evtest: can't get version");
		return 1;
	}

	info_printf("Input driver version is %d.%d.%d\n",
		version >> 16, (version >> 8) & 0xff, version & 0xff);

	ioctl(fd_touch, EVIOCGID, id);
	info_printf("Input device ID: bus 0x%x vendor 0x%x product 0x%x version 0x%x\n",
		id[ID_BUS], id[ID_VENDOR], id[ID_PRODUCT], id[ID_VERSION]);

	ioctl(fd_touch, EVIOCGNAME(sizeof(name)), name);
	info_printf("Input device name: \"%s\"\n", name);

	memset(bit, 0, sizeof(bit));
	ioctl(fd_touch, EVIOCGBIT(0, EV_MAX), bit[0]);
	info_printf("Supported events:\n");

	for (i = 0; i < EV_MAX; i++)
		if (test_bit(i, bit[0])) {
			info_printf("  Event type %d (%s)\n", i, events[i] ? events[i] : "?");
			if (!i) continue;
			ioctl(fd_touch, EVIOCGBIT(i, KEY_MAX), bit[i]);
			for (j = 0; j < KEY_MAX; j++) 
				if (test_bit(j, bit[i])) {
					info_printf("    Event code %d (%s)\n", j, names[i] ? (names[i][j] ? names[i][j] : "?") : "?");
					if (i == EV_ABS) {
						ioctl(fd_touch, EVIOCGABS(j), abs);
						for (k = 0; k < 5; k++) {
							if ((k < 3) || abs[k]) {
								info_printf("      %s %6d\n", absval[k], abs[k]);
                                                        }
                                                }
					}
				}
		}

	return 0;
}

static int button_read(buttons_t *buttons)
{
	struct input_event ev[64];
	int rd=0, i;
        uint32_t keys_pressed, keys_held, keys_released;

		if (fd_buttons >= 0)
		rd = read(fd_buttons, ev, sizeof(struct input_event) * 64);

		if (rd < (int) sizeof(struct input_event)) {
			//perror("evtest: error reading");
			if (buttons_poll_keypad(buttons, &keys_pressed, &keys_held, &keys_released))
               buttons_handle_events(buttons, keys_pressed, keys_held, keys_released);
			return 1;
		}

		for (i = 0; i < rd / sizeof(struct input_event); i++)

			if (ev[i].type == EV_SYN) {
				info_printf("Event: time %ld.%06ld, -------------- %s ------------\n",
					ev[i].time.tv_sec, ev[i].time.tv_usec, ev[i].code ? "Config Sync" : "Report Sync" );
			} else if (ev[i].type == EV_MSC && (ev[i].code == MSC_RAW || ev[i].code == MSC_SCAN)) {
				info_printf("Event: time %ld.%06ld, type %d (%s), code %d (%s), value %02x\n",
					ev[i].time.tv_sec, ev[i].time.tv_usec, ev[i].type,
					events[ev[i].type] ? events[ev[i].type] : "?",
					ev[i].code,
					names[ev[i].type] ? (names[ev[i].type][ev[i].code] ? names[ev[i].type][ev[i].code] : "?") : "?",
					ev[i].value);
			} else {
				info_printf("Event: time %ld.%06ld, type %d (%s), code %d (%s), value %d\n",
					ev[i].time.tv_sec, ev[i].time.tv_usec, ev[i].type,
					events[ev[i].type] ? events[ev[i].type] : "?",
					ev[i].code,
					names[ev[i].type] ? (names[ev[i].type][ev[i].code] ? names[ev[i].type][ev[i].code] : "?") : "?",
					ev[i].value);
#define DO_BUTTON(b, v) do {if (v) buttons->eds_ren |= b; else buttons->eds_fen |= b; } while (0)
				if (ev[i].type == EV_KEY && ev[i].code == BTN_0) DO_BUTTON(BUTTON_KEY_NUM1, ev[i].value);
				if (ev[i].type == EV_KEY && ev[i].code == BTN_1) DO_BUTTON(BUTTON_KEY_NUM3, ev[i].value);
				if (ev[i].type == EV_KEY && ev[i].code == BTN_2) DO_BUTTON(BUTTON_KEY_NUM7, ev[i].value);
				if (ev[i].type == EV_KEY && ev[i].code == BTN_3) DO_BUTTON(BUTTON_KEY_NUM9, ev[i].value);
				if (ev[i].type == EV_KEY && ev[i].code == BTN_4) DO_BUTTON(BUTTON_KEY_ENTER, ev[i].value);
				if (ev[i].type == EV_KEY && ev[i].code == BTN_5) DO_BUTTON(BUTTON_KEY_LEFT, ev[i].value);
				if (ev[i].type == EV_KEY && ev[i].code == BTN_6) DO_BUTTON(BUTTON_KEY_RIGHT, ev[i].value);
				if (ev[i].type == EV_KEY && ev[i].code == BTN_7) DO_BUTTON(BUTTON_KEY_DOWN, ev[i].value);
				if (ev[i].type == EV_KEY && ev[i].code == BTN_8) DO_BUTTON(BUTTON_KEY_UP, ev[i].value);
				if (buttons_poll_keypad(buttons, &keys_pressed, &keys_held, &keys_released))
                   buttons_handle_events(buttons, keys_pressed, keys_held, keys_released);
			}	
	return 0;
}

static struct termios orig_termios;
static void restore_termios (void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

void sig_handler(int s)
{
  restore_termios();
  exit(1);
}


static int getkey() 
{
  static int first_time = 1;
  int character, c;
  if (first_time) {
	  struct termios new_termios;

	  tcgetattr(STDIN_FILENO, &orig_termios);

	  new_termios             = orig_termios;
	  new_termios.c_lflag     &= ~(ICANON | ECHO | ECHOCTL | ECHONL);
	  new_termios.c_cflag     |= HUPCL;
	  new_termios.c_cc[VMIN]  = 0;

	  tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
	  atexit(restore_termios);

  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = sig_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);
  sigaction(SIGTSTP, &sigIntHandler, NULL);

          first_time = 0;
  }
  while((c = fgetc(stdin)) != EOF) character = (character << 8) | c;

  return character;
}

static int keyboard_read(buttons_t *buttons)
{
   uint32_t keys_pressed, keys_held, keys_released;
   int key=0, k = getkey();
   if (k <= 0) return 1;

   if (k == 0xa) key = KEY_ENTER;
   else if (k == 0x5b41 || k == 0x1b5b41) key = KEY_UP;
   else if (k == 0x5b42 || k == 0x1b5b42) key = KEY_DOWN;
   else if (k == 0x5b43 || k == 0x1b5b43) key = KEY_RIGHT;
   else if (k == 0x5b44 || k == 0x1b5b44) key = KEY_LEFT;
   else if (k == '1' || k == 0x5b31317e || k == 0x1b5b5b41) key = KEY_F1;
   else if (k == '2' || k == 0x5b31327e || k == 0x1b5b5b42) key = KEY_F2;
   else if (k == '3' || k == 0x5b31337e || k == 0x1b5b5b43) key = KEY_F3;
   else if (k == '4' || k == 0x5b31347e || k == 0x1b5b5b44) key = KEY_F4;
   else if (k == '5' || k == 0x5b31357e || k == 0x1b5b5b45) key = KEY_F5;
   else if (k == '6' || k == 0x5b31377e || k == 0x5b31377e) key = KEY_F6;

   //printf("getkey()=%x\n", k);
#define DO_BUTTON(b, v) do {if (v) buttons->eds_ren |= b; else buttons->eds_fen |= b; } while (0)
	if (key == KEY_ENTER) DO_BUTTON(BUTTON_KEY_NUM1, 1);
	if (key == KEY_F1) DO_BUTTON(BUTTON_KEY_NUM1, 1);
	if (key == KEY_F2) DO_BUTTON(BUTTON_KEY_NUM3, 1);
	if (key == KEY_F3) DO_BUTTON(BUTTON_KEY_NUM7, 1);
	if (key == KEY_F4) DO_BUTTON(BUTTON_KEY_NUM9, 1);
	if (key == KEY_F5) DO_BUTTON(BUTTON_KEY_NUM5, 1);
	if (key == KEY_F6) DO_BUTTON(BUTTON_KEY_ENTER, 1);
	if (key == KEY_LEFT) DO_BUTTON(BUTTON_KEY_LEFT, 1);
	if (key == KEY_RIGHT) DO_BUTTON(BUTTON_KEY_RIGHT, 1);
	if (key == KEY_DOWN) DO_BUTTON(BUTTON_KEY_DOWN, 1);
	if (key == KEY_UP) DO_BUTTON(BUTTON_KEY_UP, 1);
	if (buttons_poll_keypad(buttons, &keys_pressed, &keys_held, &keys_released))
	buttons_handle_events(buttons, keys_pressed, keys_held, keys_released);

	if (key == KEY_ENTER) DO_BUTTON(BUTTON_KEY_NUM1, 0);
	if (key == KEY_F1) DO_BUTTON(BUTTON_KEY_NUM1, 0);
	if (key == KEY_F2) DO_BUTTON(BUTTON_KEY_NUM3, 0);
	if (key == KEY_F3) DO_BUTTON(BUTTON_KEY_NUM7, 0);
	if (key == KEY_F4) DO_BUTTON(BUTTON_KEY_NUM9, 0);
	if (key == KEY_F5) DO_BUTTON(BUTTON_KEY_NUM5, 0);
	if (key == KEY_F6) DO_BUTTON(BUTTON_KEY_ENTER, 0);
	if (key == KEY_LEFT) DO_BUTTON(BUTTON_KEY_LEFT, 0);
	if (key == KEY_RIGHT) DO_BUTTON(BUTTON_KEY_RIGHT, 0);
	if (key == KEY_DOWN) DO_BUTTON(BUTTON_KEY_DOWN, 0);
	if (key == KEY_UP) DO_BUTTON(BUTTON_KEY_UP, 0);
	if (buttons_poll_keypad(buttons, &keys_pressed, &keys_held, &keys_released))
	buttons_handle_events(buttons, keys_pressed, keys_held, keys_released);


	return 0;
}

static int latest_touch_x=-1, latest_touch_y=-1; static int latest_touched = 0;
static int touch_read(void *touchscreen)
{
	struct input_event ev[64];
	int rd=0, i;

		if (fd_touch >= 0)
		rd = read(fd_touch, ev, sizeof(struct input_event) * 64);

		if (rd < (int) sizeof(struct input_event)) {
			//perror("evtest: error reading");
			return 1;
		}

		for (i = 0; i < rd / sizeof(struct input_event); i++)

			if (ev[i].type == EV_SYN) {
				info_printf("Event: time %ld.%06ld, -------------- %s ------------\n",
					ev[i].time.tv_sec, ev[i].time.tv_usec, ev[i].code ? "Config Sync" : "Report Sync" );
			} else if (ev[i].type == EV_MSC && (ev[i].code == MSC_RAW || ev[i].code == MSC_SCAN)) {
				info_printf("Event: time %ld.%06ld, type %d (%s), code %d (%s), value %02x\n",
					ev[i].time.tv_sec, ev[i].time.tv_usec, ev[i].type,
					events[ev[i].type] ? events[ev[i].type] : "?",
					ev[i].code,
					names[ev[i].type] ? (names[ev[i].type][ev[i].code] ? names[ev[i].type][ev[i].code] : "?") : "?",
					ev[i].value);
			} else {
				info_printf("Event: time %ld.%06ld, type %d (%s), code %d (%s), value %d\n",
					ev[i].time.tv_sec, ev[i].time.tv_usec, ev[i].type,
					events[ev[i].type] ? events[ev[i].type] : "?",
					ev[i].code,
					names[ev[i].type] ? (names[ev[i].type][ev[i].code] ? names[ev[i].type][ev[i].code] : "?") : "?",
					ev[i].value);
#if 1
   {
        static int last_touched = 0;
        static int touch_x=-1, touch_y=-1; static int touched = 0;
        TOUCHSCREEN_EVENT_T event = {0};
	if (ev[i].type == EV_KEY && ev[i].code == BTN_TOUCH) {
           latest_touched = touched = ev[i].value;
           touch_x = touch_y = -1;
        }
	if (ev[i].type == EV_ABS && ev[i].code == ABS_X) {
           latest_touch_x = touch_x = ev[i].value;
           event.type = TOUCHSCREEN_EVENT_TYPE_ABS_X; event.param = touch_x; touchscreen_handle_events(touchscreen, event);
        }
	if (ev[i].type == EV_ABS && ev[i].code == ABS_Y) {
           latest_touch_y = touch_y = ev[i].value;
           event.type = TOUCHSCREEN_EVENT_TYPE_ABS_Y; event.param = touch_y; touchscreen_handle_events(touchscreen, event);
        }
//printf("Touch (%d,%d) [%d:%d]\n", touch_x, touch_y, touched, last_touched);
        if (touch_x >=0 && touch_y >= 0 && touched && !last_touched) {
           event.type = TOUCHSCREEN_EVENT_TYPE_FINGER; event.param = touched; touchscreen_handle_events(touchscreen, event);
           last_touched = touched;
        }
        if (!touched && last_touched) {
           event.type = TOUCHSCREEN_EVENT_TYPE_FINGER; event.param = touched; touchscreen_handle_events(touchscreen, event);
           last_touched = touched;
        }
    }
#endif
   }

	return 0;
}

#endif

static void *button_task(void *param)
{
  while(1)
    {
      if (button_read(param) && keyboard_read(param))
        vcos_sleep(10);
    }
  return NULL;
}
static void *touch_task(void *param)
{
  while(1)
    {
      touch_read(param);
    }
  return NULL;
}


void
message_queue_init ()
{
  vcos_mutex_create (&msg_latch, "message latch");  
  vcos_semaphore_create(&msg_semaphore, "message semaphore", 0);

#ifndef WIN32
  {
    int rc;
    VCOS_THREAD_ATTR_T attrs;
    rc = button_read_init();
    if (rc)
    {
        //printf("evtest: could not open button device driver\n");
        //exit(1);
    }
    rc = keyboard_read_init();
    if (rc)
    {
        //printf("evtest: could not open button device driver\n");
        //exit(1);
    }
    rc = touch_read_init();
    if (rc)
    {
        //printf("evtest: could not open touch device driver\n");
        //exit(1);
    }
    touchserv_task_main(0, 0);
    local_touch_finger_ask_events_open(&touchserv_state.touch, 0 /*transport*/, 50 /*period_ms*/, 1 /*max_fingers*/);
    local_buttons_ask_events_open(&touchserv_state.touch, 0 /*transport*/, 50 /*period_ms*/);

    vcos_thread_attr_init(&attrs);
    vcos_thread_attr_setpriority(&attrs, VCOS_THREAD_PRI_ABOVE_NORMAL);
    vcos_thread_create(&button_thread, "button thread", &attrs, button_task, &touchserv_state.touch.buttons);
    vcos_thread_attr_init(&attrs);
    vcos_thread_attr_setpriority(&attrs, VCOS_THREAD_PRI_ABOVE_NORMAL);
    vcos_thread_create(&touch_thread, "touch thread", &attrs, touch_task, &touchserv_state.touch.screen);
  }
#endif
}

void
add_message (long msg, long param1, long param2)
{
  vcos_mutex_lock (&msg_latch);
  int i = end_p + 1;
  vcos_assert (i != start_p);  /* Overlapping messages! */
  if (i == countof(message_q))
    i = 0;
  message_q[i].msg = msg;
  message_q[i].param1 = param1;
  message_q[i].param2 = param2;
  end_p = i;
  vcos_mutex_unlock (&msg_latch);
  vcos_semaphore_post(&msg_semaphore);
}

void
dispatch_messages ()
{
  while (1)
    {
      vcos_semaphore_wait(&msg_semaphore);
      vcos_mutex_lock (&msg_latch);
      vcos_assert(start_p != end_p);
      start_p++;
      if (start_p == countof(message_q))
        start_p = 0;
      vcos_mutex_unlock (&msg_latch);

      /* Call the handler routine */
      host_app_message_handler (message_q[start_p].msg,
                                message_q[start_p].param1,
                                message_q[start_p].param2);
    }
}


#ifdef WIN32
int
main (int argc, char **argv)
{
  vcos_init ();
  vcos_timer_init ();
  message_queue_init ();

  vmcs_rpc_client_start ();

  MSG msg;
  while (1)
    {
      GetMessage (&msg, NULL, 0, 0);
      DispatchMessage (&msg);
    }

  vcos_assert (0);
  vcos_deinit();  /* we never reach here */
}
#endif



static TS_INLINE void
prodee_pool_init(prodee_pool_t *pool)
{  prodee_t *p;
   for (p=&(*pool)[0]; p<&(*pool)[MAX_PRODEE_COUNT]; p++)
      p->prod_fn = NULL;
}



static TS_INLINE prodee_t *
prodee_pool_alloc(prodee_pool_t *pool,
                  uncast_function_t *prod_fn, void *prod_arg)
{  prodee_t *p=&(*pool)[0];

   while (p < &(*pool)[MAX_PRODEE_COUNT] && NULL != p->prod_fn)
      p++;

   if (p < &(*pool)[MAX_PRODEE_COUNT])
   {  p->prod_fn = prod_fn;
      p->prod_arg = prod_arg;
      p->next = NULL;
      return p;
   } else
      return NULL;
}



static TS_INLINE void
prodee_pool_free(prodee_pool_t *pool, prodee_t *p)
{  vcos_assert_msg(p >= &(*pool)[0],
                   "freeing poiner beneath pool start (%p < %p)",
                   p, &(*pool)[0]);
   vcos_assert_msg(p < &(*pool)[MAX_PRODEE_COUNT],
                   "freeing pointer above pool end (%p >= %p)",
                   p, &(*pool)[MAX_PRODEE_COUNT]);
   if (p != NULL) {
      p->prod_fn = NULL;
   }
}



static TS_INLINE void
prodlist_new(prodee_t **out_prodlist)
{  *out_prodlist = NULL;
}




#if 0
static TS_INLINE void
prodlist_delete(prodee_pool_t *pool, prodee_t **ref_prodlist)
{  while (NULL != *ref_prodlist)
   {  prodee_t *doomed = *ref_prodlist;
      *ref_prodlist = (*ref_prodlist)->next;
      prodee_pool_free(pool, doomed);
   }
}
#endif



static TS_INLINE prodee_t **
prodlist_locate(prodee_t **ref_prodlist,
                uncast_function_t *prod_fn, void *prod_arg)
{  prodee_t **ref_p = ref_prodlist;

   while (NULL != *ref_p &&
          (prod_fn != (*ref_p)->prod_fn || prod_arg != (*ref_p)->prod_arg))
      ref_p = &(*ref_p)->next;

   return ref_p;
}




      
static int /*ok*/
prodee_add(prodee_pool_t *pool, prodee_t **ref_prodlist,
           uncast_function_t *prod_fn, void *prod_arg)
{  prodee_t *p = prodee_pool_alloc(pool, prod_fn, prod_arg);

   if (NULL == p)
      return 0 /*FALSE*/;
   else
   {  p->next = *ref_prodlist;
      *ref_prodlist = p;
      return 1 /*TRUE*/;
   }
}



static int /*ok*/
prodee_remove(prodee_pool_t *pool, prodee_t **ref_prodlist,
              uncast_function_t *prod_fn, void *prod_arg)
{  prodee_t **ref_p = prodlist_locate(ref_prodlist, prod_fn, prod_arg);

   if (NULL == *ref_p)
      return 0 /*FALSE*/;   /* couldn't find the event */
   else
   {  prodee_t *doomed = *ref_p;
      
      *ref_p = doomed->next; /* delete the prodee from the list */
      prodee_pool_free(pool, doomed); /* return memory to pool */
      return 1 /*TRUE*/;
   }
}




/*****************************************************************************
 *                                                                           *
 *    Touch Server Prod Lists                                                *
 *                                                                           * 
 *****************************************************************************/





static TS_INLINE void
touchserv_prodlists_init(touchserv_prodlists_t *pl)
{  prodee_pool_init(&pl->pool);
   prodlist_new(&pl->touch_event_prodees);
   prodlist_new(&pl->button_event_prodees);
}




static TS_INLINE int /*ok*/
event_register_touch(touchserv_prodlists_t *pl,
                     vc_touch_event_fn_t *event_fn, void *prod_arg)
{  return prodee_add(&pl->pool, &pl->touch_event_prodees,
                     (uncast_function_t *)event_fn, prod_arg);
}




static TS_INLINE int /*ok*/
event_deregister_touch(touchserv_prodlists_t *pl,
                       vc_touch_event_fn_t *event_fn, void *prod_arg)
{  return prodee_remove(&pl->pool, &pl->touch_event_prodees,
                     (uncast_function_t *)event_fn, prod_arg);
}



/*! Cause a touch event
 *  We want event functions (callbacks) to be called in task context so this
 *  function should be called in task context.
 */
static void
event_cause_touch(touchserv_prodlists_t *pl,
                  vc_touch_event_t action, uint32_t action_arg)
{  prodee_t *p = pl->touch_event_prodees;
//printf("event_cause_touch(%d,%d) [%p]\n", action, action_arg, p);
   while (NULL != p)
   {  vc_touch_event_fn_t *touch_event_fn =
         (vc_touch_event_fn_t *)p->prod_fn;
      (*touch_event_fn)(p->prod_arg, action, action_arg);
      p = p->next;
   }
}




static TS_INLINE int /*ok*/
event_register_button(touchserv_prodlists_t *pl,
                      vc_button_event_fn_t *event_fn, void *prod_arg)
{  return prodee_add(&pl->pool, &pl->button_event_prodees,
                     (uncast_function_t *)event_fn, prod_arg);
}




static TS_INLINE int /*ok*/
event_deregister_button(touchserv_prodlists_t *pl,
                        vc_button_event_fn_t *event_fn, void *prod_arg)
{  return prodee_remove(&pl->pool, &pl->button_event_prodees,
                     (uncast_function_t *)event_fn, prod_arg);
}




/*! Cause a touch event
 *  We want event functions (callbacks) to be called in task context so this
 *  function should be called in task context.
 */
static TS_INLINE void
event_cause_button(touchserv_prodlists_t *pl,
                   vc_button_event_t action, vc_button_id_t button,
                   uint32_t action_arg)
{  prodee_t *p = pl->button_event_prodees;
//printf("event_cause_button(%x,%x,%x) [%p]\n", action, button, action_arg, p);

   while (NULL != p)
   {  vc_button_event_fn_t *button_event_fn =
         (vc_button_event_fn_t *)p->prod_fn;
      (*button_event_fn)(p->prod_arg, action, button, action_arg);
      p = p->next;
   }
}



/*! Initialize the touchscreen state
 *  Returns 0 iff the touscreen is operational after initialization
 *  Note that - even if it is not operational - there is enough state left in
 *  touchscreen->driver and touchscreen->handle to determine exactly what
 *  is missing.  This should allow the service to operate properly - even
 *  though there is no possibility of events arriving from the touch screen.
 */
static int
touchscreen_init(touchscreen_t *touchscreen, touchserv_prodlists_t *prodlists,
                 VCOS_EVENT_FLAGS_T *eventgroup)
{
   int success = -1;
   VCOS_STATUS_T st = VCOS_SUCCESS; //touchscreen_dev_init(&touchscreen->lastcall, eventgroup);

   memset(&touchscreen->prop, 0, sizeof(touchscreen->prop));
   memset(&touchscreen->button[0], 0, sizeof(touchscreen->button));
   touchscreen->prodlists = prodlists;

   touchscreen->prop.max_pressure = 1; /* default value */
   touchscreen->prop.scaled_width = 0x100;  /* default */
   touchscreen->prop.scaled_height = 0x100; /* default */

   if (st == VCOS_SUCCESS)
   {
            touchscreen->prop.screen_width = 1024;
            touchscreen->prop.screen_height = 1024;
            touchscreen->prop.rightward_x = 1;
            touchscreen->prop.rising_y = 0;
            touchscreen->prop.finger_count = 1;
            touchscreen->prop.max_pressure = 1; /* default value */
            touchscreen->prop.scaled_width = 0x100;  /* default */
            touchscreen->prop.scaled_height = 0x100; /* default */
            
            /* work out the max outside of which a finger moving
               causes a gesture at the moment, I'm saying its around
               1/4 of the screen */
            touchscreen->prop.gesture_box_width = touchscreen->prop.screen_width / 4;
            touchscreen->prop.gesture_box_height = touchscreen->prop.screen_height / 6;
   }
   success = 0;
   return success;
}


static void
touchscreen_buttons_get_state(touchscreen_t *touchscreen, 
                              vc_button_state_t *out_state)
{
   touch_button_t *button;
   touch_button_t *first_button = &touchscreen->button[0];
   vc_button_set_t active    = 0; /*< buttons that have been defined */
   vc_button_set_t depressed = 0; /*< buttons that are currently pressed */

   for (button = first_button;
        button < first_button + MAX_BUTTON_COUNT;
        button++)
   {
      if (button->in_use)
      {
         active |= 1<<button->button_id;
         if (0 != button->depressed)
            depressed |= 1<<button->button_id;
      }
   }
   out_state->active = active;
   out_state->depressed = depressed;
}





/*! update the state of the touch buttons and generate button events
 * 
 *  \param x, y           - position of finger on screen
 *  \param finger         - the finger in use
 *  \param button_action  - button action being emulated
 *  \param button_arg     - argument for button action
 *  \param ref_sent_event - location accumulating whether an event is sent 
 *
 *  \return 0 iff successful
 *
 *  Must be called from task context because it calls \c event_cause_*
 */
static void
touchscreen_button_messages(touchscreen_t *touchscreen, 
                            uint32_t x, uint32_t y, uint32_t finger,
                            uint32_t button_action, int32_t button_arg,
                            int *ref_sent_event)
{
   touch_button_t *button;
   touchserv_prodlists_t *prodlists = touchscreen->prodlists;
   touch_button_t *first_button = &touchscreen->button[0];
   vc_button_state_t previous_state;
   vc_button_state_t new_state;
   vc_button_set_t buttons_updated = 0;
   vc_button_id_t button_id;
   uint32_t button_mask;
   
   touchscreen_buttons_get_state(touchscreen, &previous_state);
   for (button = first_button;
        button < first_button + MAX_BUTTON_COUNT;
        button++) {
      if (button->in_use)
      {  int /*bool*/ applies = 0 /*no*/;
         int /*bool*/ intouch = 0 /*no*/;

         if (button_action == vc_button_event_release)
            applies = 1 /*yes*/;
         else if (x >= button->left_x &&
                  x < button->left_x + button->width &&
                  y >= button->bottom_y &&
                  y < button->bottom_y + button->height)
         {  intouch = 1 /*yes*/;
            applies = 1 /*yes*/;
         }

         DEBUG_TBUT(DPRINTF("--- press %d,%d %s button %d (giving %d) "
                            "<%d,%d>+<%d,%d>\n",
                            x, y,
                            applies? (intouch? "depresses":"releases"):
                                     "doesn't apply to",
                            button-first_button, button->button_id,
                            button->left_x, button->bottom_y,
                            button->width, button->height););
         if (applies)
         {  if (intouch)
               button->depressed |= 1<<finger;
            else
               button->depressed &= ~(1<<finger);
         }
      }
   }

   /* If we've mapped many touch areas to the same button this approach will
      ensure that we get only one message per button: */
   touchscreen_buttons_get_state(touchscreen, &new_state);
   buttons_updated = new_state.active &
                     (previous_state.depressed ^ new_state.depressed);

   DEBUG_TBUT(DPRINTF("--- finger %d (%d,%d) updated buttons: %X - "
                      "active %X old %X new %X\n",
                      finger, x, y, buttons_updated, new_state.active,
                      previous_state.depressed, new_state.depressed););

   for (button_id=0, button_mask=1<<0;
        button_id<32;
        button_id++, button_mask <<= 1)
   {
      if (0 != (button_mask & buttons_updated))
      {
         if (NULL != ref_sent_event)
            *ref_sent_event = 1;
         event_cause_button(prodlists, button_action, button_id, button_arg);
      }
   }
}




static int
pos_is_right(touchscreen_t *ts, int later, int earlier)
{
   if (ts->prop.rightward_x)
      return later > earlier;
   else
      return later < earlier;
}




static int
pos_is_below(touchscreen_t *ts, int later, int earlier)
{
   if (ts->prop.rising_y)
      return later < earlier;
   else
      return later > earlier;
}



/*! Cause any events triggerred by recent touchscreen device changes 
 *
 *  Must be called from task context because it calls \c event_cause_*
 */
static void
touchscreen_do_action(touchscreen_t *touchscreen, int fingerno, int pressure)
{
   
   DEBUG_TEVENT(DPRINTF("f%d-%d", fingerno, pressure););

   /* we track the coords from the first button down to the button up
    * and use this for the gestures.  This needs lots of work! hack it
    * for a demo for the time being */

   if (fingerno < MAX_FINGER_COUNT)
   {
      finger_state_t *finger_state = &touchscreen->finger[fingerno];
      int depressed = (0 != pressure);

      /* is the finger down for the first time? */
      if (depressed && !touchscreen->finger[fingerno].down)
      {
         int /*bool*/ suppress_finger_down = 0/*FALSE*/;
         
         /* store the coords */
         finger_state->start_coord = touchscreen->cur_coord;

         /* mark the finger as down */
         finger_state->down = 1/*TRUE*/;

         /* store the time at which the button was pressed */
         finger_state->down_time = vcos_get_ms();
         
         touchscreen_button_messages(touchscreen,
                                     touchscreen->cur_coord.x,
                                     touchscreen->cur_coord.y,
                                     fingerno,
                                     vc_button_event_press, /*arg*/0,
                                     &suppress_finger_down);

         finger_state->press_belongs_to_button = suppress_finger_down;

         if (!suppress_finger_down)
            /* cause finger down event */
            event_cause_touch(touchscreen->prodlists,
                              vc_touch_event_down, fingerno);
      }
      else if (!depressed && finger_state->down)
      {  /* finger has been lifted up */
         int /*bool*/ suppress_finger_up = 0/*FALSE*/;

         /* check we are not in a held state? */
         if (!finger_state->held_down)
         {
            /* work out the difference from the starting point to the
             * ending point */
            int32_t delta_x = (int32_t)touchscreen->cur_coord.x -
                              (int32_t)finger_state->start_coord.x;
            int32_t gesture_width = abs(delta_x);

            int32_t delta_y = (int32_t)touchscreen->cur_coord.y -
                              (int32_t)finger_state->start_coord.y;
            int32_t gesture_height = abs(delta_y);

            /* first, has the finger now landed outside the gesture
             * box?  at the moment, only do gestures on the x axis
             * gesture left first */
            if (gesture_width > touchscreen->prop.gesture_box_width)
            {
               vc_touch_event_t event;

               /* its a gesture left-right! but which way? */
               if (pos_is_right(touchscreen, touchscreen->cur_coord.x,
                                finger_state->start_coord.x))
                  event = vc_touch_event_gesture_right;
               else
                  event = vc_touch_event_gesture_left;

               event_cause_touch(touchscreen->prodlists, event,
                                 (0x100*gesture_width)/
                                     touchscreen->prop.screen_width);

               /* force clear the double-tap counter so this event
                  is ignored */
               finger_state->last_pressed_release_time = -1;

               /* don't send both the gesture and the touch event */
               suppress_finger_up = 1/*TRUE*/;
            }

            else if (gesture_height > touchscreen->prop.gesture_box_height)
            {
               vc_touch_event_t event;

               /* its a gesture up-down! but which way? */
               if (pos_is_below(touchscreen, touchscreen->cur_coord.y,
                                finger_state->start_coord.y))
                  event = vc_touch_event_gesture_down;
               else
                  event = vc_touch_event_gesture_up;

               event_cause_touch(touchscreen->prodlists, event,
                                 (0x100*gesture_height)/
                                     touchscreen->prop.screen_height);

               /* force clear the double-tap counter so this event is ignored */
               finger_state->last_pressed_release_time = -1;

               /* don't send both the gesture and the touch event */
               suppress_finger_up = 1;
            }

            else
            {
               /* check that the previous button event was within the
                * double-tap time?  is the last press time valid? */
               if ((-1 != finger_state->last_pressed_release_time) &&
                   finger_state->last_pressed_release_time +
                      TOUCH_DOUBLE_TAP_MS > vcos_get_ms())
               {
                  /* it was just a finger press event */
                  event_cause_touch(touchscreen->prodlists,
                                    vc_touch_event_press_tap_tap, fingerno);
                  /* now clear the double-tap time */
                  finger_state->last_pressed_release_time = -1;
               }
               else
               {
                  /* it was just the end of a a finger press event */
                  event_cause_touch(touchscreen->prodlists,
                                    vc_touch_event_press, fingerno);

                  /* set the double-tap time */
                  finger_state->last_pressed_release_time = vcos_get_ms();

                  /* check for the invalid marked */
                  if (-1 == finger_state->last_pressed_release_time)
                     finger_state->last_pressed_release_time = -2;
               }
            }
         }
         else
         {
            finger_state->held_down = 0;
         }

         /* finger up - don't miss any of these events even */
         finger_state->down = 0;

         if (finger_state->press_belongs_to_button)
            touchscreen_button_messages(touchscreen,
                                        touchscreen->cur_coord.x,
                                        touchscreen->cur_coord.y,
                                        fingerno,
                                        vc_button_event_release, /*arg*/0,
                                        NULL);
         else
            event_cause_touch(touchscreen->prodlists,
                              vc_touch_event_up, fingerno);
      }
   }
}

/*! touchscreen_handle_events
 *  Read and handle all the events available from the touchscreen driver
 *  Must be called in task context
 *
 *    \param   touchscreen   pointer to touchscreen state
 *
 *    \return                0 iff successful
 */
static int
touchscreen_handle_events(touchscreen_t *touchscreen, TOUCHSCREEN_EVENT_T event)
{
   int success = -1;
//printf("touchscreen_handle_events(%d,%d,%d,%d)\n", event.type, event.param, touchscreen->finger[0].down, touchscreen->finger[0].held_down);
   if (NULL != touchscreen)
   {
      DEBUG_TEVENT(DPRINTF("["););
      //while (0 == touchscreen_driver_get_event(&event))
      {
         switch(event.type)
         {
            /* always store the current X + Y */
            case TOUCHSCREEN_EVENT_TYPE_ABS_X:
               DEBUG_TEVENT(DPRINTF("x%d", event.param););
               touchscreen->cur_coord.x = event.param;
            break;
            
            case TOUCHSCREEN_EVENT_TYPE_ABS_Y:
            {  /* currently we can only deal with one finger */
               int fingerno = 0;
               finger_state_t *finger_state = &touchscreen->finger[fingerno];
               
               DEBUG_TEVENT(DPRINTF("y%d", event.param););
               touchscreen->cur_coord.y = event.param;

               /* Here we do something rather naughty - we use the
                * fact that a finger pressed on a screen keeps
                * generating coordinates even when its 'not moving'
                * check here for a 'finger held down' event */
               if (finger_state->down && !finger_state->held_down)
               {
                  int32_t repeat_at = finger_state->down_time + TOUCH_REPEAT_MS;
                  /* check if TOUCH_REPEAT_MS milli-seconds have
                   * passed or not? */
                  if (repeat_at < vcos_get_ms())
                  {

                     if (finger_state->press_belongs_to_button)
                        touchscreen_button_messages(touchscreen,
                                                    touchscreen->cur_coord.x,
                                                    touchscreen->cur_coord.y,
                                                    fingerno,
                                                    vc_button_event_repeat,
                                                    vcos_get_ms()-repeat_at,
                                                    NULL);
                     else
                        /* send the start of a finger held event */
                        event_cause_touch(touchscreen->prodlists,
                                          vc_touch_event_hold, fingerno);

                     /* flag that we are in the hold mode */
                     finger_state->held_down = 1;
                  }
               }
            }
            break;

#if 0
            case TOUCHSCREEN_EVENT_TYPE_ABS_TOOL_WIDTH:
               DEBUG_TEVENT(DPRINTF("w%d", event.param););
               //ignore this!
            break;
#endif            
            case TOUCHSCREEN_EVENT_TYPE_FINGER:
               /* this is the trigger for all our events - we have accumulated
                  state in previous events - now deal with it */
               touchscreen->cur_coord.pressure = event.param & 0xffff;
               touchscreen_do_action(touchscreen, /*finger*/event.param >> 16,
                                     /*pressure*/event.param & 0xffff);
            break;
#if 0                     
            // gesture events (if enabled)
            case TOUCHSCREEN_EVENT_TYPE_TAP:
               DEBUG_TEVENT(DPRINTF("<t>"););
               //ignore gestures for the time being
               break;
               
            case TOUCHSCREEN_EVENT_TYPE_TAP_HOLD:
               DEBUG_TEVENT(DPRINTF("<th>"););
               //ignore gestures for the time being
               break;
               
            case TOUCHSCREEN_EVENT_TYPE_TAP_DOUBLE:
               DEBUG_TEVENT(DPRINTF("<t2>"););
               //ignore gestures for the time being
               break;
               
            case TOUCHSCREEN_EVENT_TYPE_TAP_FAST:
               DEBUG_TEVENT(DPRINTF("<tf>"););
               //ignore gestures for the time being
               break;
               
            case TOUCHSCREEN_EVENT_TYPE_PRESS:
               DEBUG_TEVENT(DPRINTF("<p>"););
               //ignore gestures for the time being
               break;
               
            case TOUCHSCREEN_EVENT_TYPE_FLICK:
               DEBUG_TEVENT(DPRINTF("<f%-d,%-d>",
                                    /*x dist*/(int16_t)(event.param & 0xffff),
                                    /*y dist*/(int16_t)(event.param >> 16)););
               //ignore gesture
               break;
               
            case TOUCHSCREEN_EVENT_TYPE_PINCH:
               DEBUG_TEVENT(DPRINTF("<c%-d>",
                                    /*dist*/(int16_t)(event.param & 0xffff)););
               //ignore gestures for the time being
               break;
               
            case TOUCHSCREEN_EVENT_TYPE_PALM:
               DEBUG_TEVENT(DPRINTF("<m>"););
               //ignore gestures for the time being
               break;
               
            case TOUCHSCREEN_EVENT_TYPE_ROTATE:
               DEBUG_TEVENT(DPRINTF("<r%-d>",
                                    /*dist*/(int16_t)(event.param & 0xffff)););
               //ignore gestures for the time being
               break;
#endif
}
      }
      DEBUG_TEVENT(DPRINTF("]\n"););

      success = 0;
   }

   return success;
}


   
/*****************************************************************************
 *                                                                           *
 *    Buttons Service Events                                                 *
 *                                                                           * 
 *****************************************************************************/



#if 0

#define buttons_init(buttons, prodllists, eventgroup) (0)
#define buttons_end(buttons)

#else






/*! Remap keys into platform keys
 *  Returns 0 iff successful
 */
static uint32_t
button_id_to_platform(const uint32_t vcfw_keys)
{
   uint32_t platform_keys = 0;

   if (vcfw_keys & BUTTON_KEY_NUM1 ) platform_keys |= 1 << vc_key_a;
   if (vcfw_keys & BUTTON_KEY_NUM3 ) platform_keys |= 1 << vc_key_b;
   if (vcfw_keys & BUTTON_KEY_NUM7 ) platform_keys |= 1 << vc_key_c;
   if (vcfw_keys & BUTTON_KEY_NUM9 ) platform_keys |= 1 << vc_key_d;
   if (vcfw_keys & BUTTON_KEY_NUM5 ) platform_keys |= 1 << vc_key_e;
   if (vcfw_keys & BUTTON_KEY_LEFT ) platform_keys |= 1 << vc_key_left;
   if (vcfw_keys & BUTTON_KEY_RIGHT) platform_keys |= 1 << vc_key_right;
   if (vcfw_keys & BUTTON_KEY_UP   ) platform_keys |= 1 << vc_key_up;
   if (vcfw_keys & BUTTON_KEY_DOWN ) platform_keys |= 1 << vc_key_down;
   if (vcfw_keys & BUTTON_KEY_ENTER) platform_keys |= 1 << vc_key_z;
   if (vcfw_keys & BUTTON_KEY_STAR ) platform_keys |= 1 << vc_key_pageup;
   if (vcfw_keys & BUTTON_KEY_HASH ) platform_keys |= 1 << vc_key_pagedown;

   return platform_keys;
}



/*! Remaps platform keys into our keys
 *  Returns 0 if successful
 */
static uint32_t
button_id_from_platform( const uint32_t platform_keys )
{
   uint32_t button_keys = 0;

   if (platform_keys & (1 << vc_key_a)    ) button_keys |= BUTTON_KEY_NUM1;
   if (platform_keys & (1 << vc_key_b)    ) button_keys |= BUTTON_KEY_NUM3;
   if (platform_keys & (1 << vc_key_c)    ) button_keys |= BUTTON_KEY_NUM7;
   if (platform_keys & (1 << vc_key_d)    ) button_keys |= BUTTON_KEY_NUM9;
   if (platform_keys & (1 << vc_key_e)    ) button_keys |= BUTTON_KEY_NUM5;
   if (platform_keys & (1 << vc_key_left) ) button_keys |= BUTTON_KEY_LEFT;
   if (platform_keys & (1 << vc_key_right)) button_keys |= BUTTON_KEY_RIGHT;
   if (platform_keys & (1 << vc_key_up)   ) button_keys |= BUTTON_KEY_UP;
   if (platform_keys & (1 << vc_key_down) ) button_keys |= BUTTON_KEY_DOWN;
   if (platform_keys & (1 << vc_key_z)    ) button_keys |= BUTTON_KEY_ENTER;

   return button_keys;
}






/*! Extract the bit position from a bitfield
 *  Only gets the first bit set's position
 *  Returns the bit position.
 */
static uint32_t
get_lsbit_pos( const uint32_t bit_field )
{
   uint32_t bit_pos = 0;
   uint32_t count = 0;

   for (count = 0; count < 32; count++)
   {
      if (0 != ((0x1 << count) & bit_field))
      {
         bit_pos = count;
         break;
      }
   }

   return bit_pos;
}





/*! poll the keyboard for presses
 *  Returns int > 0 if any buttons have been pressed
 */
static int
buttons_poll_keypad(buttons_t *buttons,
                    uint32_t *keys_pressed, uint32_t *keys_held,
                    uint32_t *keys_released )
{
   int key_change = 0;

   /* parameter check */
   if (NULL != buttons &&
       NULL != keys_pressed &&
       NULL != keys_held &&
       NULL != keys_released)
   {
      uint32_t bit_count = 0;
      button_time_t *held_time = &buttons->button_held_time[0];

      /* check for any new key presses
       * all keys that are now selected that wern't previously selected
       */
      *keys_pressed = buttons->eds_ren & BUTTON_KEY_ANY;

      buttons->eds_ren = 0;

      /* check for keys released
       * all keys that are no longer in the last keys that were previously
       */  
      *keys_released = buttons->eds_fen & BUTTON_KEY_ANY;

      buttons->eds_fen = 0;

      /* are we interested in repeating buttons? */
      if (0 != buttons->repeat_rate_ms)
      {
         int32_t now_ms = vcos_get_ms();
         
         /* set the keys held to 0 for the time being */
         *keys_held = 0;

         /* have any new keys been pressed? */
         if (*keys_pressed)
         {
            /* store the time when these buttons were initially pressed */
            for (bit_count = 0; bit_count < BUTTONS_NUM_OF_KEYS; bit_count++)
            {
               if ( (1 << bit_count) & *keys_pressed )
               {
                  /* store the time when this button was pressed */
                  held_time[bit_count].current = vcos_get_ms();

                  /* clear the total held time */
                  held_time[bit_count].total = 0;
               }
            }
         }

         /* have any new keys been pressed? */
         if (*keys_released)
         {
            //clear any buttons that have been released
            for (bit_count = 0; bit_count < BUTTONS_NUM_OF_KEYS; bit_count++)
            {
               if (0 != ((1 << bit_count) & *keys_released))
               {
                  /* store the time whenthis button was pressed */
                  held_time[bit_count].current = 0;
                  held_time[bit_count].total = 0;
               }
            }
         }

         /* check for any buttons that have repeated */
         for (bit_count = 0; bit_count < BUTTONS_NUM_OF_KEYS; bit_count++)
         {
            uint32_t current_held_time = held_time[bit_count].current;
            /* a none 0 value means the button is repeating */
            if (0 != current_held_time)
            {
               /* work out if the time has expired? */
               if ((now_ms - current_held_time) >= buttons->repeat_rate_ms)
               {
                  /* mark this button as repeating */
                  *keys_held |= (1 << bit_count);

                  /* add on to the buttons total held time the different
                   * between now and the last time we saw it
                   */
                  held_time[bit_count].total += (now_ms - current_held_time);

                  /* reset this buttons time to the current */
                  held_time[bit_count].current = now_ms;
               }
            }
         }
      }

      /* return an OR of the keys pressed */
      key_change = (*keys_pressed | *keys_released | *keys_held);
   }

   return key_change;
}





/*! Poll the keyboard for presses, releases and repeats
 *  This functions doesn't alter button state - it just signals that it is
 *  worthwhile calling buttons_handle_keys().
 *  Returns TRUE iff some button handling is necessary.
 *  Can be called from any context (including LISR)
 */
static int /*bool*/
buttons_new_event(const buttons_t *buttons)
{
   int /* bool*/ changed = 0/*FALSE*/;

   /* parameter check */
   if (NULL != buttons)
   {
      uint32_t keys_pressed  = buttons->eds_ren & BUTTON_KEY_ANY;
      uint32_t keys_released = buttons->eds_fen & BUTTON_KEY_ANY;
      
      /* check for any new key presses
       * all keys that are now selected that wern't previously selected
       */
      if (0 != keys_pressed || 0 != keys_released)
          changed = 1/*TRUE*/;
      else
      /* are we interested in repeating buttons? */
      if (0 != buttons->repeat_rate_ms)
      {
         uint32_t bit_count = 0;
         int32_t now_ms = vcos_get_ms();
         /* set the keys held to 0 for the time being */

         /* check for any buttons that have repeated */
         for (bit_count = 0; bit_count < BUTTONS_NUM_OF_KEYS; bit_count++)
         {
            uint32_t current_held_time =
               buttons->button_held_time[bit_count].current;
            /* a non-zero value means the button is repeating */
            if (0 != current_held_time)
            {  /* work out if the time has expired? */
               if ((now_ms - current_held_time) >= buttons->repeat_rate_ms)
                  changed = 1/*TRUE*/;
            }
         }
      }
   }

   return changed;
}





/*! Create events that can be sent as messages to the client
 *  Returns 0 if successful
 *  Must be called from task context because it calls \c event_cause_button
 */
static int
buttons_do_action(touchserv_prodlists_t *prodlists,
                  const uint32_t keys_pressed, const uint32_t keys_held,
                  const uint32_t keys_released,
                  const button_time_t *button_held_time)
{
   int success = 0;
   uint32_t button_id = 0;
   //printf("buttons_do_action(%x,%x,%x) [%p,%p]\n", keys_pressed, keys_held, keys_released, prodlists, button_held_time);
   /* param check */
   if (0 != keys_pressed)
   {
      /* map the buttons IDs to platform keys... */
      uint32_t remapped_keys_pressed = button_id_to_platform(keys_pressed);

      /* are any of the keys we are interested in being pressed? */
      if (0 != remapped_keys_pressed)
         for (button_id = 0; button_id <= VC_KEY_NAME_MAX; button_id++)
            if (0 != ((1 << button_id) & remapped_keys_pressed))
               event_cause_button(prodlists, vc_button_event_press,
                                  button_id, /*button arg non zero indicates platform key press origin*/1);
   }

   /* param check */
   if (0 != keys_held)
   {
      /* map the VCFW keys to platform keys... */
      uint32_t remapped_keys_held = button_id_to_platform(keys_held);

      /* are any of the keys we are interested in being held down? */
      if (0 != remapped_keys_held)
         for (button_id = 0; button_id <= VC_KEY_NAME_MAX; button_id++)
            if (0 != ((1 << button_id) & remapped_keys_held))
            {
               /* If we are interested in sending a "key held" message to the
                * client, we need to find the button held time.  This is stored
                * in an array of buttons, not platform, so we have to reverse
                * the mapping...
                */
               uint32_t button_keys =
                  button_id_from_platform(remapped_keys_held);
               uint32_t held_time =
                  button_held_time[get_lsbit_pos(button_keys)].total;
               event_cause_button(prodlists, vc_button_event_repeat,
                                  button_id, /*button arg*/held_time);
            }
   }

   /* param check */
   if (0 != keys_released)
   {
      /* map the VCFW keys to platform keys... */
      uint32_t remapped_keys_released = button_id_to_platform(keys_released);

      /* have any of the keys we are interested been released? */
      if (0 != remapped_keys_released)
         for (button_id = 0; button_id <= VC_KEY_NAME_MAX; button_id++)
            if (0 != ((1 << button_id) & remapped_keys_released))
               event_cause_button(prodlists, vc_button_event_release,
                                  button_id, /*button arg non zero indicates platform key press origin*/1);
   }

   return success;
}





/*! Discover and handle any signalled button event
 *  Should only be called from task context (including LISR)
 */
static void
buttons_handle_events(buttons_t *buttons, uint32_t keys_pressed, uint32_t keys_held, uint32_t keys_released)
{
      buttons_do_action(buttons->prodlists,
                        keys_pressed, keys_held, keys_released,
                        &buttons->button_held_time[0]);
}




/*! Initialize the buttons state
 *  Returns 0 iff the button keys are operational after initialization
 */
static int /*rc*/
buttons_init(buttons_t *buttons, touchserv_prodlists_t *prodlists,
             VCOS_EVENT_FLAGS_T *eventgroup)
{
   int rc = -1;
   
   if (NULL != buttons)
   {
      VCOS_STATUS_T st = VCOS_SUCCESS; //buttons_dev_init(&buttons->lastcall, eventgroup);

      if (st == VCOS_SUCCESS)
         rc = 0;
   }
   
   if (0 == rc)
   {
      buttons->prodlists = prodlists;
      
      if (rc >= 0)
      {
         buttons->eds_ren = 0;
         buttons->eds_fen = 0;
      }
   }

   return rc;
}




#endif


/*****************************************************************************
 *                                                                           *
 *    Touch Server API                                                       *
 *                                                                           * 
 *****************************************************************************/






/*! Return the properties of the touch device
 */
static int /*rc*/
touch_properties(TOUCH_SERVICE_T *touchserv,
                 vc_touch_properties_t **out_properties)
{  int rpc_rc = VC_TOUCH_RC_OK;
   LOCAL_TOUCH_SERVICE_T *serv = (LOCAL_TOUCH_SERVICE_T *)touchserv;

   *out_properties = &serv->screen.prop;
   return rpc_rc;
}





/*! Return the locations of the fingers currently in contact with the  
 *  touchscreen.
 *  Up to \c max_fingers finger positions will be returned - further finger
 *  positions will be ignored.  Finger records that are returned with pressure
 *  zero are unused.  Present fingers (pressure != 0) will always be recorded
 *  earlier in the array of values than absent ones.
 */
static int /*rc*/
local_touch_event(TOUCH_SERVICE_T *touchserv, 
                  vc_touch_point_t *out_posns, int max_fingers)
{  int rpc_rc = VC_TOUCH_RC_OK;
   LOCAL_TOUCH_SERVICE_T *serv = (LOCAL_TOUCH_SERVICE_T *)touchserv;
   int i;
   
   if (max_fingers < 1)
      return VC_TOUCH_RC_EINVAL;
   else
   {  out_posns[0].x = serv->screen.cur_coord.x;
      out_posns[0].y = serv->screen.cur_coord.y;
      out_posns[0].pressure = serv->screen.cur_coord.pressure;

      for (i=1; i<max_fingers; i++)
         out_posns[i].pressure = 0;
         /* effectively mark these entries as unused */

      return rpc_rc;
   }
}


static void send_touchscreen_gesture(PLATFORM_TOUCHSCREEN_EVENT_T event,
                                     int param1)
{
   switch (event)
   {
      case PLATFORM_TOUCHSCREEN_EVENT_GESTURE_RIGHT:
         param1 = PLATFORM_BUTTON_RIGHT;
         break;
      case PLATFORM_TOUCHSCREEN_EVENT_GESTURE_LEFT:
         param1 = PLATFORM_BUTTON_LEFT;
         break;
      case PLATFORM_TOUCHSCREEN_EVENT_GESTURE_DOWN:
         param1 = PLATFORM_BUTTON_DOWN;
         break;
      case PLATFORM_TOUCHSCREEN_EVENT_GESTURE_UP:
         param1 = PLATFORM_BUTTON_UP;
         break;
      default:
         vcos_assert(0);
         param1 = PLATFORM_BUTTON_UP;
         break;
   }
   add_message(PLATFORM_MSG_BUTTON_PRESS, param1, 0);
   add_message(PLATFORM_MSG_BUTTON_RELEASE, param1, 0);
}

static int translate_gestures;
int32_t platform_translate_gestures_to_joystick(int tran)
{
   translate_gestures = tran;
   return 0;
}

static void
touch_event_gen(void *open_arg, vc_touch_event_t action, uint32_t action_arg)
{  
   VC_TOUCH_MSG_FINGER_EVENT_T evmsg;
   VCHI_SERVICE_HANDLE_T transport = (VCHI_SERVICE_HANDLE_T)open_arg; 
   uint32_t param1 = -1;
   uint32_t param2 = action_arg;
   int rc = 0;
   int /*bool*/ is_gesture = 0;

#if 0
static const char *names[] = {
   "vc_touch_event_down",
   "vc_touch_event_up",
   "vc_touch_event_press",
   "vc_touch_event_hold",
   "vc_touch_event_press_tap_tap",
   "vc_touch_event_gesture_right",
   "vc_touch_event_gesture_left",
   "vc_touch_event_gesture_up",
   "vc_touch_event_gesture_down",
};
   printf("touch_event_gen %s (%d,%d)\n", (unsigned)action < countof(names)?names[action]:"UNKNOWN", action, action_arg);
#endif
   switch(action) {
      case vc_touch_event_down:
         param1 = PLATFORM_TOUCHSCREEN_EVENT_FINGER_DOWN;
         break;
      case vc_touch_event_up:
         param1 = PLATFORM_TOUCHSCREEN_EVENT_FINGER_UP;
         break;
      case vc_touch_event_press:
         param1 = PLATFORM_TOUCHSCREEN_EVENT_PRESS;
         break;
      case vc_touch_event_hold:
         param1 = PLATFORM_TOUCHSCREEN_EVENT_PRESS_AND_HOLD;
         break;
      case vc_touch_event_press_tap_tap:
         param1 = PLATFORM_TOUCHSCREEN_EVENT_PRESS_DOUBLE_TAP;
         break;
      case vc_touch_event_gesture_right:
         param1 = PLATFORM_TOUCHSCREEN_EVENT_GESTURE_RIGHT;
         is_gesture = 1;
         break;
      case vc_touch_event_gesture_left:
         param1 = PLATFORM_TOUCHSCREEN_EVENT_GESTURE_LEFT;
         is_gesture = 1;
         break;
      case vc_touch_event_gesture_up:
         param1 = PLATFORM_TOUCHSCREEN_EVENT_GESTURE_UP;
         is_gesture = 1;
         break;
      case vc_touch_event_gesture_down:
         param1 = PLATFORM_TOUCHSCREEN_EVENT_GESTURE_DOWN;
         is_gesture = 1;
         break;
      /* no server messages for the following yet:
         param1 = PLATFORM_TOUCHSCREEN_EVENT_SPREAD_OUT;
         param1 = PLATFORM_TOUCHSCREEN_EVENT_PINCH_IN;
      */
      default:
         rc = -1; /* unknown touch event type */
         printf("button_event_gen(%d,%d) action unexpected\n", action, action_arg);
         vcos_assert(rc==0);
         break;
   }

   if (rc == 0)
   {  if (is_gesture && translate_gestures)
         send_touchscreen_gesture(param1, param2);
      else
         add_message(PLATFORM_MSG_TOUCHSCREEN_EVENT, param1, param2);
   }
}



/*! Begin generating events from the touchscreen at a rate no greater than
 *  once every \c period_ms.  The events result in the callback of the \c event
 *  function which is supplied with \c open_arg.
 *  Note that the context in which \c event is called may not be suitable for
 *  sustained processing operations.
 */
static int /*rc*/
local_touch_finger_ask_events_open(TOUCH_SERVICE_T *touchserv,
                                   VCHI_SERVICE_HANDLE_T transport,
                                   int period_ms, int max_fingers)
{  int rpc_rc = VC_TOUCH_RC_FAIL;
   LOCAL_TOUCH_SERVICE_T *serv = (LOCAL_TOUCH_SERVICE_T *)touchserv;

   if (serv == NULL)
      rpc_rc = VC_TOUCH_RC_NODEV;
   else
   {  // TODO: use the period
      if (period_ms == 0)
      {  /* asking to close the event generation service */
         if (event_deregister_touch(&serv->prodlists,
                                    &touch_event_gen, (void *)transport))
            rpc_rc = VC_TOUCH_RC_OK;
      } else
      {  /* asking to open the event generation service */
         if (event_register_touch(&serv->prodlists,
                                  &touch_event_gen, (void *)transport))
            rpc_rc = VC_TOUCH_RC_OK;
      }
   }
   return rpc_rc;
}




/*! Make an existing hardware button available for use.
 *  The \c button_id integer identifies the hardware button to use.
 */
static int /*rpc rc*/
local_button_new(TOUCH_SERVICE_T *touchserv, 
                 vc_button_handle_t *button_handle,
                 vc_button_id_t button_id,
                 vc_key_name_t key_name)
{  int rpc_rc = VC_TOUCH_RC_NOTIMPL;
   LOCAL_TOUCH_SERVICE_T *serv = (LOCAL_TOUCH_SERVICE_T *)touchserv;
   // TODO: implement me (currently a bit redundant since all keys cause
   //       events no matter which key buttons we create)
   return rpc_rc;
}






/*! Set the repeat rate for all key buttons (not touch or IR buttons)
 */
static int /*rc*/ VCHPOST_
local_buttons_repeat_rate(TOUCH_SERVICE_T *touchserv,
                          uint32_t repeat_rate_in_ms,
                          uint32_t *out_old_repeat_rate_ms)
{  int rpc_rc = VC_TOUCH_RC_OK;
   LOCAL_TOUCH_SERVICE_T *serv = (LOCAL_TOUCH_SERVICE_T *)touchserv;
   
   *out_old_repeat_rate_ms = serv->buttons.repeat_rate_ms;
   serv->buttons.repeat_rate_ms = repeat_rate_in_ms;
   /* reset the repeat timers */
   memset(&serv->buttons.button_held_time[0], 0, sizeof(buttons_held_time_t));
   
   return rpc_rc;
}





  
/*! Create a new soft button operated via Infra Red and name it
 *  with the \c button_id integer.
 *  This name will be returned by the server in order to identify the new
 *  button.
 */
static int /*rc*/
local_ir_button_new(TOUCH_SERVICE_T *touchserv, 
                    vc_button_handle_t *button_handle,
                    vc_button_id_t button_id /* other IR arguments? */)
{  int rpc_rc = VC_TOUCH_RC_NOTIMPL;
   LOCAL_TOUCH_SERVICE_T *serv = (LOCAL_TOUCH_SERVICE_T *)touchserv;
   //TODO: implement me
   return rpc_rc;
}




  
/*! Create a new soft button defined by the given coordinates and name it
 *  with the \c button_id integer.
 *  This name will be returned by the server in order to identify the new
 *  button.
 */
static int /*rc*/
local_touch_button_new(TOUCH_SERVICE_T *touchserv, 
                       vc_button_handle_t *out_button_handle,
                       vc_button_id_t button_id,
                       vc_touch_button_type_t kind,
                       int left_x, int bottom_y,
                       int width, int height)
{  int rpc_rc = VC_TOUCH_RC_OK;
   LOCAL_TOUCH_SERVICE_T *serv = (LOCAL_TOUCH_SERVICE_T *)touchserv;
   touch_button_t *button;
   touch_button_t *first_button = &serv->screen.button[0];
   touch_button_t *sentinal_button = first_button + MAX_BUTTON_COUNT;
//printf("local_touch_button_new(%d,%d)(%d,%d)\n", left_x, bottom_y, width, height);
   /* find an unused button */
   button = first_button;
   while (button->in_use && button < sentinal_button)
      button++;

   if (kind != vc_touch_button_default /* unsupported type of button */ ||
       button_id >= 32 /* button ID out of range */ ||
       width < 0 || height < 0 /* invalid value */)
   {  rpc_rc = VC_TOUCH_RC_EINVAL;
      *out_button_handle = VC_BUTTON_HANDLE_BAD;
   } else
   if (out_button_handle == NULL /* illegal location for handle */)
   {  rpc_rc = VC_TOUCH_RC_FAIL;
   } else
   if (button >= sentinal_button /* no room for additional buttons */)
   {  rpc_rc = VC_TOUCH_RC_FAIL;
      *out_button_handle = VC_BUTTON_HANDLE_BAD;
   } else
   {  button->in_use     = 1 /*TRUE*/;
      button->depressed  = 0 /*FALSE*/;
      button->button_id  = button_id;
      button->kind       = kind;
      button->bottom_y   = bottom_y;
      button->left_x     = left_x;
      button->width      = width;
      button->height     = height;
      *out_button_handle = 1 + (button - first_button); /* index in array + 1*/
   } 

   return rpc_rc;
}




/*! Remove a button from further use
 */
static int /*rc*/
local_button_delete(TOUCH_SERVICE_T *touchserv, 
                    vc_button_handle_t button_handle)
{  int rpc_rc = VC_TOUCH_RC_OK;
   LOCAL_TOUCH_SERVICE_T *serv = (LOCAL_TOUCH_SERVICE_T *)touchserv;

   if (button_handle > 0 &&
       button_handle <= MAX_BUTTON_COUNT)
   {  touch_button_t *button = &serv->screen.button[button_handle-1];
      button->in_use = 0/*FALSE*/;
   }
   
   return rpc_rc;
}




/*! Retrieve the state of all the buttons so far defined
 */
static int /*rc*/
local_buttons_get_state(TOUCH_SERVICE_T *touchserv, 
                        vc_button_state_t *out_state)
{  int rpc_rc = VC_TOUCH_RC_NOTIMPL;
   LOCAL_TOUCH_SERVICE_T *serv = (LOCAL_TOUCH_SERVICE_T *)touchserv;

   touchscreen_buttons_get_state(&serv->screen, out_state);

   return rpc_rc;
}


static void
button_event_gen(void *open_arg, vc_button_event_t action,
                 vc_button_id_t button_id, uint32_t action_arg)
{  
   static const unsigned remap[] = {PLATFORM_BUTTON_A, PLATFORM_BUTTON_B, PLATFORM_BUTTON_C, PLATFORM_BUTTON_D, PLATFORM_BUTTON_E, PLATFORM_BUTTON_DOWN, PLATFORM_BUTTON_LEFT, PLATFORM_BUTTON_UP, PLATFORM_BUTTON_RIGHT, PLATFORM_BUTTON_Z, PLATFORM_BUTTON_PAGE_UP, PLATFORM_BUTTON_PAGE_DOWN, };
   VC_TOUCH_MSG_BUTTON_EVENT_T evmsg;
   VCHI_SERVICE_HANDLE_T transport = (VCHI_SERVICE_HANDLE_T)open_arg;
   unsigned b = 0;
   //printf("button_event_gen(%d,%d,%d)\n", action, button_id, action_arg);
   if ((unsigned) button_id  < countof(remap)) b = remap[button_id]; else printf("button_event_gen(%d,%d,%d) id unexpected\n", action, button_id, action_arg);
   if (action == vc_button_event_press) add_message (PLATFORM_MSG_BUTTON_PRESS, b, 0);
   else if (action == vc_button_event_release) add_message (PLATFORM_MSG_BUTTON_RELEASE, b, 0);
   else if (action == vc_button_event_repeat) add_message (PLATFORM_MSG_BUTTON_REPEAT, b, 0);
   else printf("button_event_gen(%d,%d,%d) action unexpected\n", action, b, action_arg);
}


 
/*! Begin generating events from the buttons at a rate no greater than
 *  once every \c period_ms.  The events result in the callback of the \c event
 *  function which is supplied with \c open_arg.
 *  A new event will be generated whenever button state changes.
 *  Note that the context in which \c event is called may not be suitable for
 *  sustained processing operations.
 */
static int /*rc*/
local_buttons_ask_events_open(TOUCH_SERVICE_T *touchserv,
                              VCHI_SERVICE_HANDLE_T transport, int period_ms)
{  int rpc_rc = VC_TOUCH_RC_NOTIMPL;
   LOCAL_TOUCH_SERVICE_T *serv = (LOCAL_TOUCH_SERVICE_T *)touchserv;

   if (serv == NULL)
      rpc_rc = VC_TOUCH_RC_NODEV;
   else
   {  // TODO: use the period
      if (period_ms == 0)
      {  /* we are closing the event stream */
         if (event_deregister_button(&serv->prodlists,
                                     &button_event_gen, (void *)transport))
            rpc_rc = VC_TOUCH_RC_OK;
      } else
      {  /* we are openning the event stream */
         if (event_register_button(&serv->prodlists,
                                   &button_event_gen, (void *)transport))
            rpc_rc = VC_TOUCH_RC_OK;
      }
   }
   return rpc_rc;
}


  




/*****************************************************************************
 *                                                                           *
 *    Touch Server Task                                                      *
 *                                                                           * 
 *****************************************************************************/




static int
touchscreen_setup(TOUCHSERV_STATE_T *touchstate)
{
   #define BUTTONROW_LEN 5
   int rc = 0;
   LOCAL_TOUCH_SERVICE_T *touch = &touchstate->touch;
   TOUCH_SERVICE_T *touchserv = (TOUCH_SERVICE_T *)touch;
   touchscreen_t *touchscreen = &touch->screen;

#if CREATE_DEFAULT_BUTTONS
   vc_touch_properties_t *prop = &touchscreen->prop;
   vc_button_handle_t touchbut[BUTTONROW_LEN];
   int32_t side, but_height;
   int32_t button_row_y;

   (void)touch_properties(touchserv, &prop);

   /* add some touch buttons along the bottom
      NB: touchscreen 0,0 is top right width,height is bottom left */
   side = prop->screen_width/BUTTONROW_LEN;
   but_height = prop->screen_height/10;
   button_row_y = prop->screen_height - but_height;
   rc = local_touch_button_new(touchserv, &touchbut[0],
                               0 /*PLATFORM_BUTTON_A*/,
                               vc_touch_button_default,
                               (touchscreen->prop.rightward_x ?0:4)*side, button_row_y,
                               side, but_height);
   vcos_assert_msg(rc >= 0, "couldn't create button at (%d,%d) - rc %d",
                   (4-0)*side, button_row_y, rc);
   vcos_assert_msg(VC_BUTTON_HANDLE_BAD != touchbut[0],
                   "button 0 not created properly");
   rc = local_touch_button_new(touchserv, &touchbut[1],
                               1 /*PLATFORM_BUTTON_B*/,
                               vc_touch_button_default,
                               (touchscreen->prop.rightward_x ?1:3)*side, button_row_y,
                               side, but_height);
   vcos_assert_msg(rc >= 0, "couldn't create button at (%d,%d) - rc %d",
                   (4-1)*side, button_row_y, rc);
   vcos_assert_msg(VC_BUTTON_HANDLE_BAD != touchbut[1],
                   "button 1 not created properly");
   rc = local_touch_button_new(touchserv, &touchbut[2],
                               2 /*PLATFORM_BUTTON_C*/,
                               vc_touch_button_default,
                               (touchscreen->prop.rightward_x?2:2)*side, button_row_y,
                               side, but_height);
   vcos_assert_msg(rc >= 0, "couldn't create button at (%d,%d) - rc %d",
                   (4-2)*side, button_row_y, rc);
   vcos_assert_msg(VC_BUTTON_HANDLE_BAD != touchbut[2],
                   "button 2 not created properly");
   rc = local_touch_button_new(touchserv, &touchbut[3],
                               3 /*PLATFORM_BUTTON_D*/,
                               vc_touch_button_default,
                               (touchscreen->prop.rightward_x?3:1)*side, button_row_y,
                               side, but_height);
   vcos_assert_msg(rc >= 0, "couldn't create button at (%d,%d) - rc %d",
                   (4-3)*side, button_row_y, rc);
   vcos_assert_msg(VC_BUTTON_HANDLE_BAD != touchbut[3],
                   "button 3 not created properly");
   rc = local_touch_button_new(touchserv, &touchbut[4],
                               4 /*PLATFORM_BUTTON_E*/,
                               vc_touch_button_default,
                               (touchscreen->prop.rightward_x?4:0)*side, button_row_y,
                               side, but_height);
   vcos_assert_msg(rc >= 0, "couldn't create button at (%d,%d) - rc %d",
                   (4-4)*side, button_row_y, rc);
   vcos_assert_msg(VC_BUTTON_HANDLE_BAD != touchbut[4],
                   "button 4 not created properly");
#endif /*CREATE_DEFAULT_BUTTONS*/
   return rc;
}





/*! Main server task - listen for the next request from host and handle it
 */
static void
touchserv_task_main(unsigned argc, void *argv) {
   int32_t success;
   uint32_t message_size;
   VCOS_EVENT_FLAGS_T *eventgroup = &touchserv_state.eventgroup;
   touchserv_prodlists_t *prodlists = &touchserv_state.touch.prodlists;
   VCOS_STATUS_T rc = vcos_event_flags_create(eventgroup, "tsrv_in");
   
   touchserv_prodlists_init(prodlists);
   success = touchscreen_init(&touchserv_state.touch.screen,
                              prodlists, eventgroup);

   if (success == 0)
      success = touchscreen_setup(&touchserv_state);
      
   /* Note: if we couldn't find a touch screen we still need to service
      touch screen requests - even if it's just to let them know that
      the touch screen is absent in this build
   */
   success = 0;
   
   if (success == 0)
      success = buttons_init(&touchserv_state.touch.buttons,
                             prodlists, eventgroup);
   
#if 0
   do {
      VCOS_UNSIGNED serv_events = 0;
      vcos_event_flags_get(eventgroup, (VCOS_UNSIGNED)-1,
                           VCOS_OR_CONSUME, VCOS_SUSPEND, &serv_events);

      if (0 != (serv_events & (1 << TOUCHSRV_EVENT_MSG)))
      {  /* a message has arrived */
         success = vchi_msg_dequeue(touchserv_state.open_handle,
                                    &touchserv_state.msg,
                                    sizeof(touchserv_state.msg),
                                    &message_size,
                                    VCHI_FLAGS_BLOCK_UNTIL_OP_COMPLETE);

         if (success == 0)
            success = touchserv_message_handler(touchserv_state.open_handle,
                                                &touchserv_state.touch,
                                                (void*)&touchserv_state.msg,
                                                message_size);
      }

      if (0 != (serv_events & (1 << TOUCHSRV_EVENT_TOUCH)))
      {
         /* The touch screen FIFO can't be written to again from driver
            callback until the last event has been read - so we won't
            get this TOUCHSRV_EVENT_ unless we handle the whole FIFO */
         success = touchscreen_handle_events(&touchserv_state.touch.screen);
      }
      
      if (0 != (serv_events & (1 << TOUCHSRV_EVENT_BUTTON)))
      {
         /* We know that there is likely to be something to handle recorded
            in the buttons state */
         buttons_handle_events(&touchserv_state.touch.buttons);
      }
   } while (success == 0);

   vcos_assert_msg(success == 0, "touch server task exiting with failure %d",
                   success);

   //xxx buttons_end(&touchserv_state.touch.buttons);
   //xxx touchscreen_end(&touchserv_state.touch.screen);
   vcos_event_flags_delete(eventgroup);
#endif
}


void
platform_set_button_repeat_time (const uint32_t repeat_rate_in_ms)
{
	uint32_t *out_old_repeat_rate_ms;
	int rc = local_buttons_repeat_rate(&touchserv_state.touch, repeat_rate_in_ms, &out_old_repeat_rate_ms);
}

int32_t
platform_set_touchscreen_buttons_height (uint32_t xHeight)
{
  return 0; // FIXME
}

static int scaled_width, scaled_height;
int32_t
platform_set_touchscreen_scale (int new_scaled_width,
                                int new_scaled_height)
{
  scaled_width = new_scaled_width;
  scaled_height = new_scaled_height;
  //printf("platform_set_touchscreen_scale %d,%d\n", scaled_width, scaled_height );
  return 0; // FIXME
}

int32_t
platform_touch_finger_scaled_pos (int *out_x, int *out_y,
                                  int *out_pressure, int *out_width)
{
      if (NULL != out_x)
         *out_x = scaled_width ? (latest_touch_x * scaled_width) >> 10 : latest_touch_x;
      if (NULL != out_y)
         *out_y = scaled_height ? (latest_touch_y * scaled_height) >> 10 : latest_touch_y;
      if (NULL != out_pressure)
         *out_pressure = latest_touched;
      if (NULL != out_width)
         *out_width = 1;  /* fill in later if it ever gets implemented */

  return 0; // FIXME
}

