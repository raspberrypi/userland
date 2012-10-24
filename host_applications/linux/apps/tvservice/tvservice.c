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


// ---- Include Files -------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>
#include <string.h>

#include "interface/vmcs_host/vc_tvservice.h"

// ---- Public Variables ----------------------------------------------------

// ---- Private Constants and Types -----------------------------------------

// Logging macros (for remapping to other logging mechanisms, i.e., vcos_log)
#define LOG_ERR(  fmt, arg... )  fprintf( stderr, "[E] " fmt "\n", ##arg )
#define LOG_WARN( fmt, arg... )  fprintf( stderr, "[W] " fmt "\n", ##arg )
#define LOG_INFO( fmt, arg... )  fprintf( stderr, "[I] " fmt "\n", ##arg )
#define LOG_DBG(  fmt, arg... )  fprintf( stdout, "[D] " fmt "\n", ##arg )

// Standard output log (for printing normal information to users)
#define LOG_STD(  fmt, arg... )  fprintf( stdout, fmt "\n", ##arg )

// Maximum length of option string (3 characters max for each option + NULL)
#define OPTSTRING_LEN  ( sizeof( long_opts ) / sizeof( *long_opts ) * 3 + 1 )

// ---- Private Variables ---------------------------------------------------

enum
{
   OPT_PREFERRED = 'p',
   OPT_EXPLICIT  = 'e',
   OPT_OFF       = 'o',
   OPT_SDTVON    = 'n',
   OPT_MODES     = 'm',
   OPT_MONITOR   = 'M',
   OPT_STATUS    = 's',
   OPT_DUMPEDID  = 'd',
   OPT_AUDIOSUP  = 'a',
   OPT_SHOWINFO  = 'i',
   OPT_HELP      = 'h',

   // Options from this point onwards don't have any short option equivalents
   OPT_FIRST_LONG_OPT = 0x80,
};

static struct option long_opts[] =
{
   // name                 has_arg              flag     val
   // -------------------  ------------------   ----     ---------------
   { "preferred",          no_argument,         NULL,    OPT_PREFERRED },
   { "explicit",           required_argument,   NULL,    OPT_EXPLICIT },
   { "off",                no_argument,         NULL,    OPT_OFF },
   { "sdtvon",             required_argument,   NULL,    OPT_SDTVON },
   { "modes",              required_argument,   NULL,    OPT_MODES },
   { "monitor",            no_argument,         NULL,    OPT_MONITOR },
   { "status",             no_argument,         NULL,    OPT_STATUS },
   { "dumpedid",           required_argument,   NULL,    OPT_DUMPEDID},
   { "audio",              no_argument,         NULL,    OPT_AUDIOSUP},
   { "info",               required_argument,   NULL,    OPT_SHOWINFO},
   { "help",               no_argument,         NULL,    OPT_HELP },
   { 0,                    0,                   0,       0 }
};

static VCOS_EVENT_T quit_event;

// ---- Private Functions ---------------------------------------------------

static void show_usage( void )
{
   LOG_STD( "Usage: tvservice [OPTION]..." );
   LOG_STD( "  -p, --preferred              Power on HDMI with preferred settings" );
   LOG_STD( "  -e, --explicit=\"GROUP MODE\"  Power on HDMI with explicit GROUP (CEA, DMT, CEA_3D)\n"
            "                               and MODE (see --modes)" );
   LOG_STD( "  -n, --sdtvon=\"MODE ASPECT\"   Power on SDTV with MODE (PAL or NTSC) and ASPECT (4:3 14:9 or 16:9)" );
   LOG_STD( "  -o, --off                    Power off the display" );
   LOG_STD( "  -m, --modes=GROUP            Get supported modes for GROUP (CEA, DMT, CEA_3D)" );
   LOG_STD( "  -M, --monitor                Monitor HDMI events" );
   LOG_STD( "  -s, --status                 Get HDMI status" );
   LOG_STD( "  -a, --audio                  Get supported audio information" );
   LOG_STD( "  -d, --dumpedid <filename>    Dump EDID information to file" );
   LOG_STD( "  -h, --help                   Print this information" );
}

static void create_optstring( char *optstring )
{
   char *short_opts = optstring;
   struct option *option;

   // Figure out the short options from our options structure
   for ( option = long_opts; option->name != NULL; option++ )
   {
      if (( option->flag == NULL ) && ( option->val < OPT_FIRST_LONG_OPT ))
      {
         *short_opts++ = (char)option->val;

         if ( option->has_arg != no_argument )
         {
            *short_opts++ = ':';
         }

         // Optional arguments require two ':'
         if ( option->has_arg == optional_argument )
         {
            *short_opts++ = ':';
         }
      }
   }
   *short_opts++ = '\0';
}

static int get_modes( HDMI_RES_GROUP_T group )
{
   TV_SUPPORTED_MODE_T supported_modes[TV_MAX_SUPPORTED_MODES];
   HDMI_RES_GROUP_T preferred_group;
   uint32_t preferred_mode;
   int num_modes;
   int i;

   vcos_assert(( group == HDMI_RES_GROUP_CEA ) ||
               ( group == HDMI_RES_GROUP_DMT ) ||
               ( group == HDMI_RES_GROUP_CEA_3D ));

   num_modes = vc_tv_hdmi_get_supported_modes( group, supported_modes,
                                               TV_MAX_SUPPORTED_MODES,
                                               &preferred_group,
                                               &preferred_mode );
   if ( num_modes < 0 )
   {
      LOG_ERR( "Failed to get modes" );
      return -1;
   }

   LOG_STD( "Group %s has %u modes:",
            group == HDMI_RES_GROUP_CEA ? "CEA" : group == HDMI_RES_GROUP_DMT ? "DMT" : "CEA_3D", num_modes );
   for ( i = 0; i < num_modes; i++ )
   {
      LOG_STD( "%s mode %u: %ux%u @ %uHz, %s",
               supported_modes[i].native ? "  (native)" : "          ",
               supported_modes[i].code, supported_modes[i].width,
               supported_modes[i].height, supported_modes[i].frame_rate,
               supported_modes[i].scan_mode ? "interlaced" : "progressive" );
   }

   return 0;
}

static int get_status( void )
{
   TV_GET_STATE_RESP_T tvstate = {0};
   static const char *status_str[] = {
      "HPD low",
      "HPD high",
      "DVI mode",
      "HDMI mode",
      "HDCP off",
      "HDCP on",
      "", "", "", "", "", "", "", "", "", "", //end of HDMI states
      "", "", //Currently we don't have a way to detect these 
      "NTSC mode", "PAL mode",
      "composite CP off", "composite CP on",
      "", "", "", "", "", "", "", "", "", ""
   };
   static char status[256] = {0};
   char *s = &status[0];
   int i;
   
   vc_tv_get_state( &tvstate );
   for(i = 0; i < 16; i++) {
      if(tvstate.state & (1 << i)) {
         s += sprintf(s, "%s|", status_str[i]);
      }
   }

   if((tvstate.state & (VC_HDMI_DVI|VC_HDMI_HDMI)) == 0)
      s += sprintf(s, "HDMI off|");

   if(tvstate.state & (VC_SDTV_NTSC|VC_SDTV_PAL)) {
      for(i = 18; i < 31; i++) {
         if(tvstate.state & (1 << i)) {
            s += sprintf(s, "%s|", status_str[i]);
         }
      }
   } else {
      s += sprintf(s, "composite off|");
   }

   *(--s) = '\0';

   if(tvstate.width && tvstate.height) {
      LOG_STD( "state: %s (0x%x), %ux%u @ %uHz, %s", status, tvstate.state,
               tvstate.width, tvstate.height, tvstate.frame_rate,
               tvstate.scan_mode ? "interlaced" : "progressive" );
   } else {
      LOG_STD( "state: %s (0x%x), HDMI powered off", status, tvstate.state);
   }

   return 0;
}

static int get_audiosup( void )
{
  uint8_t sample_rates[] = {32, 44, 48, 88, 96, 176, 192};
  uint8_t sample_sizes[] = {16, 20, 24};
  const char *formats[] = {"Reserved", "PCM", "AC3", "MPEG1", "MP3", "MPEG2", "AAC", "DTS", "ATRAC", "DSD", "EAC3", "DTS_HD", "MLP", "DST", "WMAPRO", "Extended"};
  int i, j, k;
  for (k=EDID_AudioFormat_ePCM; k<EDID_AudioFormat_eMaxCount; k++) {
    int num_channels = 0, max_sample_rate = 0, max_sample_size = 0;
    for (i=1; i<=8; i++) {
      if (vc_tv_hdmi_audio_supported(k, i, EDID_AudioSampleRate_e44KHz, EDID_AudioSampleSize_16bit ) == 0)
        num_channels = i;
    }
    for (i=0, j=EDID_AudioSampleRate_e32KHz; j<=EDID_AudioSampleRate_e192KHz; i++, j<<=1) {
      if (vc_tv_hdmi_audio_supported(k, 1, j, EDID_AudioSampleSize_16bit ) == 0)
        max_sample_rate = i;
    }
    if (k==EDID_AudioFormat_ePCM) {
      for (i=0, j=EDID_AudioSampleSize_16bit; j<=EDID_AudioSampleSize_24bit; i++, j<<=1) {
        if (vc_tv_hdmi_audio_supported(k, 1, EDID_AudioSampleRate_e44KHz, j ) == 0)
          max_sample_size = i;
      }
      if (num_channels>0)
        LOG_STD("%8s supported: Max channels: %d, Max samplerate:%4dkHz, Max samplesize %2d bits.", formats[k], num_channels, sample_rates[max_sample_rate], sample_sizes[max_sample_size]);
    } else {
      for (i=0; i<256; i++) {
        if (vc_tv_hdmi_audio_supported(k, 1, EDID_AudioSampleRate_e44KHz, i ) == 0)
          max_sample_size = i;
      }
      if (num_channels>0)
        LOG_STD("%8s supported: Max channels: %d, Max samplerate:%4dkHz, Max rate %4d kb/s.", formats[k], num_channels, sample_rates[max_sample_rate], 8*max_sample_size);
    }
  }
  return 0;
}

static int dump_edid( const char *filename )
{
   uint8_t buffer[128];
   int written = 0, offset = 0, i, extensions = 0;
   FILE *fp = fopen(filename, "wb");
   int siz = vc_tv_hdmi_ddc_read(offset, sizeof (buffer), buffer);
   offset += sizeof( buffer);
   /* First block always exist */
   if (fp && siz == sizeof(buffer)) {
      written += fwrite(buffer, 1, sizeof buffer, fp);
      extensions = buffer[0x7e]; /* This tells you how many more blocks to read */
      for(i = 0; i < extensions; i++, offset += sizeof( buffer)) {
         siz = vc_tv_hdmi_ddc_read(offset, sizeof( buffer), buffer);
         if(siz == sizeof(buffer)) {
            written += fwrite(buffer, 1, sizeof (buffer), fp);
         } else {
            break;
         }
      }
   } 
   if (fp)
      fclose(fp);
   LOG_STD( "Written %d bytes to %s", written, filename);
   return written < sizeof(buffer);
}

static int show_info( int on )
{
   return vc_tv_show_info(on);
}

static void control_c( int signum )
{
    (void)signum;

    LOG_STD( "Shutting down..." );

    vcos_event_signal( &quit_event );
}

static void tvservice_callback( void *callback_data,
                                uint32_t reason,
                                uint32_t param1,
                                uint32_t param2 )
{
   (void)callback_data;
   (void)param1;
   (void)param2;

   switch ( reason )
   {
      case VC_HDMI_UNPLUGGED:
      {
         LOG_INFO( "HDMI cable is unplugged" );
         break;
      }
      case VC_HDMI_STANDBY:
      {
         LOG_INFO( "HDMI in standby mode" );
         break;
      }
      case VC_HDMI_DVI:
      {
         LOG_INFO( "HDMI in DVI mode" );
         break;
      }
      case VC_HDMI_HDMI:
      {
         LOG_INFO( "HDMI in HDMI mode" );
         break;
      }
      case VC_HDMI_HDCP_UNAUTH:
      {
         LOG_INFO( "HDCP authentication is broken" );
         break;
      }
      case VC_HDMI_HDCP_AUTH:
      {
         LOG_INFO( "HDCP is active" );
         break;
      }
      case VC_HDMI_HDCP_KEY_DOWNLOAD:
      {
         LOG_INFO( "HDCP key download" );
         break;
      }
      case VC_HDMI_HDCP_SRM_DOWNLOAD:
      {
         LOG_INFO( "HDCP revocation list download" );
         break;
      }
      default:
      {
         // Ignore all other reasons
         break;
      }
   }
}

static int start_monitor( void )
{
   if ( vcos_event_create( &quit_event, "tvservice" ) != VCOS_SUCCESS )
   {
      LOG_ERR( "Failed to create quit event" );

      return -1;
   }

   // Handle the INT and TERM signals so we can quit
   signal( SIGINT, control_c );
   signal( SIGTERM, control_c );

   vc_tv_register_callback( &tvservice_callback, NULL );

   return 0;
}

static int power_on_preferred( void )
{
   int ret;

   LOG_STD( "Powering on HDMI with preferred settings" );

   ret = vc_tv_hdmi_power_on_preferred();
   if ( ret != 0 )
   {
      LOG_ERR( "Failed to power on HDMI with preferred settings" );
   }

   return ret;
}

static int power_on_explicit( HDMI_RES_GROUP_T group,
                              uint32_t mode )
{
   int ret;

   LOG_STD( "Powering on HDMI with explicit settings (%s mode %u)",
            group == HDMI_RES_GROUP_CEA ? "CEA" : group == HDMI_RES_GROUP_DMT ? "DMT" : "CEA_3D", mode );

   ret = vc_tv_hdmi_power_on_explicit( HDMI_MODE_HDMI, group, mode );
   if ( ret != 0 )
   {
      LOG_ERR( "Failed to power on HDMI with explicit settings (%s mode %u)",
               group == HDMI_RES_GROUP_CEA ? "CEA" : group == HDMI_RES_GROUP_DMT ? "DMT" : "CEA_3D", mode );
   }

   return ret;
}

static int power_on_sdtv( SDTV_MODE_T mode,
                              SDTV_ASPECT_T aspect )
{
   int ret;
   SDTV_OPTIONS_T options;
   memset(&options, 0, sizeof options);
   options.aspect = aspect;
   LOG_STD( "Powering on SDTV with explicit settings (mode:%d aspect:%d)",
            mode, aspect );

   ret = vc_tv_sdtv_power_on(mode, &options);
   if ( ret != 0 )
   {
      LOG_ERR( "Failed to power on SDTV with explicit settings (mode:%d aspect:%d)",
               mode, aspect );
   }

   return ret;
}

static int power_off( void )
{
   int ret;

   LOG_STD( "Powering off HDMI");

   ret = vc_tv_power_off();
   if ( ret != 0 )
   {
      LOG_ERR( "Failed to power off HDMI" );
   }

   return ret;
}

// ---- Public Functions -----------------------------------------------------

int main( int argc, char **argv )
{
   int32_t ret;
   char optstring[OPTSTRING_LEN];
   int  opt;
   int  opt_preferred = 0;
   int  opt_explicit = 0;
   int  opt_sdtvon = 0;
   int  opt_off = 0;
   int  opt_modes = 0;
   int  opt_monitor = 0;
   int  opt_status = 0;
   int  opt_audiosup = 0;
   int  opt_dumpedid = 0;
   int  opt_showinfo = 0;
   char *dumpedid_filename = NULL;
   VCHI_INSTANCE_T    vchi_instance;
   VCHI_CONNECTION_T *vchi_connection;
   HDMI_RES_GROUP_T power_on_explicit_group = HDMI_RES_GROUP_INVALID;
   uint32_t         power_on_explicit_mode;
   HDMI_RES_GROUP_T get_modes_group = HDMI_RES_GROUP_INVALID;
   SDTV_MODE_T sdtvon_mode;
   SDTV_ASPECT_T sdtvon_aspect;

   // Initialize VCOS
   vcos_init();

   // Create the option string that we will be using to parse the arguments
   create_optstring( optstring );

   // Parse the command line arguments
   while (( opt = getopt_long_only( argc, argv, optstring, long_opts,
                                    NULL )) != -1 )
   {
      switch ( opt )
      {
         case 0:
         {
            // getopt_long returns 0 for entries where flag is non-NULL
            break;
         }
         case OPT_PREFERRED:
         {
            opt_preferred = 1;
            break;
         }
         case OPT_EXPLICIT:
         {
            char group_str[32];

            if ( sscanf( optarg, "%s %u", group_str,
                         &power_on_explicit_mode ) != 2 )
            {
               LOG_ERR( "Invalid arguments '%s'", optarg );
               goto err_out;
            }

            // Check the group first
            if ( vcos_strcasecmp( "CEA", group_str ) == 0 )
            {
               power_on_explicit_group = HDMI_RES_GROUP_CEA;
            }
            else if ( vcos_strcasecmp( "DMT", group_str ) == 0 )
            {
               power_on_explicit_group = HDMI_RES_GROUP_DMT;
            }
            else if ( vcos_strcasecmp( "CEA_3D", group_str ) == 0 )
            {
               power_on_explicit_group = HDMI_RES_GROUP_CEA_3D;
            }
            else
            {
               LOG_ERR( "Invalid group '%s'", group_str );
               goto err_out;
            }

            // Then check if mode is a sane number
            if ( power_on_explicit_mode > 256 )
            {
               LOG_ERR( "Invalid mode '%u'", power_on_explicit_mode );
               goto err_out;
            }

            opt_explicit = 1;
            break;
         }
         case OPT_SDTVON:
         {
            char mode_str[32], aspect_str[32];

            if ( sscanf( optarg, "%s %s", mode_str,
                         aspect_str ) != 2 )
            {
               LOG_ERR( "Invalid arguments '%s'", optarg );
               goto err_out;
            }

            // Check the group first
            if ( vcos_strcasecmp( "NTSC", mode_str ) == 0 )
            {
               sdtvon_mode = SDTV_MODE_NTSC;
            }
            else if ( vcos_strcasecmp( "NTSC_J", mode_str ) == 0 )
            {
               sdtvon_mode = SDTV_MODE_NTSC_J;
            }
            else if ( vcos_strcasecmp( "PAL", mode_str ) == 0 )
            {
               sdtvon_mode = SDTV_MODE_PAL;
            }
            else if ( vcos_strcasecmp( "PAL_M", mode_str ) == 0 )
            {
               sdtvon_mode = SDTV_MODE_PAL_M;
            }
            else
            {
               LOG_ERR( "Invalid mode '%s'", mode_str );
               goto err_out;
            }

            if ( vcos_strcasecmp( "4:3", aspect_str ) == 0 )
            {
               sdtvon_aspect = SDTV_ASPECT_4_3;
            }
            else if ( vcos_strcasecmp( "14:9", aspect_str ) == 0 )
            {
               sdtvon_aspect = SDTV_ASPECT_14_9;
            }
            else if ( vcos_strcasecmp( "16:9", aspect_str ) == 0 )
            {
               sdtvon_aspect = SDTV_ASPECT_16_9;
            }

            opt_sdtvon = 1;
            break;
         }
         case OPT_OFF:
         {
            opt_off = 1;
            break;
         }
         case OPT_MODES:
         {
            if ( vcos_strcasecmp( "CEA", optarg ) == 0 )
            {
               get_modes_group = HDMI_RES_GROUP_CEA;
            }
            else if ( vcos_strcasecmp( "DMT", optarg ) == 0 )
            {
               get_modes_group = HDMI_RES_GROUP_DMT;
            }
            else if ( vcos_strcasecmp( "CEA_3D", optarg ) == 0 )
            {
               get_modes_group = HDMI_RES_GROUP_CEA_3D;
            }
            else
            {
               LOG_ERR( "Invalid group '%s'", optarg );
               goto err_out;
            }

            opt_modes = 1;
            break;
         }
         case OPT_MONITOR:
         {
            opt_monitor = 1;
            break;
         }
         case OPT_STATUS:
         {
            opt_status = 1;
            break;
         }
         case OPT_AUDIOSUP:
         {
            opt_audiosup = 1;
            break;
         }
         case OPT_DUMPEDID:
         {
            opt_dumpedid = 1;
            dumpedid_filename = optarg;
            break;
         }
         case OPT_SHOWINFO:
         {
            opt_showinfo = atoi(optarg)+1;
            break;
         }
         default:
         {
            LOG_ERR( "Unrecognized option '%d'\n", opt );
         }
         case '?':
         case OPT_HELP:
         {
            goto err_usage;
         }
      } // end switch
   } // end while

   argc -= optind;
   argv += optind;

   if (( optind == 1 ) || ( argc > 0 ))
   {
      if ( argc > 0 )
      {
         LOG_ERR( "Unrecognized argument -- '%s'", *argv );
      }

      goto err_usage;
   }

   if (( opt_preferred + opt_explicit + opt_sdtvon > 1 ))
   {
      LOG_ERR( "Conflicting power on options" );
      goto err_usage;
   }

   if ((( opt_preferred == 1 ) || ( opt_explicit == 1 ) || ( opt_sdtvon == 1)) && ( opt_off == 1 ))
   {
      LOG_ERR( "Cannot power on and power off simultaneously" );
      goto err_out;
   }

   // Initialize the VCHI connection
   ret = vchi_initialise( &vchi_instance );
   if ( ret != 0 )
   {
      LOG_ERR( "Failed to initialize VCHI (ret=%d)", ret );
      goto err_out;
   }

   ret = vchi_connect( NULL, 0, vchi_instance );
   if ( ret != 0)
   {
      LOG_ERR( "Failed to create VCHI connection (ret=%d)", ret );
      goto err_out;
   }

//   LOG_INFO( "Starting tvservice" );

   // Initialize the tvservice
   vc_vchi_tv_init( vchi_instance, &vchi_connection, 1 );

   if ( opt_monitor == 1 )
   {
      LOG_STD( "Starting to monitor for HDMI events" );

      if ( start_monitor() != 0 )
      {
         goto err_stop_service;
      }
   }

   if ( opt_modes == 1 )
   {
      if ( get_modes( get_modes_group ) != 0 )
      {
         goto err_stop_service;
      }
   }

   if ( opt_preferred == 1 )
   {
      if ( power_on_preferred() != 0 )
      {
         goto err_stop_service;
      }
   }
   else if ( opt_explicit == 1 )
   {
      if ( power_on_explicit( power_on_explicit_group,
                              power_on_explicit_mode ) != 0 )
      {
         goto err_stop_service;
      }
   }
   else if ( opt_sdtvon == 1 )
   {
      if ( power_on_sdtv( sdtvon_mode,
                              sdtvon_aspect ) != 0 )
      {
         goto err_stop_service;
      }
   }
   else if (opt_off == 1 )
   {
      if ( power_off() != 0 )
      {
         goto err_stop_service;
      }
   }

   if ( opt_status == 1 )
   {
      if ( get_status() != 0 )
      {
         goto err_stop_service;
      }
   }
   
   if ( opt_audiosup == 1 )
   {
      if ( get_audiosup() != 0 )
      {
         goto err_stop_service;
      }
   }
   
   if ( opt_dumpedid == 1 )
   {
      if ( dump_edid(dumpedid_filename) != 0 )
      {
         goto err_stop_service;
      }
   }

   if ( opt_showinfo )
   {
      if ( show_info(opt_showinfo-1) != 0 )
      {
         goto err_stop_service;
      }
   }
   
   if ( opt_monitor == 1 )
   {
      // Wait until we get the signal to exit
      vcos_event_wait( &quit_event );

      vcos_event_delete( &quit_event );
   }

err_stop_service:
//   LOG_INFO( "Stopping tvservice" );

   // Stop the tvservice
   vc_vchi_tv_stop();

   // Disconnect the VCHI connection
   vchi_disconnect( vchi_instance );

   exit( 0 );

err_usage:
   show_usage();

err_out:
   exit( 1 );
}
