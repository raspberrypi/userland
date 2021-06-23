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

#include <stdio.h>
#include "include/bcm_host.h"
#include "interface/vmcs_host/vc_dispmanx.h"
#include "interface/vmcs_host/vc_vchi_gencmd.h"
#include "interface/vmcs_host/vc_vchi_bufman.h"
#include "interface/vmcs_host/vc_tvservice.h"
#include "interface/vmcs_host/vc_cecservice.h"
#include "interface/vchiq_arm/vchiq_if.h"

static VCHI_INSTANCE_T global_initialise_instance;
static VCHI_CONNECTION_T *global_connection;

int32_t graphics_get_display_size( const uint16_t display_number,
                                                    uint32_t *width,
                                                    uint32_t *height)
{
   DISPMANX_DISPLAY_HANDLE_T display_handle = 0;
   DISPMANX_MODEINFO_T mode_info;
   int32_t success = -1;

   // Display must be opened first.
   display_handle = vc_dispmanx_display_open(display_number);

   if (display_handle) {
      success = vc_dispmanx_display_get_info(display_handle, &mode_info);
         
      if( success >= 0 )
      {
         if( NULL != width )
         {
            *width = mode_info.width;
         }
         
         if( NULL != height )
         {
            *height = mode_info.height;
         }
      }
      vc_dispmanx_display_close(display_handle);
      display_handle = 0;
   }

   return success;
}

void host_app_message_handler(void)
{
   printf("host_app_message_handler\n");
}

void vc_host_get_vchi_state(VCHI_INSTANCE_T *initialise_instance, VCHI_CONNECTION_T **connection)
{
   if (initialise_instance) *initialise_instance = global_initialise_instance;
   if (connection) *connection = global_connection;
}

void bcm_host_init(void)
{
   VCHIQ_INSTANCE_T vchiq_instance;
   static int initted;
   int success = -1;
   char response[ 128 ];
   
   if (initted)
	return;
   initted = 1;
   vcos_init();

   if (vchiq_initialise(&vchiq_instance) != VCHIQ_SUCCESS)
   {
      printf("* failed to open vchiq instance\n");
      exit(-1);
   }

   vcos_log("vchi_initialise");
   success = vchi_initialise( &global_initialise_instance);
   vcos_assert(success == 0);
   vchiq_instance = (VCHIQ_INSTANCE_T)global_initialise_instance;

   global_connection = vchi_create_connection(single_get_func_table(),
                                              vchi_mphi_message_driver_func_table());

   vcos_log("vchi_connect");
   vchi_connect(&global_connection, 1, global_initialise_instance);
  
   vc_vchi_gencmd_init (global_initialise_instance, &global_connection, 1);
   vc_vchi_dispmanx_init (global_initialise_instance, &global_connection, 1);
   vc_vchi_tv_init (global_initialise_instance, &global_connection, 1);
   vc_vchi_cec_init (global_initialise_instance, &global_connection, 1);
   //vc_vchi_bufman_init (global_initialise_instance, &global_connection, 1);

   if ( success == 0 )
   {
      success = vc_gencmd( response, sizeof(response), "set_vll_dir /sd/vlls" );
      vcos_assert( success == 0 );
   }
}

void bcm_host_deinit(void)
{
}

// Fix linking problems. These are referenced by libs, but shouldn't be called
void wfc_stream_await_buffer(void * stream)
{
   vcos_assert(0);
}

static unsigned get_dt_ranges(const char *filename, unsigned offset)
{
   unsigned address = ~0;
   FILE *fp = fopen(filename, "rb");
   if (fp)
   {
      unsigned char buf[4];
      fseek(fp, offset, SEEK_SET);
      if (fread(buf, 1, sizeof buf, fp) == sizeof buf)
         address = buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3] << 0;
      fclose(fp);
   }
   return address;
}

unsigned bcm_host_get_peripheral_address(void)
{
   unsigned address = get_dt_ranges("/proc/device-tree/soc/ranges", 4);
   if (address == 0)
      address = get_dt_ranges("/proc/device-tree/soc/ranges", 8);
   return address == ~0 ? 0x20000000 : address;
}

unsigned bcm_host_get_peripheral_size(void)
{
   unsigned address = get_dt_ranges("/proc/device-tree/soc/ranges", 4);
   address = get_dt_ranges("/proc/device-tree/soc/ranges", (address == 0) ? 12 : 8);
   return address == ~0 ? 0x01000000 : address;
}

unsigned bcm_host_get_sdram_address(void)
{
   unsigned address = get_dt_ranges("/proc/device-tree/axi/vc_mem/reg", 8);
   return address == ~0 ? 0x40000000 : address;
}

static int read_string_from_file(const char *filename, const char *format, unsigned int *value)
{
   FILE *fin;
   char str[256];
   int found = 0;

   fin = fopen(filename, "rt");

   if (fin == NULL)
      return 0;

   while (fgets(str, sizeof(str), fin) != NULL)
   {
      if (value)
      {
         if (sscanf(str, format, value) == 1)
         {
            found = 1;
            break;
         }
      }
      else
      {
         if (!strcmp(str, format))
         {
            found = 1;
            break;
         }
      }
   }

   fclose(fin);

   return found;
}

static unsigned int get_revision_code()
{
   static unsigned int revision_num = -1;
   unsigned int num;
   if (revision_num == -1 && read_string_from_file("/proc/cpuinfo", "Revision : %x", &num))
      revision_num = num;
   return revision_num;
}

/* Returns the type of the Pi being used
*/
int bcm_host_get_model_type(void)
{
   static int model_type = -1;
   if (model_type != -1)
      return model_type;
   unsigned int revision_num = get_revision_code();

   if (!revision_num)
      model_type = 0;

   // Check for old/new style revision code. Bit 23 will be guaranteed one for new style
   else if (revision_num & 0x800000)
      model_type = (revision_num & 0xff0) >> 4;
   else
   {
      // Mask off warrantee and overclock bits.
      revision_num &= 0xffffff;

      // Map old style to new Type code
      if (revision_num < 2 || revision_num > 21)
         return 0;

      static const unsigned char type_map[] =
      {
         1,   // B rev 1.0  2
         1,   // B rev 1.0  3
         1,   // B rev 2.0  4
         1,   // B rev 2.0  5
         1,   // B rev 2.0  6
         0,   // A rev 2    7
         0,   // A rev 2    8
         0,   // A rev 2    9
         0, 0, 0,  // unused  a,b,c
         1,   // B  rev 2.0  d
         1,   // B rev 2.0  e
         1,   // B rev 2.0  f
         3,   // B+ rev 1.2 10
         6,   // CM1        11
         2,   // A+ rev1.1  12
         3,   // B+ rev 1.2 13
         6,   // CM1        14
         2    // A+         15
      };
      model_type = type_map[revision_num-2];
   }

   return model_type;
}

/* Test if the host is a member of the Pi 4 family (4B, 400 and CM4)
*/
int bcm_host_is_model_pi4(void)
{
   return bcm_host_get_processor_id() == BCM_HOST_PROCESSOR_BCM2711;
}

/* returns the processor ID
*/
int bcm_host_get_processor_id(void)
{
   unsigned int revision_num = get_revision_code();

   if (revision_num & 0x800000)
   {
      return (revision_num & 0xf000) >> 12;
   }
   else
   {
      // Old style number only used 2835
      return BCM_HOST_PROCESSOR_BCM2835;
   }
}


static int bcm_host_is_fkms_or_kms_active(int kms)
{
   if (!read_string_from_file("/proc/device-tree/soc/v3d@7ec00000/status", "okay", NULL) &&
       !read_string_from_file("/proc/device-tree/v3dbus/v3d@7ec04000/status", "okay", NULL))
      return 0;
   else
      return read_string_from_file("/proc/device-tree/soc/firmwarekms@7e600000/status", "okay", NULL) ^ kms;
}

int bcm_host_is_fkms_active(void)
{
   static int active = -1;
   if (active == -1)
      active = bcm_host_is_fkms_or_kms_active(0);
   return active;
}

int bcm_host_is_kms_active(void)
{
   static int active = -1;
   if (active == -1)
      active = bcm_host_is_fkms_or_kms_active(1);
   return active;
}
