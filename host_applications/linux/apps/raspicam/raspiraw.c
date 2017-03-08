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

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "interface/vcos/vcos.h"
#include "bcm_host.h"

#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_connection.h"

#include <sys/ioctl.h>

//If CAPTURE is 1, images are saved to file.
//If 0, the ISP and render are hooked up instead
#define CAPTURE	0

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
	int native_bit_depth;
};

struct sensor_def
{
	char *name;
	struct mode_def *modes;
	int num_modes;
	struct sensor_regs *stop;
	int num_stop_regs;
	uint8_t i2c_addr;
	int i2c_ident_length;
	uint16_t i2c_ident_reg;
	uint16_t i2c_ident_value;

	uint16_t vflip_reg;
	int vflip_reg_bit;
	uint16_t hflip_reg;
	int hflip_reg_bit;

	uint16_t exposure_reg;
	int exposure_reg_num_bits;

	uint16_t gain_reg;
	int gain_reg_num_bits;
};


#define NUM_ELEMENTS(a)  (sizeof(a) / sizeof(a[0]))

#include "ov5647_modes.h"
#include "imx219_modes.h"

struct sensor_def *sensors[] = {
	&ov5647,
	&imx219,
	NULL
};

//#define RAW16
//#define RAW8

#ifdef RAW16
	#define BIT_DEPTH 16
#elif defined RAW8
	#define BIT_DEPTH 8
#else
	#define BIT_DEPTH 10
#endif

void update_regs(struct sensor_def *sensor, struct mode_def *mode, int hflip, int vflip, int exposure, int gain);

static int i2c_rd(int fd, uint8_t i2c_addr, uint16_t reg, uint8_t *values, uint32_t n)
{
	int err;
	uint8_t buf[2] = { reg >> 8, reg & 0xff };
	struct i2c_rdwr_ioctl_data msgset;
	struct i2c_msg msgs[2] = {
		{
			 .addr = i2c_addr,
			 .flags = 0,
			 .len = 2,
			 .buf = buf,
		},
		{
			.addr = i2c_addr,
			.flags = I2C_M_RD,
			.len = n,
			.buf = values,
		},
	};

	msgset.msgs = msgs;
	msgset.nmsgs = 2;

	err = ioctl(fd, I2C_RDWR, &msgset);
	if(err != msgset.nmsgs)
		return -1;

	return 0;
}

struct sensor_def * probe_sensor(void)
{
	int fd;
	struct sensor_def **sensor_list = &sensors[0];
	struct sensor_def *sensor = NULL;

	fd = open("/dev/i2c-0", O_RDWR);
	if (!fd)
	{
		vcos_log_error("Couldn't open I2C device");
		return NULL;
	}

	while(*sensor_list != NULL)
	{
		uint16_t reg = 0;
		sensor = *sensor_list;
		vcos_log_error("Probing sensor %s on addr %02X", sensor->name, sensor->i2c_addr);
		if(sensor->i2c_ident_length <= 2)
		{
			if(!i2c_rd(fd, sensor->i2c_addr, sensor->i2c_ident_reg, (uint8_t*)&reg, sensor->i2c_ident_length))
			{
				if (reg == sensor->i2c_ident_value)
				{
					vcos_log_error("Found sensor at address %02X", sensor->i2c_addr);
					break;
				}
			}
	}
		sensor_list++;
	}
	return sensor;
}

void start_camera_streaming(struct sensor_def *sensor, struct mode_def *mode)
{
	int fd, i;

	fd = open("/dev/i2c-0", O_RDWR);
	if (!fd)
	{
		vcos_log_error("Couldn't open I2C device");
		return;
	}
	if(ioctl(fd, I2C_SLAVE, sensor->i2c_addr) < 0)
	{
		vcos_log_error("Failed to set I2C address");
		return;
	}
	for (i=0; i<mode->num_regs; i++)
	{
		unsigned char msg[3];
		*((unsigned short*)&msg) = ((0xFF00&(mode->regs[i].reg<<8)) + (0x00FF&(mode->regs[i].reg>>8)));
		msg[2] = mode->regs[i].data;
		if(write(fd, msg, 3) != 3)
		{
			vcos_log_error("Failed to write register index %d", i);
		}
	}
	close(fd);
}

void stop_camera_streaming(struct sensor_def *sensor)
{
	int fd, i;
	fd = open("/dev/i2c-0", O_RDWR);
	if (!fd)
	{
		vcos_log_error("Couldn't open I2C device");
		return;
	}
	if(ioctl(fd, I2C_SLAVE, sensor->i2c_addr) < 0)
	{
		vcos_log_error("Failed to set I2C address");
		return;
	}
	for (i=0; i<sensor->num_stop_regs; i++)
	{
		unsigned char msg[3];
		*((unsigned short*)&msg) = ((0xFF00&(sensor->stop[i].reg<<8)) + (0x00FF&(sensor->stop[i].reg>>8)));
		msg[2] = sensor->stop[i].data;
		if(write(fd, msg, 3) != 3)
		{
			vcos_log_error("Failed to write register index %d", i);
		}
	}
	close(fd);
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
	int exposure = -1;
	int gain = -1;
	uint32_t encoding;
	struct sensor_def *sensor;
	struct mode_def *sensor_mode = NULL;

	bcm_host_init();
	vcos_log_register("RaspiRaw", VCOS_LOG_CATEGORY);

	sensor = probe_sensor();
	if (!sensor)
	{
		vcos_log_error("No sensor found. Aborting");
		return -1;
	}

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
	if(mode >= 0 && mode < sensor->num_modes) {
		sensor_mode = &sensor->modes[mode];
	}

	if(!sensor_mode)
	{
		vcos_log_error("Invalid mode %d - aborting", mode);
		return -2;
	}

	update_regs(sensor, sensor_mode, hflip, vflip, exposure, gain);
	encoding = order_and_bit_depth_to_encoding(sensor_mode->order, BIT_DEPTH);
	if (!encoding)
	{
		vcos_log_error("Failed to map bitdepth %d and order %d into encoding\n", BIT_DEPTH, sensor_mode->order);
		return -3;
	}
	vcos_log_error("Encoding %08X", encoding);

	MMAL_COMPONENT_T *rawcam=NULL, *isp=NULL, *render=NULL;
	MMAL_STATUS_T status;
	MMAL_PORT_T *output = NULL;
	MMAL_POOL_T *pool = NULL;
	MMAL_CONNECTION_T *rawcam_isp = NULL;
	MMAL_CONNECTION_T *isp_render = NULL;
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

	status = mmal_component_create("vc.ril.isp", &isp);
	if(status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to create isp");
		goto component_destroy;
	}

	status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER, &render);
	if(status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to create render");
		goto component_destroy;
	}

	output = rawcam->output[0];
	status = mmal_port_parameter_get(output, &rx_cfg.hdr);
	if(status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to get cfg");
		goto component_destroy;
	}
	if (BIT_DEPTH == sensor_mode->native_bit_depth)
	{
		rx_cfg.unpack = MMAL_CAMERA_RX_CONFIG_UNPACK_NONE;
		rx_cfg.pack = MMAL_CAMERA_RX_CONFIG_PACK_NONE;
	}
	else
	{
		switch(sensor_mode->native_bit_depth)
		{
			case 8:
				rx_cfg.unpack = MMAL_CAMERA_RX_CONFIG_UNPACK_8;
				break;
			case 10:
				rx_cfg.unpack = MMAL_CAMERA_RX_CONFIG_UNPACK_10;
				break;
			case 12:
				rx_cfg.unpack = MMAL_CAMERA_RX_CONFIG_UNPACK_12;
				break;
			case 14:
				rx_cfg.unpack = MMAL_CAMERA_RX_CONFIG_UNPACK_16;
				break;
			case 16:
				rx_cfg.unpack = MMAL_CAMERA_RX_CONFIG_UNPACK_16;
				break;
			default:
				vcos_log_error("Unknown native bit depth %d", sensor_mode->native_bit_depth);
				rx_cfg.unpack = MMAL_CAMERA_RX_CONFIG_UNPACK_NONE;
				break;
		}
		switch(BIT_DEPTH)
		{
			case 8:
				rx_cfg.pack = MMAL_CAMERA_RX_CONFIG_PACK_8;
				break;
			case 10:
				rx_cfg.pack = MMAL_CAMERA_RX_CONFIG_PACK_10;
				break;
			case 12:
				rx_cfg.pack = MMAL_CAMERA_RX_CONFIG_PACK_12;
				break;
			case 14:
				rx_cfg.pack = MMAL_CAMERA_RX_CONFIG_PACK_14;
				break;
			case 16:
				rx_cfg.pack = MMAL_CAMERA_RX_CONFIG_PACK_16;
				break;
			default:
				vcos_log_error("Unknown output bit depth %d", BIT_DEPTH);
				rx_cfg.pack = MMAL_CAMERA_RX_CONFIG_UNPACK_NONE;
				break;
		}
	}
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
		vcos_log_error("Failed to enable rawcam");
		goto component_destroy;
	}
	status = mmal_component_enable(isp);
	if(status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to enable isp");
		goto component_destroy;
	}
	status = mmal_component_enable(render);
	if(status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to enable render");
		goto component_destroy;
	}

	output->format->es->video.crop.width = sensor_mode->width;
	output->format->es->video.crop.height = sensor_mode->height;
	output->format->es->video.width = VCOS_ALIGN_UP(sensor_mode->width, 16);
	output->format->es->video.height = VCOS_ALIGN_UP(sensor_mode->height, 16);
	output->format->encoding = encoding;

	status = mmal_port_format_commit(output);
	if(status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed port_format_commit");
		goto component_disable;
	}

	if (CAPTURE)
	{
		status = mmal_port_parameter_set_boolean(output, MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE);
		if(status != MMAL_SUCCESS)
		{
			vcos_log_error("Failed to set zero copy");
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
	}
	else
	{
		status = mmal_connection_create(&rawcam_isp, output, isp->input[0], MMAL_CONNECTION_FLAG_TUNNELLING);
		if(status != MMAL_SUCCESS)
		{
			vcos_log_error("Failed to create rawcam->isp connection");
			goto pool_destroy;
		}

		MMAL_PORT_T *port = isp->output[0];
		port->format->es->video.crop.width = sensor_mode->width;
		port->format->es->video.crop.height = sensor_mode->height;
		if (port->format->es->video.crop.width > 1920)
		{
			//Display can only go up to a certain resolution before underflowing
			port->format->es->video.crop.width /= 2;
			port->format->es->video.crop.height /= 2;
		}
		port->format->es->video.width = VCOS_ALIGN_UP(port->format->es->video.crop.width, 32);
		port->format->es->video.height = VCOS_ALIGN_UP(port->format->es->video.crop.height, 16);
		port->format->encoding = MMAL_ENCODING_I420;
		status = mmal_port_format_commit(port);
		if(status != MMAL_SUCCESS)
		{
			vcos_log_error("Failed to commit port format on isp output");
			goto pool_destroy;
		}

		status = mmal_connection_create(&isp_render, isp->output[0], render->input[0], MMAL_CONNECTION_FLAG_TUNNELLING);
		if(status != MMAL_SUCCESS)
		{
			vcos_log_error("Failed to create isp->render connection");
			goto pool_destroy;
		}

		status = mmal_connection_enable(rawcam_isp);
		if(status != MMAL_SUCCESS)
		{
			vcos_log_error("Failed to enable rawcam->isp connection");
			goto pool_destroy;
		}
		status = mmal_connection_enable(isp_render);
		if(status != MMAL_SUCCESS)
		{
			vcos_log_error("Failed to enable isp->render connection");
			goto pool_destroy;
		}
	}

	start_camera_streaming(sensor, sensor_mode);

	while(1) {
		vcos_sleep(1000);
	}
	running = 0;

	stop_camera_streaming(sensor);

port_disable:
	status = mmal_port_disable(output);
	if(status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to disable port");
		return -1;
	}
pool_destroy:
	if (pool)
		mmal_port_pool_destroy(output, pool);
	if (isp_render)
	{
		mmal_connection_disable(isp_render);
		mmal_connection_destroy(isp_render);
	}
	if (rawcam_isp)
	{
		mmal_connection_disable(rawcam_isp);
		mmal_connection_destroy(rawcam_isp);
	}
component_disable:
	status = mmal_component_disable(render);
	if(status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to disable render");
	}
	status = mmal_component_disable(isp);
	if(status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to disable isp");
	}
	status = mmal_component_disable(rawcam);
	if(status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to disable rawcam");
	}
component_destroy:
	if (rawcam)
		mmal_component_destroy(rawcam);
	if (isp)
		mmal_component_destroy(isp);
	if (render)
		mmal_component_destroy(render);
	return 0;
}

//The process first loads the cleaned up dump of the registers
//than updates the known registers to the proper values
//based on: http://www.seeedstudio.com/wiki/images/3/3c/Ov5647_full.pdf
enum operation {
	EQUAL,	//Set bit to value
	SET,	//Set bit
	CLEAR,	//Clear bit
	XOR	//Xor bit
};

void modRegBit(struct mode_def *mode, uint16_t reg, int bit, int value, enum operation op)
{
	int i = 0;
	uint8_t val;
	while(i < mode->num_regs && mode->regs[i].reg != reg) i++;
	if(i == mode->num_regs) {
		vcos_log_error("Reg: %04X not found!\n", reg);
		return;
	}
	val = mode->regs[i].data;

	switch(op)
	{
		case EQUAL:
			val = (val | (1 << bit)) & (~( (1 << bit) ^ (value << bit) ));
			break;
		case SET:
			val = val | (1 << bit);
			break;
		case CLEAR:
			val = val & ~(1 << bit);
			break;
		case XOR:
			val = val ^ (value << bit);
			break;
	}
	mode->regs[i].data = val;
}

void modReg(struct mode_def *mode, uint16_t reg, int startBit, int endBit, int value, enum operation op)
{
	int i;
	for(i = startBit; i <= endBit; i++) {
		modRegBit(mode, reg, i, value >> i & 1, op);
	}
}

void update_regs(struct sensor_def *sensor, struct mode_def *mode, int hflip, int vflip, int exposure, int gain)
{
	if (sensor->vflip_reg)
	{
		modRegBit(mode, sensor->vflip_reg, 1, vflip, XOR);
		if(vflip)
			mode->order ^= 2;
	}

	if (sensor->hflip_reg)
	{
		modRegBit(mode, sensor->hflip_reg, 1, hflip, XOR);
		if(hflip)
			mode->order ^= 1;
	}

	if (sensor->exposure_reg && exposure != -1)
	{
		if(exposure < 0 || exposure >= (1<<sensor->exposure_reg_num_bits)) {
			vcos_log_error("Invalid exposure:%d, exposure range is 0 to %u!\n",
						exposure, (1<<sensor->exposure_reg_num_bits)-1);
		} else {
			uint8_t val;
			int i, j=sensor->exposure_reg_num_bits-1;
			int num_regs = (sensor->exposure_reg_num_bits+7)>>3;

			for(i=0; i<num_regs; i++, j-=8)
			{
				val = (exposure >> (j&~7)) & 0xFF;
				modReg(mode, sensor->exposure_reg+i, 0, j&0x7, val, EQUAL);
				vcos_log_error("Set exposure %04X to %02X", sensor->exposure_reg+i, val);
			}
		}
	}
	if (sensor->gain_reg && gain != -1)
	{
		if(gain < 0 || gain >= (1<<sensor->gain_reg_num_bits)) {
			vcos_log_error("Invalid gain:%d, gain range is 0 to %u\n",
						gain, (1<<sensor->gain_reg_num_bits)-1);
		} else {
			uint8_t val;
			int i, j=sensor->gain_reg_num_bits-1;
			int num_regs = (sensor->gain_reg_num_bits+7)>>3;

			for(i=0; i<num_regs; i++, j-=8)
			{
				val = (gain >> (j&~7)) & 0xFF;
				modReg(mode, sensor->gain_reg+i, 0, j&0x7, val, EQUAL);
				vcos_log_error("Set gain %04X to %02X", sensor->gain_reg+i, val);
			}
		}
	}
}

