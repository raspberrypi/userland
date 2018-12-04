/*
<<<<<<< HEAD
Copyright (c) 2018, Raspberry Pi (Trading) Ltd.
=======
Copyright (c) 2018, Raspberry Pi (Trading) Ltd
>>>>>>> Moved common settings to own file
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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <unistd.h>
#include <stdint.h>


#include "interface/vcos/vcos.h"
#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/mmal_parameters_camera.h"

#include "RaspiCLI.h"
#include "RaspiPreview.h"
#include "RaspiCamControl.h"
#include "RaspiCommonSettings.h"

static const char *app_name;

<<<<<<< HEAD
=======
void __attribute__((weak)) application_help_message(char *name)
{
   fprintf(stdout, "No application help function for %s has been defined\n", name);
}



>>>>>>> Moved common settings to own file
void display_valid_parameters(char *name, void (*app_help)(char*))
{
   fprintf(stdout, "\n%s Camera App\n\n", basename(app_name));

   // This should be defined in the main app source code
   if (app_help)
      (*app_help)(name);

   // general settings
   raspicommonsettings_display_help();

   // Help for preview options
   raspipreview_display_help();

   // Now display any help information from the camcontrol code
   raspicamcontrol_display_help();

   fprintf(stdout, "\n");
}


void get_sensor_defaults(int camera_num, char *camera_name, int *width, int *height )
{
   MMAL_COMPONENT_T *camera_info;
   MMAL_STATUS_T status;

   // Default to the OV5647 setup
   strncpy(camera_name, "OV5647", MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN);

   // Try to get the camera name and maximum supported resolution
   status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA_INFO, &camera_info);
   if (status == MMAL_SUCCESS)
   {
      MMAL_PARAMETER_CAMERA_INFO_T param;
      param.hdr.id = MMAL_PARAMETER_CAMERA_INFO;
      param.hdr.size = sizeof(param)-4;  // Deliberately undersize to check firmware version
      status = mmal_port_parameter_get(camera_info->control, &param.hdr);

      if (status != MMAL_SUCCESS)
      {
         // Running on newer firmware
         param.hdr.size = sizeof(param);
         status = mmal_port_parameter_get(camera_info->control, &param.hdr);
         if (status == MMAL_SUCCESS && param.num_cameras > camera_num)
         {
            // Take the parameters from the first camera listed.
            if (*width == 0)
               *width = param.cameras[camera_num].max_width;
            if (*height == 0)
               *height = param.cameras[camera_num].max_height;
            strncpy(camera_name, param.cameras[camera_num].camera_name, MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN);
            camera_name[MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN-1] = 0;
         }
         else
            vcos_log_error("Cannot read camera info, keeping the defaults for OV5647");
      }
      else
      {
         // Older firmware
         // Nothing to do here, keep the defaults for OV5647
      }

      mmal_component_destroy(camera_info);
   }
   else
   {
      vcos_log_error("Failed to create camera_info component");
   }

   // default to OV5647 if nothing detected..
   if (*width == 0)
      *width = 2592;
   if (*height == 0)
      *height = 1944;
}

void set_app_name(const char *name)
{
   app_name = name;
}

const char *get_app_name()
{
   return app_name;
}

