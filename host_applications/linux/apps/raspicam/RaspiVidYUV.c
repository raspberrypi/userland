/*
Copyright (c) 2014, DSP Group Ltd
Copyright (c) 2014, James Hughes
Copyright (c) 2013, Broadcom Europe Ltd

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

/**
 * \file RaspiVidYUV.c
 * Command line program to capture a camera video stream and save file
 * as uncompressed YUV420 data
 * Also optionally display a preview/viewfinder of current camera input.
 *
 * \date 7th Jan 2014
 * \Author: James Hughes
 *
 * Description
 *
  * 2 components are created; camera and preview.
 * Camera component has three ports, preview, video and stills.
 * Preview is connected using standard mmal connections, the video output
 * is written straight to the file in YUV 420 format via the requisite buffer
 * callback. Still port is not used
 *
 * We use the RaspiCamControl code to handle the specific camera settings.
 * We use the RaspiPreview code to handle the generic preview
*/

// We use some GNU extensions (basename)
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <sysexits.h>

#define VERSION_STRING "v1.3.12"

#include "bcm_host.h"
#include "interface/vcos/vcos.h"

#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"

#include "RaspiCamControl.h"
#include "RaspiPreview.h"
#include "RaspiCLI.h"

#include <semaphore.h>

// Standard port setting for the camera component
#define MMAL_CAMERA_PREVIEW_PORT 0
#define MMAL_CAMERA_VIDEO_PORT 1
#define MMAL_CAMERA_CAPTURE_PORT 2

// Video format information
// 0 implies variable
#define VIDEO_FRAME_RATE_NUM 30
#define VIDEO_FRAME_RATE_DEN 1

/// Video render needs at least 2 buffers.
#define VIDEO_OUTPUT_BUFFERS_NUM 3

/// Interval at which we check for an failure abort during capture
const int ABORT_INTERVAL = 100; // ms


/// Capture/Pause switch method
/// Simply capture for time specified
#define WAIT_METHOD_NONE           0
/// Cycle between capture and pause for times specified
#define WAIT_METHOD_TIMED          1
/// Switch between capture and pause on keypress
#define WAIT_METHOD_KEYPRESS       2
/// Switch between capture and pause on signal
#define WAIT_METHOD_SIGNAL         3
/// Run/record forever
#define WAIT_METHOD_FOREVER        4

int mmal_status_to_int(MMAL_STATUS_T status);
static void signal_handler(int signal_number);

// Forward
typedef struct RASPIVIDYUV_STATE_S RASPIVIDYUV_STATE;

/** Struct used to pass information in camera video port userdata to callback
 */
typedef struct
{
   FILE *file_handle;                   /// File handle to write buffer data to.
   RASPIVIDYUV_STATE *pstate;           /// pointer to our state in case required in callback
   int abort;                           /// Set to 1 in callback if an error occurs to attempt to abort the capture
} PORT_USERDATA;

/** Structure containing all state information for the current run
 */
struct RASPIVIDYUV_STATE_S
{
   int timeout;                        /// Time taken before frame is grabbed and app then shuts down. Units are milliseconds
   int width;                          /// Requested width of image
   int height;                         /// requested height of image
   int framerate;                      /// Requested frame rate (fps)
   char *filename;                     /// filename of output file
   int verbose;                        /// !0 if want detailed run information
   int demoMode;                       /// Run app in demo mode
   int demoInterval;                   /// Interval between camera settings changes
   int waitMethod;                     /// Method for switching between pause and capture

   int onTime;                         /// In timed cycle mode, the amount of time the capture is on per cycle
   int offTime;                        /// In timed cycle mode, the amount of time the capture is off per cycle

   RASPIPREVIEW_PARAMETERS preview_parameters;   /// Preview setup parameters
   RASPICAM_CAMERA_PARAMETERS camera_parameters; /// Camera setup parameters

   MMAL_COMPONENT_T *camera_component;    /// Pointer to the camera component
   MMAL_CONNECTION_T *preview_connection; /// Pointer to the connection from camera to preview

   MMAL_POOL_T *camera_pool;            /// Pointer to the pool of buffers used by camera video port

   PORT_USERDATA callback_data;         /// Used to move data to the camera callback

   int bCapturing;                      /// State of capture/pause

   int cameraNum;                       /// Camera number
   int settings;                        /// Request settings from the camera
   int sensor_mode;                     /// Sensor mode. 0=auto. Check docs/forum for modes selected by other values.
};

static XREF_T  initial_map[] =
{
   {"record",     0},
   {"pause",      1},
};

static int initial_map_size = sizeof(initial_map) / sizeof(initial_map[0]);


static void display_valid_parameters(char *app_name);

/// Command ID's and Structure defining our command line options
#define CommandHelp         0
#define CommandWidth        1
#define CommandHeight       2
#define CommandOutput       3
#define CommandVerbose      4
#define CommandTimeout      5
#define CommandDemoMode     6
#define CommandFramerate    7
#define CommandTimed        8
#define CommandSignal       9
#define CommandKeypress     10
#define CommandInitialState 11
#define CommandCamSelect    12
#define CommandSettings     13
#define CommandSensorMode   14

static COMMAND_LIST cmdline_commands[] =
{
   { CommandHelp,          "-help",       "?",  "This help information", 0 },
   { CommandWidth,         "-width",      "w",  "Set image width <size>. Default 1920", 1 },
   { CommandHeight,        "-height",     "h",  "Set image height <size>. Default 1080", 1 },
   { CommandOutput,        "-output",     "o",  "Output filename <filename> (to write to stdout, use '-o -')", 1 },
   { CommandVerbose,       "-verbose",    "v",  "Output verbose information during run", 0 },
   { CommandTimeout,       "-timeout",    "t",  "Time (in ms) to capture for. If not specified, set to 5s. Zero to disable", 1 },
   { CommandDemoMode,      "-demo",       "d",  "Run a demo mode (cycle through range of camera options, no capture)", 1},
   { CommandFramerate,     "-framerate",  "fps","Specify the frames per second to record", 1},
   { CommandTimed,         "-timed",      "td", "Cycle between capture and pause. -cycle on,off where on is record time and off is pause time in ms", 0},
   { CommandSignal,        "-signal",     "s",  "Cycle between capture and pause on Signal", 0},
   { CommandKeypress,      "-keypress",   "k",  "Cycle between capture and pause on ENTER", 0},
   { CommandInitialState,  "-initial",    "i",  "Initial state. Use 'record' or 'pause'. Default 'record'", 1},
   { CommandCamSelect,     "-camselect",  "cs", "Select camera <number>. Default 0", 1 },
   { CommandSettings,      "-settings",   "set","Retrieve camera settings and write to stdout", 0},
   { CommandSensorMode,    "-mode",       "md", "Force sensor mode. 0=auto. See docs for other modes available", 1},
};

static int cmdline_commands_size = sizeof(cmdline_commands) / sizeof(cmdline_commands[0]);


static struct
{
   char *description;
   int nextWaitMethod;
} wait_method_description[] =
{
      {"Simple capture",         WAIT_METHOD_NONE},
      {"Capture forever",        WAIT_METHOD_FOREVER},
      {"Cycle on time",          WAIT_METHOD_TIMED},
      {"Cycle on keypress",      WAIT_METHOD_KEYPRESS},
      {"Cycle on signal",        WAIT_METHOD_SIGNAL},
};

static int wait_method_description_size = sizeof(wait_method_description) / sizeof(wait_method_description[0]);



/**
 * Assign a default set of parameters to the state passed in
 *
 * @param state Pointer to state structure to assign defaults to
 */
static void default_status(RASPIVIDYUV_STATE *state)
{
   if (!state)
   {
      vcos_assert(0);
      return;
   }

   // Default everything to zero
   memset(state, 0, sizeof(RASPIVIDYUV_STATE));

   // Now set anything non-zero
   state->timeout = 5000;     // 5s delay before take image
   state->width = 1920;       // Default to 1080p
   state->height = 1080;
   state->framerate = VIDEO_FRAME_RATE_NUM;
   state->demoMode = 0;
   state->demoInterval = 250; // ms
   state->waitMethod = WAIT_METHOD_NONE;
   state->onTime = 5000;
   state->offTime = 5000;

   state->bCapturing = 0;

   state->cameraNum = 0;
   state->settings = 0;
   state->sensor_mode = 0;

   // Setup preview window defaults
   raspipreview_set_defaults(&state->preview_parameters);

   // Set up the camera_parameters to default
   raspicamcontrol_set_defaults(&state->camera_parameters);
}


/**
 * Dump image state parameters to stderr.
 *
 * @param state Pointer to state structure to assign defaults to
 */
static void dump_status(RASPIVIDYUV_STATE *state)
{
   int i, size, ystride, yheight;

   if (!state)
   {
      vcos_assert(0);
      return;
   }

   fprintf(stderr, "Width %d, Height %d, filename %s\n", state->width, state->height, state->filename);
   fprintf(stderr, "framerate %d, time delay %d\n", state->framerate, state->timeout);

   // Calculate the individual image size
   // Y stride rounded to multiple of 32. U&V stride is Y stride/2 (ie multiple of 16).
   // Y height is padded to a 16. U/V height is Y height/2 (ie multiple of 8).

   // Y plane
   ystride = ((state->width + 31) & ~31);
   yheight = ((state->height + 15) & ~15);

   size = ystride * yheight;

   // U and V plane
   size += 2 * ystride/2 * yheight/2;

   fprintf(stderr, "Sub-image size %d bytes in total.\n  Y pitch %d, Y height %d, UV pitch %d, UV Height %d\n", size, ystride, yheight, ystride/2,yheight/2);

   fprintf(stderr, "Wait method : ");
   for (i=0;i<wait_method_description_size;i++)
   {
      if (state->waitMethod == wait_method_description[i].nextWaitMethod)
         fprintf(stderr, "%s", wait_method_description[i].description);
   }
   fprintf(stderr, "\nInitial state '%s'\n", raspicli_unmap_xref(state->bCapturing, initial_map, initial_map_size));
   fprintf(stderr, "\n\n");

   raspipreview_dump_parameters(&state->preview_parameters);
   raspicamcontrol_dump_parameters(&state->camera_parameters);
}

/**
 * Parse the incoming command line and put resulting parameters in to the state
 *
 * @param argc Number of arguments in command line
 * @param argv Array of pointers to strings from command line
 * @param state Pointer to state structure to assign any discovered parameters to
 * @return Non-0 if failed for some reason, 0 otherwise
 */
static int parse_cmdline(int argc, const char **argv, RASPIVIDYUV_STATE *state)
{
   // Parse the command line arguments.
   // We are looking for --<something> or -<abreviation of something>

   int valid = 1;
   int i;

   for (i = 1; i < argc && valid; i++)
   {
      int command_id, num_parameters;

      if (!argv[i])
         continue;

      if (argv[i][0] != '-')
      {
         valid = 0;
         continue;
      }

      // Assume parameter is valid until proven otherwise
      valid = 1;

      command_id = raspicli_get_command_id(cmdline_commands, cmdline_commands_size, &argv[i][1], &num_parameters);

      // If we found a command but are missing a parameter, continue (and we will drop out of the loop)
      if (command_id != -1 && num_parameters > 0 && (i + 1 >= argc) )
         continue;

      //  We are now dealing with a command line option
      switch (command_id)
      {
      case CommandHelp:
         display_valid_parameters(basename(argv[0]));
         return -1;

      case CommandWidth: // Width > 0
         if (sscanf(argv[i + 1], "%u", &state->width) != 1)
            valid = 0;
         else
            i++;
         break;

      case CommandHeight: // Height > 0
         if (sscanf(argv[i + 1], "%u", &state->height) != 1)
            valid = 0;
         else
            i++;
         break;

      case CommandOutput:  // output filename
      {
         int len = strlen(argv[i + 1]);
         if (len)
         {
            state->filename = malloc(len + 1);
            vcos_assert(state->filename);
            if (state->filename)
               strncpy(state->filename, argv[i + 1], len+1);
            i++;
         }
         else
            valid = 0;
         break;
      }

      case CommandVerbose: // display lots of data during run
         state->verbose = 1;
         break;

      case CommandTimeout: // Time to run viewfinder/capture
      {
         if (sscanf(argv[i + 1], "%u", &state->timeout) == 1)
         {
            // Ensure that if previously selected a waitMethod we dont overwrite it
            if (state->timeout == 0 && state->waitMethod == WAIT_METHOD_NONE)
               state->waitMethod = WAIT_METHOD_FOREVER;

            i++;
         }
         else
            valid = 0;
         break;
      }

      case CommandDemoMode: // Run in demo mode - no capture
      {
         // Demo mode might have a timing parameter
         // so check if a) we have another parameter, b) its not the start of the next option
         if (i + 1 < argc  && argv[i+1][0] != '-')
         {
            if (sscanf(argv[i + 1], "%u", &state->demoInterval) == 1)
            {
               // TODO : What limits do we need for timeout?
               if (state->demoInterval == 0)
                  state->demoInterval = 250; // ms

               state->demoMode = 1;
               i++;
            }
            else
               valid = 0;
         }
         else
         {
            state->demoMode = 1;
         }

         break;
      }

      case CommandFramerate: // fps to record
      {
         if (sscanf(argv[i + 1], "%u", &state->framerate) == 1)
         {
            // TODO : What limits do we need for fps 1 - 30 - 120??
            i++;
         }
         else
            valid = 0;
         break;
      }

      case CommandTimed:
      {
         if (sscanf(argv[i + 1], "%u,%u", &state->onTime, &state->offTime) == 2)
         {
            i++;

            if (state->onTime < 1000)
               state->onTime = 1000;

            if (state->offTime < 1000)
               state->offTime = 1000;

            state->waitMethod = WAIT_METHOD_TIMED;
         }
         else
            valid = 0;
         break;
      }

      case CommandKeypress:
         state->waitMethod = WAIT_METHOD_KEYPRESS;
         break;

      case CommandSignal:
         state->waitMethod = WAIT_METHOD_SIGNAL;
         // Reenable the signal
         signal(SIGUSR1, signal_handler);
         break;

      case CommandInitialState:
      {
         state->bCapturing = raspicli_map_xref(argv[i + 1], initial_map, initial_map_size);

         if( state->bCapturing == -1)
            state->bCapturing = 0;

         i++;
         break;
      }

      case CommandCamSelect:  //Select camera input port
      {
         if (sscanf(argv[i + 1], "%u", &state->cameraNum) == 1)
         {
            i++;
         }
         else
            valid = 0;
         break;
      }

      case CommandSettings:
         state->settings = 1;
         break;

      case CommandSensorMode:
      {
         if (sscanf(argv[i + 1], "%u", &state->sensor_mode) == 1)
         {
            i++;
         }
         else
            valid = 0;
         break;
      }

      default:
      {
         // Try parsing for any image specific parameters
         // result indicates how many parameters were used up, 0,1,2
         // but we adjust by -1 as we have used one already
         const char *second_arg = (i + 1 < argc) ? argv[i + 1] : NULL;
         int parms_used = (raspicamcontrol_parse_cmdline(&state->camera_parameters, &argv[i][1], second_arg));

         // Still unused, try preview options
         if (!parms_used)
            parms_used = raspipreview_parse_cmdline(&state->preview_parameters, &argv[i][1], second_arg);


         // If no parms were used, this must be a bad parameters
         if (!parms_used)
            valid = 0;
         else
            i += parms_used - 1;

         break;
      }
      }
   }

   if (!valid)
   {
      fprintf(stderr, "Invalid command line option (%s)\n", argv[i-1]);
      return 1;
   }

   // Always disable verbose if output going to stdout
   if (state->filename && state->filename[0] == '-')
   {
      state->verbose = 0;
   }

   return 0;
}

/**
 * Display usage information for the application to stdout
 *
 * @param app_name String to display as the application name
 */
static void display_valid_parameters(char *app_name)
{
   fprintf(stdout, "Display camera output to display, and optionally saves an uncompressed YUV420 file \n\n");
   fprintf(stdout, "NOTE: High resolutions and/or frame rates may exceed the bandwidth of the system due\n");
   fprintf(stdout, "to the large amounts of data being moved to the SD card. This will result in undefined\n");
   fprintf(stdout, "results in the subsequent file.\n");
   fprintf(stdout, "The raw file produced contains all the files. Each image in the files will be of size\n");
   fprintf(stdout, "width*height*1.5, unless width and/or height are not divisible by 16. Use the image size\n");
   fprintf(stdout, "displayed during the run (in verbose mode) for an accurate value\n");

   fprintf(stdout, "The Linux split command can be used to split up the file to individual frames\n");

   fprintf(stdout, "\nusage: %s [options]\n\n", app_name);

   fprintf(stdout, "Image parameter commands\n\n");

   raspicli_display_help(cmdline_commands, cmdline_commands_size);

   fprintf(stdout, "\n");

   // Help for preview options
   raspipreview_display_help();

   // Now display any help information from the camcontrol code
   raspicamcontrol_display_help();

   fprintf(stdout, "\n");

   return;
}

/**
 *  buffer header callback function for camera control
 *
 *  Callback will dump buffer data to the specific file
 *
 * @param port Pointer to port from which callback originated
 * @param buffer mmal buffer header pointer
 */
static void camera_control_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   if (buffer->cmd == MMAL_EVENT_PARAMETER_CHANGED)
   {
      MMAL_EVENT_PARAMETER_CHANGED_T *param = (MMAL_EVENT_PARAMETER_CHANGED_T *)buffer->data;
      switch (param->hdr.id) {
         case MMAL_PARAMETER_CAMERA_SETTINGS:
         {
            MMAL_PARAMETER_CAMERA_SETTINGS_T *settings = (MMAL_PARAMETER_CAMERA_SETTINGS_T*)param;
            vcos_log_error("Exposure now %u, analog gain %u/%u, digital gain %u/%u",
			settings->exposure,
                        settings->analog_gain.num, settings->analog_gain.den,
                        settings->digital_gain.num, settings->digital_gain.den);
            vcos_log_error("AWB R=%u/%u, B=%u/%u",
                        settings->awb_red_gain.num, settings->awb_red_gain.den,
                        settings->awb_blue_gain.num, settings->awb_blue_gain.den
                        );
         }
         break;
      }
   }
   else if (buffer->cmd == MMAL_EVENT_ERROR)
   {
      vcos_log_error("No data received from sensor. Check all connections, including the Sunny one on the camera board");
   }
   else
   {
      vcos_log_error("Received unexpected camera control callback event, 0x%08x", buffer->cmd);
   }

   mmal_buffer_header_release(buffer);
}


/**
 * Open a file based on the settings in state
 *
 * @param state Pointer to state
 */
static FILE *open_filename(RASPIVIDYUV_STATE *pState)
{
   FILE *new_handle = NULL;
   char *tempname = NULL, *filename = NULL;

   filename = pState->filename;

   if (filename)
      new_handle = fopen(filename, "wb");

   if (pState->verbose)
   {
      if (new_handle)
         fprintf(stderr, "Opening output file \"%s\"\n", filename);
      else
         fprintf(stderr, "Failed to open new file \"%s\"\n", filename);
   }

   if (tempname)
      free(tempname);

   return new_handle;
}


/**
 *  buffer header callback function for camera
 *
 *  Callback will dump buffer data to internal buffer
 *
 * @param port Pointer to port from which callback originated
 * @param buffer mmal buffer header pointer
 */
static void camera_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   MMAL_BUFFER_HEADER_T *new_buffer;
   static int64_t base_time =  -1;

   // All our times based on the receipt of the first callback
   if (base_time == -1)
      base_time = vcos_getmicrosecs64()/1000;

   // We pass our file handle and other stuff in via the userdata field.

   PORT_USERDATA *pData = (PORT_USERDATA *)port->userdata;

   if (pData)
   {
      int bytes_written = buffer->length;

      vcos_assert(pData->file_handle);

      if (buffer->length)
      {
         mmal_buffer_header_mem_lock(buffer);
         bytes_written = fwrite(buffer->data, 1, buffer->length, pData->file_handle);
         mmal_buffer_header_mem_unlock(buffer);

         if (bytes_written != buffer->length)
         {
            vcos_log_error("Failed to write buffer data (%d from %d)- aborting", bytes_written, buffer->length);
            pData->abort = 1;
         }
      }
   }
   else
   {
      vcos_log_error("Received a camera buffer callback with no state");
   }

   // release buffer back to the pool
   mmal_buffer_header_release(buffer);

   // and send one back to the port (if still open)
   if (port->is_enabled)
   {
      MMAL_STATUS_T status;

      new_buffer = mmal_queue_get(pData->pstate->camera_pool->queue);

      if (new_buffer)
         status = mmal_port_send_buffer(port, new_buffer);

      if (!new_buffer || status != MMAL_SUCCESS)
         vcos_log_error("Unable to return a buffer to the camera port");
   }
}


/**
 * Create the camera component, set up its ports
 *
 * @param state Pointer to state control struct
 *
 * @return MMAL_SUCCESS if all OK, something else otherwise
 *
 */
static MMAL_STATUS_T create_camera_component(RASPIVIDYUV_STATE *state)
{
   MMAL_COMPONENT_T *camera = 0;
   MMAL_ES_FORMAT_T *format;
   MMAL_PORT_T *preview_port = NULL, *video_port = NULL, *still_port = NULL;
   MMAL_STATUS_T status;
   MMAL_POOL_T *pool;

   /* Create the component */
   status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Failed to create camera component");
      goto error;
   }

   MMAL_PARAMETER_INT32_T camera_num =
      {{MMAL_PARAMETER_CAMERA_NUM, sizeof(camera_num)}, state->cameraNum};

   status = mmal_port_parameter_set(camera->control, &camera_num.hdr);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Could not select camera : error %d", status);
      goto error;
   }

   if (!camera->output_num)
   {
      status = MMAL_ENOSYS;
      vcos_log_error("Camera doesn't have output ports");
      goto error;
   }

   status = mmal_port_parameter_set_uint32(camera->control, MMAL_PARAMETER_CAMERA_CUSTOM_SENSOR_CONFIG, state->sensor_mode);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Could not set sensor mode : error %d", status);
      goto error;
   }

   preview_port = camera->output[MMAL_CAMERA_PREVIEW_PORT];
   video_port = camera->output[MMAL_CAMERA_VIDEO_PORT];
   still_port = camera->output[MMAL_CAMERA_CAPTURE_PORT];

   if (state->settings)
   {
      MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T change_event_request =
         {{MMAL_PARAMETER_CHANGE_EVENT_REQUEST, sizeof(MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T)},
          MMAL_PARAMETER_CAMERA_SETTINGS, 1};

      status = mmal_port_parameter_set(camera->control, &change_event_request.hdr);
      if ( status != MMAL_SUCCESS )
      {
         vcos_log_error("No camera settings events");
      }
   }

   // Enable the camera, and tell it its control callback function
   status = mmal_port_enable(camera->control, camera_control_callback);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("Unable to enable control port : error %d", status);
      goto error;
   }

   //  set up the camera configuration
   {
      MMAL_PARAMETER_CAMERA_CONFIG_T cam_config =
      {
         { MMAL_PARAMETER_CAMERA_CONFIG, sizeof(cam_config) },
         .max_stills_w = state->width,
         .max_stills_h = state->height,
         .stills_yuv422 = 0,
         .one_shot_stills = 0,
         .max_preview_video_w = state->width,
         .max_preview_video_h = state->height,
         .num_preview_video_frames = 3,
         .stills_capture_circular_buffer_height = 0,
         .fast_preview_resume = 0,
         .use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC
      };
      mmal_port_parameter_set(camera->control, &cam_config.hdr);
   }

   // Now set up the port formats

   // Set the encode format on the Preview port
   // HW limitations mean we need the preview to be the same size as the required recorded output

   format = preview_port->format;

   format->encoding = MMAL_ENCODING_OPAQUE;
   format->encoding_variant = MMAL_ENCODING_I420;

   if(state->camera_parameters.shutter_speed > 6000000)
   {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
                                                     { 50, 1000 }, {166, 1000}};
        mmal_port_parameter_set(preview_port, &fps_range.hdr);
   }
   else if(state->camera_parameters.shutter_speed > 1000000)
   {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
                                                     { 166, 1000 }, {999, 1000}};
        mmal_port_parameter_set(preview_port, &fps_range.hdr);
   }

   //enable dynamic framerate if necessary
   if (state->camera_parameters.shutter_speed)
   {
      if (state->framerate > 1000000./state->camera_parameters.shutter_speed)
      {
         state->framerate=0;
         if (state->verbose)
            fprintf(stderr, "Enable dynamic frame rate to fulfil shutter speed requirement\n");
      }
   }

   format->encoding = MMAL_ENCODING_OPAQUE;
   format->es->video.width = VCOS_ALIGN_UP(state->width, 32);
   format->es->video.height = VCOS_ALIGN_UP(state->height, 16);
   format->es->video.crop.x = 0;
   format->es->video.crop.y = 0;
   format->es->video.crop.width = state->width;
   format->es->video.crop.height = state->height;
   format->es->video.frame_rate.num = PREVIEW_FRAME_RATE_NUM;
   format->es->video.frame_rate.den = PREVIEW_FRAME_RATE_DEN;

   status = mmal_port_format_commit(preview_port);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("camera viewfinder format couldn't be set");
      goto error;
   }

   // Set the encode format on the video  port

   format = video_port->format;
   format->encoding_variant = MMAL_ENCODING_I420;

   if(state->camera_parameters.shutter_speed > 6000000)
   {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
                                                     { 50, 1000 }, {166, 1000}};
        mmal_port_parameter_set(video_port, &fps_range.hdr);
   }
   else if(state->camera_parameters.shutter_speed > 1000000)
   {
        MMAL_PARAMETER_FPS_RANGE_T fps_range = {{MMAL_PARAMETER_FPS_RANGE, sizeof(fps_range)},
                                                     { 167, 1000 }, {999, 1000}};
        mmal_port_parameter_set(video_port, &fps_range.hdr);
   }

   format->encoding = MMAL_ENCODING_I420;
   format->encoding_variant = MMAL_ENCODING_I420;

   format->es->video.width = VCOS_ALIGN_UP(state->width, 32);
   format->es->video.height = VCOS_ALIGN_UP(state->height, 16);
   format->es->video.crop.x = 0;
   format->es->video.crop.y = 0;
   format->es->video.crop.width = state->width;
   format->es->video.crop.height = state->height;
   format->es->video.frame_rate.num = state->framerate;
   format->es->video.frame_rate.den = VIDEO_FRAME_RATE_DEN;

   status = mmal_port_format_commit(video_port);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("camera video format couldn't be set");
      goto error;
   }

   // Ensure there are enough buffers to avoid dropping frames
   if (video_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
      video_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;

   // Set the encode format on the still  port

   format = still_port->format;

   format->encoding = MMAL_ENCODING_OPAQUE;
   format->encoding_variant = MMAL_ENCODING_I420;

   format->es->video.width = VCOS_ALIGN_UP(state->width, 32);
   format->es->video.height = VCOS_ALIGN_UP(state->height, 16);
   format->es->video.crop.x = 0;
   format->es->video.crop.y = 0;
   format->es->video.crop.width = state->width;
   format->es->video.crop.height = state->height;
   format->es->video.frame_rate.num = 0;
   format->es->video.frame_rate.den = 1;

   status = mmal_port_format_commit(still_port);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("camera still format couldn't be set");
      goto error;
   }

   /* Ensure there are enough buffers to avoid dropping frames */
   if (still_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
      still_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;

   /* Enable component */
   status = mmal_component_enable(camera);

   if (status != MMAL_SUCCESS)
   {
      vcos_log_error("camera component couldn't be enabled");
      goto error;
   }

   raspicamcontrol_set_all_parameters(camera, &state->camera_parameters);

   /* Create pool of buffer headers for the output port to consume */
   pool = mmal_port_pool_create(video_port, video_port->buffer_num, video_port->buffer_size);

   if (!pool)
   {
      vcos_log_error("Failed to create buffer header pool for camera still port %s", still_port->name);
   }

   state->camera_pool = pool;
   state->camera_component = camera;

   if (state->verbose)
      fprintf(stderr, "Camera component done\n");

   return status;

error:

   if (camera)
      mmal_component_destroy(camera);

   return status;
}

/**
 * Destroy the camera component
 *
 * @param state Pointer to state control struct
 *
 */
static void destroy_camera_component(RASPIVIDYUV_STATE *state)
{
   if (state->camera_component)
   {
      mmal_component_destroy(state->camera_component);
      state->camera_component = NULL;
   }
}


/**
 * Connect two specific ports together
 *
 * @param output_port Pointer the output port
 * @param input_port Pointer the input port
 * @param Pointer to a mmal connection pointer, reassigned if function successful
 * @return Returns a MMAL_STATUS_T giving result of operation
 *
 */
static MMAL_STATUS_T connect_ports(MMAL_PORT_T *output_port, MMAL_PORT_T *input_port, MMAL_CONNECTION_T **connection)
{
   MMAL_STATUS_T status;

   status =  mmal_connection_create(connection, output_port, input_port, MMAL_CONNECTION_FLAG_TUNNELLING | MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT);

   if (status == MMAL_SUCCESS)
   {
      status =  mmal_connection_enable(*connection);
      if (status != MMAL_SUCCESS)
         mmal_connection_destroy(*connection);
   }

   return status;
}

/**
 * Checks if specified port is valid and enabled, then disables it
 *
 * @param port  Pointer the port
 *
 */
static void check_disable_port(MMAL_PORT_T *port)
{
   if (port && port->is_enabled)
      mmal_port_disable(port);
}

/**
 * Handler for sigint signals
 *
 * @param signal_number ID of incoming signal.
 *
 */
static void signal_handler(int signal_number)
{
   if (signal_number == SIGUSR1)
   {
      // Handle but ignore - prevents us dropping out if started in none-signal mode
      // and someone sends us the USR1 signal anyway
   }
   else
   {
      // Going to abort on all other signals
      vcos_log_error("Aborting program\n");
      exit(130);
   }

}

/**
 * Pause for specified time, but return early if detect an abort request
 *
 * @param state Pointer to state control struct
 * @param pause Time in ms to pause
 * @param callback Struct contain an abort flag tested for early termination
 *
 */
static int pause_and_test_abort(RASPIVIDYUV_STATE *state, int pause)
{
   int wait;

   if (!pause)
      return 0;

   // Going to check every ABORT_INTERVAL milliseconds
   for (wait = 0; wait < pause; wait+= ABORT_INTERVAL)
   {
      vcos_sleep(ABORT_INTERVAL);
      if (state->callback_data.abort)
         return 1;
   }

   return 0;
}


/**
 * Function to wait in various ways (depending on settings)
 *
 * @param state Pointer to the state data
 *
 * @return !0 if to continue, 0 if reached end of run
 */
static int wait_for_next_change(RASPIVIDYUV_STATE *state)
{
   int keep_running = 1;
   static int64_t complete_time = -1;

   // Have we actually exceeded our timeout?
   int64_t current_time =  vcos_getmicrosecs64()/1000;

   if (complete_time == -1)
      complete_time =  current_time + state->timeout;

   // if we have run out of time, flag we need to exit
   if (current_time >= complete_time && state->timeout != 0)
      keep_running = 0;

   switch (state->waitMethod)
   {
   case WAIT_METHOD_NONE:
      (void)pause_and_test_abort(state, state->timeout);
      return 0;

   case WAIT_METHOD_FOREVER:
   {
      // We never return from this. Expect a ctrl-c to exit.
      while (1)
         // Have a sleep so we don't hog the CPU.
         vcos_sleep(10000);

      return 0;
   }

   case WAIT_METHOD_TIMED:
   {
      int abort;

      if (state->bCapturing)
         abort = pause_and_test_abort(state, state->onTime);
      else
         abort = pause_and_test_abort(state, state->offTime);

      if (abort)
         return 0;
      else
         return keep_running;
   }

   case WAIT_METHOD_KEYPRESS:
   {
      char ch;

      if (state->verbose)
         fprintf(stderr, "Press Enter to %s, X then ENTER to exit\n", state->bCapturing ? "pause" : "capture");

      ch = getchar();
      if (ch == 'x' || ch == 'X')
         return 0;
      else
         return keep_running;
   }

   case WAIT_METHOD_SIGNAL:
   {
      // Need to wait for a SIGUSR1 signal
      sigset_t waitset;
      int sig;
      int result = 0;

      sigemptyset( &waitset );
      sigaddset( &waitset, SIGUSR1 );

      // We are multi threaded because we use mmal, so need to use the pthread
      // variant of procmask to block SIGUSR1 so we can wait on it.
      pthread_sigmask( SIG_BLOCK, &waitset, NULL );

      if (state->verbose)
      {
         fprintf(stderr, "Waiting for SIGUSR1 to %s\n", state->bCapturing ? "pause" : "capture");
      }

      result = sigwait( &waitset, &sig );

      if (state->verbose && result != 0)
         fprintf(stderr, "Bad signal received - error %d\n", errno);

      return keep_running;
   }

   } // switch

   return keep_running;
}

/**
 * main
 */
int main(int argc, const char **argv)
{
   // Our main data storage vessel..
   RASPIVIDYUV_STATE state;
   int exit_code = EX_OK;

   MMAL_STATUS_T status = MMAL_SUCCESS;
   MMAL_PORT_T *camera_preview_port = NULL;
   MMAL_PORT_T *camera_video_port = NULL;
   MMAL_PORT_T *camera_still_port = NULL;
   MMAL_PORT_T *preview_input_port = NULL;

   bcm_host_init();

   // Register our application with the logging system
   vcos_log_register("RaspiVid", VCOS_LOG_CATEGORY);

   signal(SIGINT, signal_handler);

   // Disable USR1 for the moment - may be reenabled if go in to signal capture mode
   signal(SIGUSR1, SIG_IGN);

   default_status(&state);

   // Do we have any parameters
   if (argc == 1)
   {
      fprintf(stdout, "\n%s Camera App %s\n\n", basename(argv[0]), VERSION_STRING);

      display_valid_parameters(basename(argv[0]));
      exit(EX_USAGE);
   }

   // Parse the command line and put options in to our status structure
   if (parse_cmdline(argc, argv, &state))
   {
      status = -1;
      exit(EX_USAGE);
   }

   if (state.verbose)
   {
      fprintf(stderr, "\n%s Camera App %s\n\n", basename(argv[0]), VERSION_STRING);
      dump_status(&state);
   }

   // OK, we have a nice set of parameters. Now set up our components
   // We have two components. Camera, Preview

   if ((status = create_camera_component(&state)) != MMAL_SUCCESS)
   {
      vcos_log_error("%s: Failed to create camera component", __func__);
      exit_code = EX_SOFTWARE;
   }
   else if ((status = raspipreview_create(&state.preview_parameters)) != MMAL_SUCCESS)
   {
      vcos_log_error("%s: Failed to create preview component", __func__);
      destroy_camera_component(&state);
      exit_code = EX_SOFTWARE;
   }
   else
   {
      if (state.verbose)
         fprintf(stderr, "Starting component connection stage\n");

      camera_preview_port = state.camera_component->output[MMAL_CAMERA_PREVIEW_PORT];
      camera_video_port   = state.camera_component->output[MMAL_CAMERA_VIDEO_PORT];
      camera_still_port   = state.camera_component->output[MMAL_CAMERA_CAPTURE_PORT];
      preview_input_port  = state.preview_parameters.preview_component->input[0];

      if (state.preview_parameters.wantPreview )
      {
         if (state.verbose)
         {
            fprintf(stderr, "Connecting camera preview port to preview input port\n");
            fprintf(stderr, "Starting video preview\n");
         }

         // Connect camera to preview
         status = connect_ports(camera_preview_port, preview_input_port, &state.preview_connection);

         if (status != MMAL_SUCCESS)
            state.preview_connection = NULL;
      }
      else
      {
         status = MMAL_SUCCESS;
      }

      if (status == MMAL_SUCCESS)
      {
         state.callback_data.file_handle = NULL;

         if (state.filename)
         {
            if (state.filename[0] == '-')
            {
               state.callback_data.file_handle = stdout;

               // Ensure we don't upset the output stream with diagnostics/info
               state.verbose = 0;
            }
            else
            {
               state.callback_data.file_handle = open_filename(&state);
            }

            if (!state.callback_data.file_handle)
            {
               // Notify user, carry on but discarding output buffers
               vcos_log_error("%s: Error opening output file: %s\nNo output file will be generated\n", __func__, state.filename);
            }
         }

         // Set up our userdata - this is passed though to the callback where we need the information.
         state.callback_data.pstate = &state;
         state.callback_data.abort = 0;

         camera_video_port->userdata = (struct MMAL_PORT_USERDATA_T *)&state.callback_data;

         if (state.verbose)
            fprintf(stderr, "Enabling camera video port\n");

         // Enable the camera video port and tell it its callback function
         status = mmal_port_enable(camera_video_port, camera_buffer_callback);

         if (status != MMAL_SUCCESS)
         {
            vcos_log_error("Failed to setup camera output");
            goto error;
         }

         if (state.demoMode)
         {
            // Run for the user specific time..
            int num_iterations = state.timeout / state.demoInterval;
            int i;

            if (state.verbose)
               fprintf(stderr, "Running in demo mode\n");

            for (i=0;state.timeout == 0 || i<num_iterations;i++)
            {
               raspicamcontrol_cycle_test(state.camera_component);
               vcos_sleep(state.demoInterval);
            }
         }
         else
         {
            // Only save stuff if we have a filename and it opened
            // Note we use the file handle copy in the callback, as the call back MIGHT change the file handle
            if (state.callback_data.file_handle)
            {
               int running = 1;

               // Send all the buffers to the camera video port
               {
                  int num = mmal_queue_length(state.camera_pool->queue);
                  int q;
                  for (q=0;q<num;q++)
                  {
                     MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(state.camera_pool->queue);

                     if (!buffer)
                        vcos_log_error("Unable to get a required buffer %d from pool queue", q);

                     if (mmal_port_send_buffer(camera_video_port, buffer)!= MMAL_SUCCESS)
                        vcos_log_error("Unable to send a buffer to camera video port (%d)", q);
                  }
               }

               while (running)
               {
                  // Change state

                  state.bCapturing = !state.bCapturing;

                  if (mmal_port_parameter_set_boolean(camera_video_port, MMAL_PARAMETER_CAPTURE, state.bCapturing) != MMAL_SUCCESS)
                  {
                     // How to handle?
                  }

                  if (state.verbose)
                  {
                     if (state.bCapturing)
                        fprintf(stderr, "Starting video capture\n");
                     else
                        fprintf(stderr, "Pausing video capture\n");
                  }

                  running = wait_for_next_change(&state);
               }

               if (state.verbose)
                  fprintf(stderr, "Finished capture\n");
            }
            else
            {
               if (state.timeout)
                  vcos_sleep(state.timeout);
               else
               {
                  // timeout = 0 so run forever
                  while(1)
                     vcos_sleep(ABORT_INTERVAL);
               }
            }
         }
      }
      else
      {
         mmal_status_to_int(status);
         vcos_log_error("%s: Failed to connect camera to preview", __func__);
      }

error:

      mmal_status_to_int(status);

      if (state.verbose)
         fprintf(stderr, "Closing down\n");

      // Disable all our ports that are not handled by connections
      check_disable_port(camera_still_port);

      if (state.preview_parameters.wantPreview && state.preview_connection)
         mmal_connection_destroy(state.preview_connection);

      if (state.preview_parameters.preview_component)
         mmal_component_disable(state.preview_parameters.preview_component);

      if (state.camera_component)
         mmal_component_disable(state.camera_component);

      // Can now close our file. Note disabling ports may flush buffers which causes
      // problems if we have already closed the file!
      if (state.callback_data.file_handle && state.callback_data.file_handle != stdout)
         fclose(state.callback_data.file_handle);

      raspipreview_destroy(&state.preview_parameters);
      destroy_camera_component(&state);

      if (state.verbose)
         fprintf(stderr, "Close down completed, all components disconnected, disabled and destroyed\n\n");
   }

   if (status != MMAL_SUCCESS)
      raspicamcontrol_check_configuration(128);

   return exit_code;
}


