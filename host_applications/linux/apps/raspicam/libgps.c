/*
Copyright (c) 2015, Joo Aun Saw
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
#include <math.h>
#include <time.h>
#include <string.h>
#include <dlfcn.h>

#include "libgps.h"

#define GPS_DATA_OF_INTEREST (TIME_SET | LATLON_SET | ALTITUDE_SET | SPEED_SET | TRACK_SET)

static int libgps_load_sym(void **func, void *handle, char *symbol)
{
   char *sym_error;

   *func = dlsym(handle, symbol);
   if ((sym_error = dlerror()) != NULL)
   {
      fprintf(stderr, "%s\n", sym_error);
      return -1;
   }
   return 0;
}

int libgps_load(gpsd_info *gpsd)
{
   const char *LIBGPS = "libgps.so.20";
   int err = 0;

   gpsd->libgps_handle = dlopen(LIBGPS, RTLD_LAZY);
   if (!gpsd->libgps_handle)
   {
      fprintf(stderr, "%s\n", dlerror());
      return -1;
   }

   err |= libgps_load_sym((void **)(&gpsd->gps_read),    gpsd->libgps_handle, "gps_read");
   err |= libgps_load_sym((void **)(&gpsd->gps_waiting), gpsd->libgps_handle, "gps_waiting");
   err |= libgps_load_sym((void **)(&gpsd->gps_open),    gpsd->libgps_handle, "gps_open");
   err |= libgps_load_sym((void **)(&gpsd->gps_close),   gpsd->libgps_handle, "gps_close");
   err |= libgps_load_sym((void **)(&gpsd->gps_errstr),  gpsd->libgps_handle, "gps_errstr");
   err |= libgps_load_sym((void **)(&gpsd->gps_stream),  gpsd->libgps_handle, "gps_stream");
   if (err)
      return -1;

   return 0;
}

void libgps_unload(gpsd_info *gpsd)
{
   if (gpsd->libgps_handle)
   {
      dlclose(gpsd->libgps_handle);
      gpsd->libgps_handle = NULL;
   }
}

void gpsd_init(gpsd_info *gpsd)
{
   memset(gpsd, 0, sizeof(gpsd_info));
   gpsd->server = "localhost";
   gpsd->port = DEFAULT_GPSD_PORT;
}

int connect_gpsd(gpsd_info *gpsd)
{
   if (!gpsd->gpsd_connected)
   {
      if (gpsd->gps_open(gpsd->server, gpsd->port, &gpsd->gpsdata) != 0)
         return -1;
      gpsd->gpsd_connected = 1;
      gpsd->gps_stream(&gpsd->gpsdata, WATCH_ENABLE, NULL);
   }
   return 0;
}

void disconnect_gpsd(gpsd_info *gpsd)
{
   if (gpsd->gpsd_connected)
   {
      gpsd->gps_stream(&gpsd->gpsdata, WATCH_DISABLE, NULL);
      gpsd->gps_close(&gpsd->gpsdata);
      gpsd->gpsd_connected = 0;
   }
}

void wait_gpsd_stream(gpsd_info *gpsd, int timeout_ms)
{
   if (gpsd->gpsd_connected)
   {
      time_t start;
      gps_mask_t mask = GPS_DATA_OF_INTEREST;
      start = time(NULL);
      while (((time(NULL) - start) < timeout_ms) && ((gpsd->gpsdata.set & mask) == 0))
      {
         if (gpsd->gps_waiting(&gpsd->gpsdata, 200))
            read_gps_data(gpsd);
      }
   }
}

void read_gps_data(gpsd_info *gpsd)
{
   int r = 1;
   while (gpsd->gpsd_connected && (r > 0))
   {
      r = gpsd->gps_read(&gpsd->gpsdata);
      if (r < 0)
      {
         gpsd->gps_close(&gpsd->gpsdata);
         gpsd->gpsd_connected = 0;
      }
      else if (r > 0)
      {
         gpsd->gpsdata_cache.online = gpsd->gpsdata.online;
         if (gpsd->gpsdata.online)
         {
            gps_mask_t mask = GPS_DATA_OF_INTEREST;
            if ((gpsd->gpsdata.fix.mode >= MODE_2D) &&
                (gpsd->gpsdata.fix.mode >= gpsd->gpsdata_cache.fix.mode) &&
                ((gpsd->gpsdata.set & mask) >= gpsd->current_mask))
            {
               gpsd->current_mask = gpsd->gpsdata.set & mask;
               memcpy(&gpsd->gpsdata_cache, &gpsd->gpsdata, sizeof(gpsd->gpsdata_cache));
            }
         }
      }
   }
}

int deg_to_str(double f, char *buf, int buf_size)
{
   int sec, deg, min;
   long frac_deg;
   double fdsec, fsec, fdeg, fmin;

   if (buf_size > 0)
      *buf = 0;

   if (f < 0 || f > 360)
      return -1;

   fmin = modf(f, &fdeg);
   deg = (int)fdeg;
   frac_deg = (long)(fmin * 1000000);

   fsec = modf(fmin * 60, &fmin);
   min = (int)fmin;
   sec = (int)(fsec * 10000.0);

   fdsec = modf(fsec * 60, &fsec);
   sec = (int)fsec;
   snprintf(buf, buf_size, "%02d/1,%02d/1,%02d/1", deg, min, sec);

   return 0;
}
