/*
Copyright (c) 2015, Raspberry Pi Foundation
Copyright (c) 2015, Dave Stevenson
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
#include <fcntl.h>

//#include <wiringPi.h>
//#include <wiringPiI2C.h>
#include <linux/i2c-dev.h>

#include "interface/vcos/vcos.h"
#include "bcm_host.h"

#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"

#include <sys/ioctl.h>

// Do the GPIO waggling from here, except that needs root access, plus
// there is variation on pin allocation between the various Pi platforms.
// Provided for reference, but needs tweaking to be useful.
//#define DO_PIN_CONFIG

struct sensor_regs {
   uint16_t reg;
   uint8_t  data;
};

#define WIDTH 2592
#define HEIGHT 1944

//#define RAW16
//#define RAW8

#ifdef RAW16
	#define ENCODING MMAL_ENCODING_BAYER_SBGGR16
	#define UNPACK MMAL_CAMERA_RX_CONFIG_UNPACK_10;
	#define PACK MMAL_CAMERA_RX_CONFIG_PACK_16;
#elif defined RAW8
	#define ENCODING MMAL_ENCODING_BAYER_SBGGR8
	#define UNPACK MMAL_CAMERA_RX_CONFIG_UNPACK_10;
	#define PACK MMAL_CAMERA_RX_CONFIG_PACK_8;
#else
	#define ENCODING MMAL_ENCODING_BAYER_SBGGR10P
	#define UNPACK MMAL_CAMERA_RX_CONFIG_UNPACK_NONE;
	#define PACK MMAL_CAMERA_RX_CONFIG_PACK_NONE;
#endif

// These register settings were as logged off the line
// by jbeale. There is a datasheet for OV5647 floating
// about on the internet, but the Pi Foundation have
// information from Omnivision under NDA, therefore
// we can not offer support on this.
// There is some information/discussion on the Freescale
// i.MX6 forums about supporting OV5647 on that board.
// There may be information available there that is of use.
//
// REQUESTS FOR SUPPORT ABOUT THESE REGISTER VALUES WILL
// BE IGNORED.
struct sensor_regs ov5647[] =
{
   {0x0100, 0x00},
   {0x0103, 0x01},
   {0x3034, 0x1a},
   {0x3035, 0x21},
   {0x3036, 0x69},
   {0x303c, 0x11},
   {0x3106, 0xf5},
   {0x3821, 0x00},
   {0x3820, 0x00},
   {0x3827, 0xec},
   {0x370c, 0x03},
   {0x3612, 0x5b},
   {0x3618, 0x04},
   {0x5000, 0x06},
   {0x5002, 0x40},
   {0x5003, 0x08},
   {0x5a00, 0x08},
   {0x3000, 0x00},
   {0x3001, 0x00},
   {0x3002, 0x00},
   {0x3016, 0x08},
   {0x3017, 0xe0},
   {0x3018, 0x44},
   {0x301c, 0xf8},
   {0x301d, 0xf0},
   {0x3a18, 0x00},
   {0x3a19, 0xf8},
   {0x3c01, 0x80},
   {0x3b07, 0x0c},
   {0x380c, 0x0b},
   {0x380d, 0x1c},
   {0x380e, 0x07},
   {0x380f, 0xb0},
   {0x3814, 0x11},
   {0x3815, 0x11},
   {0x3708, 0x64},
   {0x3709, 0x12},
   {0x3808, 0x0a},
   {0x3809, 0x20},
   {0x380a, 0x07},
   {0x380b, 0x98},
   {0x3800, 0x00},
   {0x3801, 0x00},
   {0x3802, 0x00},
   {0x3803, 0x00},
   {0x3804, 0x0a},
   {0x3805, 0x3f},
   {0x3806, 0x07},
   {0x3807, 0xa3},
   {0x3811, 0x10},
   {0x3813, 0x06},
   {0x3630, 0x2e},
   {0x3632, 0xe2},
   {0x3633, 0x23},
   {0x3634, 0x44},
   {0x3636, 0x06},
   {0x3620, 0x64},
   {0x3621, 0xe0},
   {0x3600, 0x37},
   {0x3704, 0xa0},
   {0x3703, 0x5a},
   {0x3715, 0x78},
   {0x3717, 0x01},
   {0x3731, 0x02},
   {0x370b, 0x60},
   {0x3705, 0x1a},
   {0x3f05, 0x02},
   {0x3f06, 0x10},
   {0x3f01, 0x0a},
   {0x3a08, 0x01},
   {0x3a09, 0x28},
   {0x3a0a, 0x00},
   {0x3a0b, 0xf6},
   {0x3a0d, 0x08},
   {0x3a0e, 0x06},
   {0x3a0f, 0x58},
   {0x3a10, 0x50},
   {0x3a1b, 0x58},
   {0x3a1e, 0x50},
   {0x3a11, 0x60},
   {0x3a1f, 0x28},
   {0x4001, 0x02},
   {0x4004, 0x04},
   {0x4000, 0x09},
   {0x4837, 0x16},
   {0x4800, 0x24},
   {0x3503, 0x03},
   { 0x3820, 0x41 },
   { 0x3821, 0x03 },

   { 0x350A, 0x00 },
   { 0x350B, 0x23 },
   { 0x3212, 0x00 },
   { 0x3500, 0x00 },
   { 0x3501, 0x04 },
   { 0x3502, 0x60 },
   { 0x3212, 0x10 },
   { 0x3212, 0xA0 },

   { 0x0100, 0x01 }
};

struct sensor_regs ov5647_stop[] = {
   { 0x0100, 0x00 }
};
const int NUM_REGS_START = sizeof(ov5647) / sizeof(struct sensor_regs);

const int NUM_REGS_STOP = sizeof(ov5647_stop) / sizeof(struct sensor_regs);

void start_camera_streaming(void)
{
   int fd, i;

#ifdef DO_PIN_CONFIG
   wiringPiSetupGpio();
   pinModeAlt(0, INPUT);
   pinModeAlt(1, INPUT);
   //Toggle these pin modes to ensure they get changed.
   pinModeAlt(28, INPUT);
   pinModeAlt(28, 4);	//Alt0
   pinModeAlt(29, INPUT);
   pinModeAlt(29, 4);	//Alt0
   digitalWrite(41, 1); //Shutdown pin on B+ and Pi2
   digitalWrite(32, 1); //LED pin on B+ and Pi2
#endif
   fd = open("/dev/i2c-0", O_RDWR);
   if (!fd)
   {
      vcos_log_error("Couldn't open I2C device");
      return;
   }
   if(ioctl(fd, I2C_SLAVE, 0x36) < 0)
   {
      vcos_log_error("Failed to set I2C address");
      return;
   }
   for (i=0; i<NUM_REGS_START; i++)
   {
      unsigned char msg[3];
      *((unsigned short*)&msg) = ((0xFF00&(ov5647[i].reg<<8)) + (0x00FF&(ov5647[i].reg>>8)));
      msg[2] = ov5647[i].data;
      if(write(fd, msg, 3) != 3)
      {
         vcos_log_error("Failed to write register index %d", i);
      }
   }
   close(fd);
}

void stop_camera_streaming(void)
{
   int fd, i;
   fd = open("/dev/i2c-0", O_RDWR);
   if (!fd)
   {
      vcos_log_error("Couldn't open I2C device");
      return;
   }
   if(ioctl(fd, I2C_SLAVE, 0x36) < 0)
   {
      vcos_log_error("Failed to set I2C address");
      return;
   }
   for (i=0; i<NUM_REGS_STOP; i++)
   {
      unsigned char msg[3];
      *((unsigned short*)&msg) = ((0xFF00&(ov5647_stop[i].reg<<8)) + (0x00FF&(ov5647_stop[i].reg>>8)));
      msg[2] = ov5647_stop[i].data;
      if(write(fd, msg, 3) != 3)
      {
         vcos_log_error("Failed to write register index %d", i);
      }
   }
   close(fd);
#ifdef DO_PIN_CONFIG
   digitalWrite(41, 0); //Shutdown pin on B+ and Pi2
   digitalWrite(32, 0); //LED pin on B+ and Pi2
#endif
}

int running = 0;
static void callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
	static int count = 0;
	vcos_log_error("Buffer %p returned, filled %d, timestamp %llu, flags %04X", buffer, buffer->length, buffer->pts, buffer->flags);
	if(running)
	{
		if(!(buffer->flags&MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO) &&
                   (((count++)%15)==0))
		{
			// Save every 15th frame
			// SD card access is to slow to do much more.
			FILE *file;
			char filename[20];
			sprintf(filename, "raw%04d.raw", count);
			file = fopen(filename, "wb");
			if(file)
			{
				fwrite(buffer->data, buffer->length, 1, file);
				fclose(file);
			}
		}
		buffer->length = 0;
		mmal_port_send_buffer(port, buffer);
	}
	else
		mmal_buffer_header_release(buffer);
}

int main (void)
{
	MMAL_COMPONENT_T *rawcam;
	MMAL_STATUS_T status;
	MMAL_PORT_T *output;
	MMAL_POOL_T *pool;
	MMAL_PARAMETER_CAMERA_RX_CONFIG_T rx_cfg = {{MMAL_PARAMETER_CAMERA_RX_CONFIG, sizeof(rx_cfg)}};
	int i;

	bcm_host_init();
	vcos_log_register("RaspiRaw", VCOS_LOG_CATEGORY);

	status = mmal_component_create("vc.ril.rawcam", &rawcam);
	if(status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to create rawcam");
		return -1;
	}
	output = rawcam->output[0];
	status = mmal_port_parameter_get(output, &rx_cfg.hdr);
	if(status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to get cfg");
		goto component_destroy;
	}
	rx_cfg.unpack = UNPACK;
	rx_cfg.pack = PACK;
vcos_log_error("Set pack to %d, unpack to %d", rx_cfg.unpack, rx_cfg.pack);
	status = mmal_port_parameter_set(output, &rx_cfg.hdr);
	if(status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to set cfg");
		goto component_destroy;
	}

	status = mmal_component_enable(rawcam);
	if(status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to enable");
		goto component_destroy;
	}
	status = mmal_port_parameter_set_boolean(output, MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE);
	if(status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to set zero copy");
		goto component_disable;
	}

	output->format->es->video.crop.width = WIDTH;
	output->format->es->video.crop.height = HEIGHT;
	output->format->es->video.width = VCOS_ALIGN_UP(WIDTH, 32);
	output->format->es->video.height = VCOS_ALIGN_UP(HEIGHT, 16);
	output->format->encoding = ENCODING;
	status = mmal_port_format_commit(output);
	if(status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed port_format_commit");
		goto component_disable;
	}

	vcos_log_error("Create pool of %d buffers of size %d", output->buffer_num, output->buffer_size);
	pool = mmal_port_pool_create(output, output->buffer_num, output->buffer_size);
	if(!pool)
	{
		vcos_log_error("Failed to create pool");
		goto component_disable;
	}

	status = mmal_port_enable(output, callback);
	if(status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to enable port");
		goto pool_destroy;
	}
	running = 1;
	for(i=0; i<output->buffer_num; i++)
	{
		MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(pool->queue);

		if (!buffer)
		{
			vcos_log_error("Where'd my buffer go?!");
			goto port_disable;
		}
		status = mmal_port_send_buffer(output, buffer);
		if(status != MMAL_SUCCESS)
		{
			vcos_log_error("mmal_port_send_buffer failed on buffer %p, status %d", buffer, status);
			goto port_disable;
		}
		vcos_log_error("Sent buffer %p", buffer);
	}

	start_camera_streaming();

	vcos_sleep(30000);
	running = 0;

	stop_camera_streaming();

port_disable:
	status = mmal_port_disable(output);
	if(status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to disable port");
		return -1;
	}
pool_destroy:
	mmal_port_pool_destroy(output, pool);
component_disable:
	status = mmal_component_disable(rawcam);
	if(status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to disable");
	}
component_destroy:
	mmal_component_destroy(rawcam);
	return 0;
}
