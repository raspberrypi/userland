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

enum bayer_order {
	//Carefully ordered so that an hflip is ^1,
	//and a vflip is ^2.
	BAYER_ORDER_BGGR,
	BAYER_ORDER_GBRG,
	BAYER_ORDER_GRBG,
	BAYER_ORDER_RGGB
};

struct sensor_regs {
   uint16_t reg;
   uint8_t  data;
};

struct mode_def
{
   struct sensor_regs *regs;
   int num_regs;
   int width;
   int height;
   enum bayer_order order;
};
#define NUM_ELEMENTS(a)  (sizeof(a) / sizeof(a[0]))

#include "ov5647_modes.h"

int width = 0;
int height = 0;

//#define RAW16
//#define RAW8

#ifdef RAW16
	#define BIT_DEPTH 16
	#define UNPACK MMAL_CAMERA_RX_CONFIG_UNPACK_10;
	#define PACK MMAL_CAMERA_RX_CONFIG_PACK_16;
#elif defined RAW8
	#define BIT_DEPTH 8
	#define UNPACK MMAL_CAMERA_RX_CONFIG_UNPACK_10;
	#define PACK MMAL_CAMERA_RX_CONFIG_PACK_8;
#else
	#define BIT_DEPTH 10
	#define UNPACK MMAL_CAMERA_RX_CONFIG_UNPACK_NONE;
	#define PACK MMAL_CAMERA_RX_CONFIG_PACK_NONE;
#endif

struct mode_def *sensor_mode = NULL;


struct sensor_regs ov5647_stop[] = {
   { 0x0100, 0x00 }
};

void update_regs(struct mode_def *mode, int hflip, int vflip, int exposure, int gain);

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
   for (i=0; i<sensor_mode->num_regs; i++)
   {
      unsigned char msg[3];
      *((unsigned short*)&msg) = ((0xFF00&(sensor_mode->regs[i].reg<<8)) + (0x00FF&(sensor_mode->regs[i].reg>>8)));
      msg[2] = sensor_mode->regs[i].data;
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

uint32_t order_and_bit_depth_to_encoding(enum bayer_order order, int bit_depth)
{
	//BAYER_ORDER_BGGR,
	//BAYER_ORDER_GBRG,
	//BAYER_ORDER_GRBG,
	//BAYER_ORDER_RGGB
	const uint32_t depth8[] = {
		MMAL_ENCODING_BAYER_SBGGR8,
		MMAL_ENCODING_BAYER_SGBRG8,
		MMAL_ENCODING_BAYER_SGRBG8,
		MMAL_ENCODING_BAYER_SRGGB8
	};
	const uint32_t depth10[] = {
		MMAL_ENCODING_BAYER_SBGGR10P,
		MMAL_ENCODING_BAYER_SGBRG10P,
		MMAL_ENCODING_BAYER_SGRBG10P,
		MMAL_ENCODING_BAYER_SRGGB10P
	};
	const uint32_t depth12[] = {
		MMAL_ENCODING_BAYER_SBGGR12P,
		0,
		0,
		0
	};
	const uint32_t depth16[] = {
		MMAL_ENCODING_BAYER_SBGGR16,
		0,
		0,
		0
	};
	if (order < 0 || order > 3)
	{
		vcos_log_error("order out of range - %d", order);
		return 0;
	}

	switch(bit_depth)
	{
		case 8:
			return depth8[order];
		case 10:
			return depth10[order];
		case 12:
			return depth12[order];
		case 16:
			return depth16[order];
	}
	vcos_log_error("%d not one of the handled bit depths", bit_depth);
	return 0;
}

int main(int argc, char** args) {
	int mode = 0;
	int hflip = 0;
	int vflip = 0;
	int exposure = 0;
	int gain = 0;
	uint32_t encoding;

	bcm_host_init();
	vcos_log_register("RaspiRaw", VCOS_LOG_CATEGORY);

	if(argc > 1) {
		mode = atoi(args[1]);
	}
	if(argc > 2) {
		hflip = atoi(args[2]);
	}
	if(argc > 3) {
		vflip = atoi(args[3]);
	}
	if(argc > 4) {
		exposure = atoi(args[4]);
	}
	if(argc > 5) {
		gain = atoi(args[5]);
	}
	if(mode >= 0 && mode < NUM_ELEMENTS(ov5647_modes)) {
		sensor_mode = &ov5647_modes[mode];
	}

	if(!sensor_mode)
	{
		vcos_log_error("Invalid mode %d - aborting", mode);
		return -2;
	}

	update_regs(sensor_mode, hflip, vflip, exposure, gain);
	encoding = order_and_bit_depth_to_encoding(sensor_mode->order, BIT_DEPTH);
	if (!encoding)
	{
		vcos_log_error("Failed to map bitdepth %d and order %d into encoding\n", BIT_DEPTH, sensor_mode->order);
		return -3;
	}
	vcos_log_error("Encoding %08X", encoding);

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

	output->format->es->video.crop.width = sensor_mode->width;
	output->format->es->video.crop.height = sensor_mode->height;
	output->format->es->video.width = VCOS_ALIGN_UP(width, 32);
	output->format->es->video.height = VCOS_ALIGN_UP(height, 16);
	output->format->encoding = encoding;

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

	while(1);
	//vcos_sleep(30000);
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

//The process first loads the cleaned up dump of the regiesters
//than updates the known registers to the proper values
//based on: http://www.seeedstudio.com/wiki/images/3/3c/Ov5647_full.pdf

void setRegBit(struct mode_def *mode, uint16_t reg, int bit, int value) {
	int i = 0;
	while(i < mode->num_regs && mode->regs[i].reg != reg) i++;
	if(i == mode->num_regs) {
		vcos_log_error("Reg: %d not found!\n", reg);
		return;
	}
	mode->regs[i].data = (mode->regs[i].data | (1 << bit)) & (~( (1 << bit) ^ (value << bit) ));
}

void setReg(struct mode_def *mode, uint16_t reg, int startBit, int endBit, int value) {
	int i;
	for(i = startBit; i <= endBit; i++) {
		setRegBit(mode, reg, i, value >> i & 1);
	}
}

unsigned int createMask(unsigned int a, int unsigned b) {
	unsigned int r = 0;
	unsigned int i;
	for(i = a; i <= b; i++) {
		r |= 1 << i;
	}

	return r;
}

void update_regs(struct mode_def *mode, int hflip, int vflip, int exposure, int gain) {
	setRegBit(mode, 0x3820, 1, vflip);
	if(vflip)
		mode->order ^= 2;

	setRegBit(mode, 0x3821, 1, hflip);
	if(hflip)
		mode->order ^= 1;

	if(exposure < 0 || exposure > 262143) {
		vcos_log_error("Invalid exposurerange:%d, exposure range is 0 to 262143!\n", exposure);
	} else {
		setReg(mode, 0x3502, 0, 7, exposure & createMask(0, 7));
		setReg(mode, 0x3501, 0, 7, (exposure & createMask(8, 15)) >> 8);
		setReg(mode, 0x3500, 0, 3, (exposure & createMask(16, 19)) >> 16);
	}
	if(gain < 0 || gain > 1023) {
		vcos_log_error("Invalid gain range:%d, gain range is 0 to 1023\n", gain);
	} else {
		setReg(mode, 0x350B, 0, 7, gain & createMask(0, 7));
		setReg(mode, 0x350A, 0, 1, (gain & createMask(8, 9)) >> 8);
	}
}

