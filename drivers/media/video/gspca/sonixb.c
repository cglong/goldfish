/*
 *		sonix sn9c102 (bayer) library
 *		Copyright (C) 2003 2004 Michel Xhaard mxhaard@magic.fr
 * Add Pas106 Stefano Mozzi (C) 2004
 *
 * V4L2 by Jean-Francois Moine <http://moinejf.free.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#define MODULE_NAME "sonixb"

#include "gspca.h"

#define DRIVER_VERSION_NUMBER	KERNEL_VERSION(2, 1, 8)
static const char version[] = "2.1.8";

MODULE_AUTHOR("Michel Xhaard <mxhaard@users.sourceforge.net>");
MODULE_DESCRIPTION("GSPCA/SN9C102 USB Camera Driver");
MODULE_LICENSE("GPL");

/* specific webcam descriptor */
struct sd {
	struct gspca_dev gspca_dev;	/* !! must be the first item */

	struct sd_desc sd_desc;		/* our nctrls differ dependend upon the
					   sensor, so we use a per cam copy */
	atomic_t avg_lum;

	unsigned char gain;
	unsigned char exposure;
	unsigned char brightness;
	unsigned char autogain;
	unsigned char autogain_ignore_frames;
	unsigned char freq;		/* light freq filter setting */
	unsigned char saturation;
	unsigned char hue;
	unsigned char contrast;

	unsigned char fr_h_sz;		/* size of frame header */
	char sensor;			/* Type of image sensor chip */
#define SENSOR_HV7131R 0
#define SENSOR_OV6650 1
#define SENSOR_OV7630 2
#define SENSOR_OV7630_3 3
#define SENSOR_PAS106 4
#define SENSOR_PAS202 5
#define SENSOR_TAS5110 6
#define SENSOR_TAS5130CXX 7
	char sensor_has_gain;
	__u8 sensor_addr;
};

#define COMP2 0x8f
#define COMP 0xc7		/* 0x87 //0x07 */
#define COMP1 0xc9		/* 0x89 //0x09 */

#define MCK_INIT 0x63
#define MCK_INIT1 0x20		/*fixme: Bayer - 0x50 for JPEG ??*/

#define SYS_CLK 0x04

/* We calculate the autogain at the end of the transfer of a frame, at this
   moment a frame with the old settings is being transmitted, and a frame is
   being captured with the old settings. So if we adjust the autogain we must
   ignore atleast the 2 next frames for the new settings to come into effect
   before doing any other adjustments */
#define AUTOGAIN_IGNORE_FRAMES 3
#define AUTOGAIN_DEADZONE 1000
#define DESIRED_AVG_LUM 7000

/* V4L2 controls supported by the driver */
static int sd_setbrightness(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getbrightness(struct gspca_dev *gspca_dev, __s32 *val);
static int sd_setgain(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getgain(struct gspca_dev *gspca_dev, __s32 *val);
static int sd_setexposure(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getexposure(struct gspca_dev *gspca_dev, __s32 *val);
static int sd_setautogain(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getautogain(struct gspca_dev *gspca_dev, __s32 *val);
static int sd_setfreq(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getfreq(struct gspca_dev *gspca_dev, __s32 *val);
static int sd_setsaturation(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getsaturation(struct gspca_dev *gspca_dev, __s32 *val);
static int sd_sethue(struct gspca_dev *gspca_dev, __s32 val);
static int sd_gethue(struct gspca_dev *gspca_dev, __s32 *val);
static int sd_setcontrast(struct gspca_dev *gspca_dev, __s32 val);
static int sd_getcontrast(struct gspca_dev *gspca_dev, __s32 *val);

static struct ctrl sd_ctrls[] = {
	{
	    {
		.id      = V4L2_CID_BRIGHTNESS,
		.type    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Brightness",
		.minimum = 0,
		.maximum = 255,
		.step    = 1,
#define BRIGHTNESS_DEF 127
		.default_value = BRIGHTNESS_DEF,
	    },
	    .set = sd_setbrightness,
	    .get = sd_getbrightness,
	},
	{
	    {
		.id      = V4L2_CID_GAIN,
		.type    = V4L2_CTRL_TYPE_INTEGER,
		.name    = "Gain",
		.minimum = 0,
		.maximum = 255,
		.step    = 1,
#define GAIN_DEF 127
#define GAIN_KNEE 200
		.default_value = GAIN_DEF,
	    },
	    .set = sd_setgain,
	    .get = sd_getgain,
	},
	{
		{
			.id = V4L2_CID_EXPOSURE,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "Exposure",
#define EXPOSURE_DEF  16 /*  32 ms / 30 fps */
#define EXPOSURE_KNEE 50 /* 100 ms / 10 fps */
			.minimum = 0,
			.maximum = 255,
			.step = 1,
			.default_value = EXPOSURE_DEF,
			.flags = 0,
		},
		.set = sd_setexposure,
		.get = sd_getexposure,
	},
	{
		{
			.id = V4L2_CID_AUTOGAIN,
			.type = V4L2_CTRL_TYPE_BOOLEAN,
			.name = "Automatic Gain (and Exposure)",
			.minimum = 0,
			.maximum = 1,
			.step = 1,
#define AUTOGAIN_DEF 1
			.default_value = AUTOGAIN_DEF,
			.flags = 0,
		},
		.set = sd_setautogain,
		.get = sd_getautogain,
	},
	{
		{
			.id	 = V4L2_CID_POWER_LINE_FREQUENCY,
			.type    = V4L2_CTRL_TYPE_MENU,
			.name    = "Light frequency filter",
			.minimum = 0,
			.maximum = 2,	/* 0: 0, 1: 50Hz, 2:60Hz */
			.step    = 1,
#define FREQ_DEF 1
			.default_value = FREQ_DEF,
		},
		.set = sd_setfreq,
		.get = sd_getfreq,
	},
	{
		{
			.id      = V4L2_CID_SATURATION,
			.type    = V4L2_CTRL_TYPE_INTEGER,
			.name    = "Saturation",
			.minimum = 0,
			.maximum = 255,
			.step    = 1,
#define SATURATION_DEF 127
			.default_value = SATURATION_DEF,
		},
		.set = sd_setsaturation,
		.get = sd_getsaturation,
	},
	{
		{
			.id      = V4L2_CID_HUE,
			.type    = V4L2_CTRL_TYPE_INTEGER,
			.name    = "Hue",
			.minimum = 0,
			.maximum = 255,
			.step    = 1,
#define HUE_DEF 127
			.default_value = HUE_DEF,
		},
		.set = sd_sethue,
		.get = sd_gethue,
	},
	{
		{
			.id      = V4L2_CID_CONTRAST,
			.type    = V4L2_CTRL_TYPE_INTEGER,
			.name    = "Contrast",
			.minimum = 0,
			.maximum = 255,
			.step    = 1,
#define CONTRAST_DEF 127
			.default_value = CONTRAST_DEF,
		},
		.set = sd_setcontrast,
		.get = sd_getcontrast,
	},
};

static struct v4l2_pix_format vga_mode[] = {
	{160, 120, V4L2_PIX_FMT_SN9C10X, V4L2_FIELD_NONE,
		.bytesperline = 160,
		.sizeimage = 160 * 120,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 2},
	{320, 240, V4L2_PIX_FMT_SN9C10X, V4L2_FIELD_NONE,
		.bytesperline = 320,
		.sizeimage = 320 * 240,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 1},
	{640, 480, V4L2_PIX_FMT_SN9C10X, V4L2_FIELD_NONE,
		.bytesperline = 640,
		.sizeimage = 640 * 480,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 0},
};
static struct v4l2_pix_format sif_mode[] = {
	{176, 144, V4L2_PIX_FMT_SN9C10X, V4L2_FIELD_NONE,
		.bytesperline = 176,
		.sizeimage = 176 * 144,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 1},
	{352, 288, V4L2_PIX_FMT_SN9C10X, V4L2_FIELD_NONE,
		.bytesperline = 352,
		.sizeimage = 352 * 288,
		.colorspace = V4L2_COLORSPACE_SRGB,
		.priv = 0},
};

static const __u8 probe_ov7630[] = {0x08, 0x44};

static const __u8 initHv7131[] = {
	0x46, 0x77, 0x00, 0x04, 0x00, 0x00, 0x00, 0x80, 0x11, 0x00, 0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00, 0x00, 0x03, 0x01, 0x00,	/* shift from 0x02 0x01 0x00 */
	0x28, 0x1e, 0x60, 0x8a, 0x20,
	0x1d, 0x10, 0x02, 0x03, 0x0f, 0x0c
};
static const __u8 hv7131_sensor_init[][8] = {
	{0xc0, 0x11, 0x31, 0x38, 0x2a, 0x2e, 0x00, 0x10},
	{0xa0, 0x11, 0x01, 0x08, 0x2a, 0x2e, 0x00, 0x10},
	{0xb0, 0x11, 0x20, 0x00, 0xd0, 0x2e, 0x00, 0x10},
	{0xc0, 0x11, 0x25, 0x03, 0x0e, 0x28, 0x00, 0x16},
	{0xa0, 0x11, 0x30, 0x10, 0x0e, 0x28, 0x00, 0x15},
};
static const __u8 initOv6650[] = {
	0x44, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
	0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x02, 0x01, 0x0a, 0x16, 0x12, 0x68, 0x0b,
	0x10, 0x1d, 0x10, 0x00, 0x06, 0x1f, 0x00
};
static const __u8 ov6650_sensor_init[][8] =
{
	/* Bright, contrast, etc are set througth SCBB interface.
	 * AVCAP on win2 do not send any data on this 	controls. */
	/* Anyway, some registers appears to alter bright and constrat */

	/* Reset sensor */
	{0xa0, 0x60, 0x12, 0x80, 0x00, 0x00, 0x00, 0x10},
	/* Set clock register 0x11 low nibble is clock divider */
	{0xd0, 0x60, 0x11, 0xc0, 0x1b, 0x18, 0xc1, 0x10},
	/* Next some unknown stuff */
	{0xb0, 0x60, 0x15, 0x00, 0x02, 0x18, 0xc1, 0x10},
/*	{0xa0, 0x60, 0x1b, 0x01, 0x02, 0x18, 0xc1, 0x10},
		 * THIS SET GREEN SCREEN
		 * (pixels could be innverted in decode kind of "brg",
		 * but blue wont be there. Avoid this data ... */
	{0xd0, 0x60, 0x26, 0x01, 0x14, 0xd8, 0xa4, 0x10}, /* format out? */
	{0xd0, 0x60, 0x26, 0x01, 0x14, 0xd8, 0xa4, 0x10},
	{0xa0, 0x60, 0x30, 0x3d, 0x0A, 0xd8, 0xa4, 0x10},
	/* Enable rgb brightness control */
	{0xa0, 0x60, 0x61, 0x08, 0x00, 0x00, 0x00, 0x10},
	/* HDG: Note windows uses the line below, which sets both register 0x60
	   and 0x61 I believe these registers of the ov6650 are identical as
	   those of the ov7630, because if this is true the windows settings
	   add a bit additional red gain and a lot additional blue gain, which
	   matches my findings that the windows settings make blue much too
	   blue and red a little too red.
	{0xb0, 0x60, 0x60, 0x66, 0x68, 0xd8, 0xa4, 0x10}, */
	/* Some more unknown stuff */
	{0xa0, 0x60, 0x68, 0x04, 0x68, 0xd8, 0xa4, 0x10},
	{0xd0, 0x60, 0x17, 0x24, 0xd6, 0x04, 0x94, 0x10}, /* Clipreg */
};

static const __u8 initOv7630[] = {
	0x04, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,	/* r01 .. r08 */
	0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* r09 .. r10 */
	0x00, 0x02, 0x01, 0x0a,				/* r11 .. r14 */
	0x28, 0x1e,			/* H & V sizes     r15 .. r16 */
	0x68, COMP1, MCK_INIT1,				/* r17 .. r19 */
	0x1d, 0x10, 0x02, 0x03, 0x0f, 0x0c		/* r1a .. r1f */
};
static const __u8 initOv7630_3[] = {
	0x44, 0x44, 0x00, 0x1a, 0x20, 0x20, 0x20, 0x80,	/* r01 .. r08 */
	0x21, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,	/* r09 .. r10 */
	0x00, 0x01, 0x01, 0x0a,				/* r11 .. r14 */
	0x28, 0x1e,			/* H & V sizes     r15 .. r16 */
	0x68, 0x8f, MCK_INIT1,				/* r17 .. r19 */
	0x1d, 0x10, 0x02, 0x03, 0x0f, 0x0c, 0x00,	/* r1a .. r20 */
	0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70, 0x80, /* r21 .. r28 */
	0x90, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0, 0xf0, 0xff  /* r29 .. r30 */
};
static const __u8 ov7630_sensor_init_com[][8] = {
	{0xa0, 0x21, 0x12, 0x80, 0x00, 0x00, 0x00, 0x10},
	{0xb0, 0x21, 0x01, 0x77, 0x3a, 0x00, 0x00, 0x10},
/*	{0xd0, 0x21, 0x12, 0x7c, 0x01, 0x80, 0x34, 0x10},	   jfm */
	{0xd0, 0x21, 0x12, 0x1c, 0x00, 0x80, 0x34, 0x10},	/* jfm */
	{0xa0, 0x21, 0x1b, 0x04, 0x00, 0x80, 0x34, 0x10},
	{0xa0, 0x21, 0x20, 0x44, 0x00, 0x80, 0x34, 0x10},
	{0xa0, 0x21, 0x23, 0xee, 0x00, 0x80, 0x34, 0x10},
	{0xd0, 0x21, 0x26, 0xa0, 0x9a, 0xa0, 0x30, 0x10},
	{0xb0, 0x21, 0x2a, 0x80, 0x00, 0xa0, 0x30, 0x10},
	{0xb0, 0x21, 0x2f, 0x3d, 0x24, 0xa0, 0x30, 0x10},
	{0xa0, 0x21, 0x32, 0x86, 0x24, 0xa0, 0x30, 0x10},
	{0xb0, 0x21, 0x60, 0xa9, 0x4a, 0xa0, 0x30, 0x10},
/*	{0xb0, 0x21, 0x60, 0xa9, 0x42, 0xa0, 0x30, 0x10},	 * jfm */
	{0xa0, 0x21, 0x65, 0x00, 0x42, 0xa0, 0x30, 0x10},
	{0xa0, 0x21, 0x69, 0x38, 0x42, 0xa0, 0x30, 0x10},
	{0xc0, 0x21, 0x6f, 0x88, 0x0b, 0x00, 0x30, 0x10},
	{0xc0, 0x21, 0x74, 0x21, 0x8e, 0x00, 0x30, 0x10},
	{0xa0, 0x21, 0x7d, 0xf7, 0x8e, 0x00, 0x30, 0x10},
	{0xd0, 0x21, 0x17, 0x1c, 0xbd, 0x06, 0xf6, 0x10},
};
static const __u8 ov7630_sensor_init[][8] = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, /* delay 200ms */
	{0xa0, 0x21, 0x11, 0x01, 0xbd, 0x06, 0xf6, 0x10},	/* jfm */
	{0xa0, 0x21, 0x10, 0x57, 0xbd, 0x06, 0xf6, 0x16},
	{0xa0, 0x21, 0x76, 0x02, 0xbd, 0x06, 0xf6, 0x16},
	{0xa0, 0x21, 0x00, 0x10, 0xbd, 0x06, 0xf6, 0x15},	/* gain */
};
static const __u8 ov7630_sensor_init_3[][8] = {
	{0xa0, 0x21, 0x2a, 0xa0, 0x00, 0x00, 0x00, 0x10},
	{0xa0, 0x21, 0x2a, 0x80, 0x00, 0x00, 0x00, 0x10},
};

static const __u8 initPas106[] = {
	0x04, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81, 0x40, 0x00, 0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00, 0x00, 0x05, 0x01, 0x00,
	0x16, 0x12, 0x28, COMP1, MCK_INIT1,
	0x18, 0x10, 0x04, 0x03, 0x11, 0x0c
};
/* compression 0x86 mckinit1 0x2b */
static const __u8 pas106_data[][2] = {
	{0x02, 0x04},		/* Pixel Clock Divider 6 */
	{0x03, 0x13},		/* Frame Time MSB */
/*	{0x03, 0x12},		 * Frame Time MSB */
	{0x04, 0x06},		/* Frame Time LSB */
/*	{0x04, 0x05},		 * Frame Time LSB */
	{0x05, 0x65},		/* Shutter Time Line Offset */
/*	{0x05, 0x6d},		 * Shutter Time Line Offset */
/*	{0x06, 0xb1},		 * Shutter Time Pixel Offset */
	{0x06, 0xcd},		/* Shutter Time Pixel Offset */
	{0x07, 0xc1},		/* Black Level Subtract Sign */
/*	{0x07, 0x00},		 * Black Level Subtract Sign */
	{0x08, 0x06},		/* Black Level Subtract Level */
	{0x08, 0x06},		/* Black Level Subtract Level */
/*	{0x08, 0x01},		 * Black Level Subtract Level */
	{0x09, 0x05},		/* Color Gain B Pixel 5 a */
	{0x0a, 0x04},		/* Color Gain G1 Pixel 1 5 */
	{0x0b, 0x04},		/* Color Gain G2 Pixel 1 0 5 */
	{0x0c, 0x05},		/* Color Gain R Pixel 3 1 */
	{0x0d, 0x00},		/* Color GainH  Pixel */
	{0x0e, 0x0e},		/* Global Gain */
	{0x0f, 0x00},		/* Contrast */
	{0x10, 0x06},		/* H&V synchro polarity */
	{0x11, 0x06},		/* ?default */
	{0x12, 0x06},		/* DAC scale */
	{0x14, 0x02},		/* ?default */
	{0x13, 0x01},		/* Validate Settings */
};
static const __u8 initPas202[] = {
	0x44, 0x44, 0x21, 0x30, 0x00, 0x00, 0x00, 0x80, 0x40, 0x00, 0x00, 0x00,
	0x00, 0x00,
	0x00, 0x00, 0x00, 0x07, 0x03, 0x0a,	/* 6 */
	0x28, 0x1e, 0x28, 0x89, 0x30,
	0x00, 0x00, 0x02, 0x03, 0x0f, 0x0c
};
static const __u8 pas202_sensor_init[][8] = {
	{0xa0, 0x40, 0x02, 0x03, 0x00, 0x00, 0x00, 0x10},
	{0xd0, 0x40, 0x04, 0x07, 0x34, 0x00, 0x09, 0x10},
	{0xd0, 0x40, 0x08, 0x01, 0x00, 0x00, 0x01, 0x10},
	{0xd0, 0x40, 0x0C, 0x00, 0x0C, 0x00, 0x32, 0x10},
	{0xd0, 0x40, 0x10, 0x00, 0x01, 0x00, 0x63, 0x10},
	{0xa0, 0x40, 0x15, 0x70, 0x01, 0x00, 0x63, 0x10},
	{0xa0, 0x40, 0x18, 0x00, 0x01, 0x00, 0x63, 0x10},
	{0xa0, 0x40, 0x11, 0x01, 0x01, 0x00, 0x63, 0x10},
	{0xa0, 0x40, 0x03, 0x56, 0x01, 0x00, 0x63, 0x10},
	{0xa0, 0x40, 0x11, 0x01, 0x01, 0x00, 0x63, 0x10},
	{0xb0, 0x40, 0x04, 0x07, 0x2a, 0x00, 0x63, 0x10},
	{0xb0, 0x40, 0x0e, 0x00, 0x3d, 0x00, 0x63, 0x10},

	{0xa0, 0x40, 0x11, 0x01, 0x3d, 0x00, 0x63, 0x16},
	{0xa0, 0x40, 0x10, 0x08, 0x3d, 0x00, 0x63, 0x15},
	{0xa0, 0x40, 0x02, 0x04, 0x3d, 0x00, 0x63, 0x16},
	{0xa0, 0x40, 0x11, 0x01, 0x3d, 0x00, 0x63, 0x16},
	{0xb0, 0x40, 0x0e, 0x00, 0x31, 0x00, 0x63, 0x16},
	{0xa0, 0x40, 0x11, 0x01, 0x31, 0x00, 0x63, 0x16},
	{0xa0, 0x40, 0x10, 0x0e, 0x31, 0x00, 0x63, 0x15},
	{0xa0, 0x40, 0x11, 0x01, 0x31, 0x00, 0x63, 0x16},
};

static const __u8 initTas5110[] = {
	0x44, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x11, 0x00, 0x00, 0x00,
	0x00, 0x00,
	0x00, 0x01, 0x00, 0x46, 0x09, 0x0a,	/* shift from 0x45 0x09 0x0a */
	0x16, 0x12, 0x60, 0x86, 0x2b,
	0x14, 0x0a, 0x02, 0x02, 0x09, 0x07
};
static const __u8 tas5110_sensor_init[][8] = {
	{0x30, 0x11, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x10},
	{0x30, 0x11, 0x02, 0x20, 0xa9, 0x00, 0x00, 0x10},
	{0xa0, 0x61, 0x9a, 0xca, 0x00, 0x00, 0x00, 0x17},
};

static const __u8 initTas5130[] = {
	0x04, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x11, 0x00, 0x00, 0x00,
	0x00, 0x00,
	0x00, 0x01, 0x00, 0x69, 0x0c, 0x0a,
	0x28, 0x1e, 0x60, COMP, MCK_INIT,
	0x18, 0x10, 0x04, 0x03, 0x11, 0x0c
};
static const __u8 tas5130_sensor_init[][8] = {
/* 	{0x30, 0x11, 0x00, 0x40, 0x47, 0x00, 0x00, 0x10},
					* shutter 0x47 short exposure? */
	{0x30, 0x11, 0x00, 0x40, 0x01, 0x00, 0x00, 0x10},
					/* shutter 0x01 long exposure */
	{0x30, 0x11, 0x02, 0x20, 0x70, 0x00, 0x00, 0x10},
};

/* get one byte in gspca_dev->usb_buf */
static void reg_r(struct gspca_dev *gspca_dev,
		  __u16 value)
{
	usb_control_msg(gspca_dev->dev,
			usb_rcvctrlpipe(gspca_dev->dev, 0),
			0,			/* request */
			USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_INTERFACE,
			value,
			0,			/* index */
			gspca_dev->usb_buf, 1,
			500);
}

static void reg_w(struct gspca_dev *gspca_dev,
		  __u16 value,
		  const __u8 *buffer,
		  int len)
{
#ifdef CONFIG_VIDEO_ADV_DEBUG
	if (len > sizeof gspca_dev->usb_buf) {
		PDEBUG(D_ERR|D_PACK, "reg_w: buffer overflow");
		return;
	}
#endif
	memcpy(gspca_dev->usb_buf, buffer, len);
	usb_control_msg(gspca_dev->dev,
			usb_sndctrlpipe(gspca_dev->dev, 0),
			0x08,			/* request */
			USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_INTERFACE,
			value,
			0,			/* index */
			gspca_dev->usb_buf, len,
			500);
}

static void reg_w_big(struct gspca_dev *gspca_dev,
		  __u16 value,
		  const __u8 *buffer,
		  int len)
{
	__u8 *tmpbuf;

	tmpbuf = kmalloc(len, GFP_KERNEL);
	memcpy(tmpbuf, buffer, len);
	usb_control_msg(gspca_dev->dev,
			usb_sndctrlpipe(gspca_dev->dev, 0),
			0x08,			/* request */
			USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_INTERFACE,
			value,
			0,			/* index */
			tmpbuf, len,
			500);
	kfree(tmpbuf);
}

static int i2c_w(struct gspca_dev *gspca_dev, const __u8 *buffer)
{
	int retry = 60;

	/* is i2c ready */
	reg_w(gspca_dev, 0x08, buffer, 8);
	while (retry--) {
		msleep(10);
		reg_r(gspca_dev, 0x08);
		if (gspca_dev->usb_buf[0] & 0x04) {
			if (gspca_dev->usb_buf[0] & 0x08)
				return -1;
			return 0;
		}
	}
	return -1;
}

static void i2c_w_vector(struct gspca_dev *gspca_dev,
			const __u8 buffer[][8], int len)
{
	for (;;) {
		reg_w(gspca_dev, 0x08, *buffer, 8);
		len -= 8;
		if (len <= 0)
			break;
		buffer++;
	}
}

static void setbrightness(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	__u8 value;

	switch (sd->sensor) {
	case  SENSOR_OV6650:
	case  SENSOR_OV7630_3:
	case  SENSOR_OV7630: {
		__u8 i2cOV[] =
			{0xa0, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x10};

		/* change reg 0x06 */
		i2cOV[1] = sd->sensor_addr;
		i2cOV[3] = sd->brightness;
		if (i2c_w(gspca_dev, i2cOV) < 0)
			goto err;
		break;
	    }
	case SENSOR_PAS106: {
		__u8 i2c1[] =
			{0xa1, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14};

		i2c1[3] = sd->brightness >> 3;
		i2c1[2] = 0x0e;
		if (i2c_w(gspca_dev, i2c1) < 0)
			goto err;
		i2c1[3] = 0x01;
		i2c1[2] = 0x13;
		if (i2c_w(gspca_dev, i2c1) < 0)
			goto err;
		break;
	    }
	case SENSOR_PAS202: {
		/* __u8 i2cpexpo1[] =
			{0xb0, 0x40, 0x04, 0x07, 0x2a, 0x00, 0x63, 0x16}; */
		__u8 i2cpexpo[] =
			{0xb0, 0x40, 0x0e, 0x01, 0xab, 0x00, 0x63, 0x16};
		__u8 i2cp202[] =
			{0xa0, 0x40, 0x10, 0x0e, 0x31, 0x00, 0x63, 0x15};
		static __u8 i2cpdoit[] =
			{0xa0, 0x40, 0x11, 0x01, 0x31, 0x00, 0x63, 0x16};

		/* change reg 0x10 */
		i2cpexpo[4] = 0xff - sd->brightness;
/*		if(i2c_w(gspca_dev,i2cpexpo1) < 0)
			goto err; */
/*		if(i2c_w(gspca_dev,i2cpdoit) < 0)
			goto err; */
		if (i2c_w(gspca_dev, i2cpexpo) < 0)
			goto err;
		if (i2c_w(gspca_dev, i2cpdoit) < 0)
			goto err;
		i2cp202[3] = sd->brightness >> 3;
		if (i2c_w(gspca_dev, i2cp202) < 0)
			goto err;
		if (i2c_w(gspca_dev, i2cpdoit) < 0)
			goto err;
		break;
	    }
	case SENSOR_TAS5130CXX: {
		__u8 i2c[] =
			{0x30, 0x11, 0x02, 0x20, 0x70, 0x00, 0x00, 0x10};

		value = 0xff - sd->brightness;
		i2c[4] = value;
		PDEBUG(D_CONF, "brightness %d : %d", value, i2c[4]);
		if (i2c_w(gspca_dev, i2c) < 0)
			goto err;
		break;
	    }
	case SENSOR_TAS5110:
		/* FIXME figure out howto control brightness on TAS5110 */
		break;
	}
	return;
err:
	PDEBUG(D_ERR, "i2c error brightness");
}

static void setsensorgain(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	unsigned char gain = sd->gain;

	switch (sd->sensor) {

	case SENSOR_TAS5110: {
		__u8 i2c[] =
			{0x30, 0x11, 0x02, 0x20, 0x70, 0x00, 0x00, 0x10};

		i2c[4] = 255 - gain;
		if (i2c_w(gspca_dev, i2c) < 0)
			goto err;
		break;
	    }

	case SENSOR_OV6650:
		gain >>= 1;
		/* fall thru */
	case SENSOR_OV7630_3: {
		__u8 i2c[] = {0xa0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10};

		i2c[1] = sd->sensor_addr;
		i2c[3] = gain >> 2;
		if (i2c_w(gspca_dev, i2c) < 0)
			goto err;
		break;
	    }
	}
	return;
err:
	PDEBUG(D_ERR, "i2c error gain");
}

static void setgain(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	__u8 gain;
	__u8 rgb_value;

	gain = sd->gain >> 4;

	/* red and blue gain */
	rgb_value = gain << 4 | gain;
	reg_w(gspca_dev, 0x10, &rgb_value, 1);
	/* green gain */
	rgb_value = gain;
	reg_w(gspca_dev, 0x11, &rgb_value, 1);

	if (sd->sensor_has_gain)
		setsensorgain(gspca_dev);
}

static void setexposure(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	switch (sd->sensor) {
	case SENSOR_TAS5110: {
		__u8 reg;

		/* register 19's high nibble contains the sn9c10x clock divider
		   The high nibble configures the no fps according to the
		   formula: 60 / high_nibble. With a maximum of 30 fps */
		reg = 120 * sd->exposure / 1000;
		if (reg < 2)
			reg = 2;
		else if (reg > 15)
			reg = 15;
		reg = (reg << 4) | 0x0b;
		reg_w(gspca_dev, 0x19, &reg, 1);
		break;
	    }
	case SENSOR_OV6650:
	case SENSOR_OV7630_3: {
		/* The ov6650 / ov7630 have 2 registers which both influence
		   exposure, register 11, whose low nibble sets the nr off fps
		   according to: fps = 30 / (low_nibble + 1)

		   The fps configures the maximum exposure setting, but it is
		   possible to use less exposure then what the fps maximum
		   allows by setting register 10. register 10 configures the
		   actual exposure as quotient of the full exposure, with 0
		   being no exposure at all (not very usefull) and reg10_max
		   being max exposure possible at that framerate.

		   The code maps our 0 - 510 ms exposure ctrl to these 2
		   registers, trying to keep fps as high as possible.
		*/
		__u8 i2c[] = {0xb0, 0x00, 0x10, 0x00, 0xc0, 0x00, 0x00, 0x10};
		int reg10, reg11;
		/* ov6645 datasheet says reg10_max is 9a, but that uses
		   tline * 2 * reg10 as formula for calculating texpo, the
		   ov6650 probably uses the same formula as the 7730 which uses
		   tline * 4 * reg10, which explains why the reg10max we've
		   found experimentally for the ov6650 is exactly half that of
		   the ov6645. The ov7630 datasheet says the max is 0x41. */
		const int reg10_max = (sd->sensor == SENSOR_OV6650)
				? 0x4d : 0x41;

		reg11 = (60 * sd->exposure + 999) / 1000;
		if (reg11 < 1)
			reg11 = 1;
		else if (reg11 > 16)
			reg11 = 16;

		/* frame exposure time in ms = 1000 * reg11 / 30    ->
		reg10 = sd->exposure * 2 * reg10_max / (1000 * reg11 / 30) */
		reg10 = (sd->exposure * 60 * reg10_max) / (1000 * reg11);

		/* Don't allow this to get below 10 when using autogain, the
		   steps become very large (relatively) when below 10 causing
		   the image to oscilate from much too dark, to much too bright
		   and back again. */
		if (sd->autogain && reg10 < 10)
			reg10 = 10;
		else if (reg10 > reg10_max)
			reg10 = reg10_max;

		/* Write reg 10 and reg11 low nibble */
		i2c[1] = sd->sensor_addr;
		i2c[3] = reg10;
		i2c[4] |= reg11 - 1;
		if (sd->sensor == SENSOR_OV7630_3) {
			__u8 reg76 = reg10 & 0x03;
			__u8 i2c_reg76[] = {0xa0, 0x21, 0x76, 0x00,
					    0x00, 0x00, 0x00, 0x10};
			reg10 >>= 2;
			i2c_reg76[3] = reg76;
			if (i2c_w(gspca_dev, i2c_reg76) < 0)
				PDEBUG(D_ERR, "i2c error exposure");
		}
		if (i2c_w(gspca_dev, i2c) < 0)
			PDEBUG(D_ERR, "i2c error exposure");
		break;
	    }
	}
}

static void setfreq(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	switch (sd->sensor) {
	case SENSOR_OV6650:
	case SENSOR_OV7630_3: {
		/* Framerate adjust register for artificial light 50 hz flicker
		   compensation, identical to ov6630 0x2b register, see ov6630
		   datasheet.
		   0x4f -> (30 fps -> 25 fps), 0x00 -> no adjustment */
		__u8 i2c[] = {0xa0, 0x00, 0x2b, 0x00, 0x00, 0x00, 0x00, 0x10};
		switch (sd->freq) {
		default:
/*		case 0:			 * no filter*/
/*		case 2:			 * 60 hz */
			i2c[3] = 0;
			break;
		case 1:			/* 50 hz */
			i2c[3] = (sd->sensor == SENSOR_OV6650)
					? 0x4f : 0x8a;
			break;
		}
		i2c[1] = sd->sensor_addr;
		if (i2c_w(gspca_dev, i2c) < 0)
			PDEBUG(D_ERR, "i2c error setfreq");
		break;
	    }
	}
}

static void setsaturation(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	switch (sd->sensor) {
/*	case SENSOR_OV6650: */
	case SENSOR_OV7630_3:
	case SENSOR_OV7630: {
		__u8 i2c[] = {0xa0, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x10};
		i2c[1] = sd->sensor_addr;
		i2c[3] = sd->saturation & 0xf0;
		if (i2c_w(gspca_dev, i2c) < 0)
			PDEBUG(D_ERR, "i2c error setsaturation");
		else
			PDEBUG(D_CONF, "saturation set to: %d",
				(int)sd->saturation);
		break;
	    }
	}
}

static void sethue(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	switch (sd->sensor) {
/*	case SENSOR_OV6650: */
	case SENSOR_OV7630_3:
	case SENSOR_OV7630: {
		__u8 i2c[] = {0xa0, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x10};
		i2c[1] = sd->sensor_addr;
		i2c[3] = 0x20 | (sd->hue >> 3);
		if (i2c_w(gspca_dev, i2c) < 0)
			PDEBUG(D_ERR, "i2c error setsaturation");
		else
			PDEBUG(D_CONF, "hue set to: %d", (int)sd->hue);
		break;
	    }
	}
}

static void setcontrast(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;

	switch (sd->sensor) {
/*	case SENSOR_OV6650: */
	case SENSOR_OV7630_3:
	case SENSOR_OV7630: {
		__u8 i2c[] = {0xa0, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x10};
		i2c[1] = sd->sensor_addr;
		i2c[3] = 0x20 | (sd->contrast >> 3);
		if (i2c_w(gspca_dev, i2c) < 0)
			PDEBUG(D_ERR, "i2c error setcontrast");
		else
			PDEBUG(D_CONF, "contrast set to: %d",
				(int)sd->contrast);
		break;
	    }
	}
}


static void do_autogain(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	int avg_lum = atomic_read(&sd->avg_lum);

	if (avg_lum == -1)
		return;

	if (sd->autogain_ignore_frames > 0)
		sd->autogain_ignore_frames--;
	else if (gspca_auto_gain_n_exposure(gspca_dev, avg_lum,
			sd->brightness * DESIRED_AVG_LUM / 127,
			AUTOGAIN_DEADZONE, GAIN_KNEE, EXPOSURE_KNEE)) {
		PDEBUG(D_FRAM, "autogain: gain changed: gain: %d expo: %d\n",
			(int)sd->gain, (int)sd->exposure);
		sd->autogain_ignore_frames = AUTOGAIN_IGNORE_FRAMES;
	}
}

/* this function is called at probe time */
static int sd_config(struct gspca_dev *gspca_dev,
			const struct usb_device_id *id)
{
	struct sd *sd = (struct sd *) gspca_dev;
	struct cam *cam;
	__u16 product;
	int sif = 0;

	/* nctrls depends upon the sensor, so we use a per cam copy */
	memcpy(&sd->sd_desc, gspca_dev->sd_desc, sizeof(struct sd_desc));
	gspca_dev->sd_desc = &sd->sd_desc;

	sd->fr_h_sz = 12;		/* default size of the frame header */
	sd->sd_desc.nctrls = 2;		/* default nb of ctrls */
	sd->autogain = AUTOGAIN_DEF;    /* default is autogain active */

	product = id->idProduct;
/*	switch (id->idVendor) { */
/*	case 0x0c45:				 * Sonix */
		switch (product) {
		case 0x6001:			/* SN9C102 */
		case 0x6005:			/* SN9C101 */
		case 0x6007:			/* SN9C101 */
			sd->sensor = SENSOR_TAS5110;
			sd->sensor_has_gain = 1;
			sd->sd_desc.nctrls = 4;
			sd->sd_desc.dq_callback = do_autogain;
			sif = 1;
			break;
		case 0x6009:			/* SN9C101 */
		case 0x600d:			/* SN9C101 */
		case 0x6029:			/* SN9C101 */
			sd->sensor = SENSOR_PAS106;
			sif = 1;
			break;
		case 0x6011:			/* SN9C101 - SN9C101G */
			sd->sensor = SENSOR_OV6650;
			sd->sensor_has_gain = 1;
			sd->sensor_addr = 0x60;
			sd->sd_desc.nctrls = 5;
			sd->sd_desc.dq_callback = do_autogain;
			sif = 1;
			break;
		case 0x6019:			/* SN9C101 */
		case 0x602c:			/* SN9C102 */
		case 0x602e:			/* SN9C102 */
			sd->sensor = SENSOR_OV7630;
			sd->sensor_addr = 0x21;
			break;
		case 0x60b0:			/* SN9C103 */
			sd->sensor = SENSOR_OV7630_3;
			sd->sensor_addr = 0x21;
			sd->fr_h_sz = 18;	/* size of frame header */
			sd->sensor_has_gain = 1;
			sd->sd_desc.nctrls = 8;
			sd->sd_desc.dq_callback = do_autogain;
			sd->autogain = 0;
			break;
		case 0x6024:			/* SN9C102 */
		case 0x6025:			/* SN9C102 */
			sd->sensor = SENSOR_TAS5130CXX;
			break;
		case 0x6028:			/* SN9C102 */
			sd->sensor = SENSOR_PAS202;
			break;
		case 0x602d:			/* SN9C102 */
			sd->sensor = SENSOR_HV7131R;
			break;
		case 0x60af:			/* SN9C103 */
			sd->sensor = SENSOR_PAS202;
			sd->fr_h_sz = 18;	/* size of frame header (?) */
			break;
		}
/*		break; */
/*	} */

	cam = &gspca_dev->cam;
	cam->dev_name = (char *) id->driver_info;
	cam->epaddr = 0x01;
	if (!sif) {
		cam->cam_mode = vga_mode;
		cam->nmodes = ARRAY_SIZE(vga_mode);
		if (sd->sensor == SENSOR_OV7630_3) {
			/* We only have 320x240 & 640x480 */
			cam->cam_mode++;
			cam->nmodes--;
		}
	} else {
		cam->cam_mode = sif_mode;
		cam->nmodes = ARRAY_SIZE(sif_mode);
	}
	sd->brightness = BRIGHTNESS_DEF;
	sd->gain = GAIN_DEF;
	sd->exposure = EXPOSURE_DEF;
	sd->freq = FREQ_DEF;
	sd->contrast = CONTRAST_DEF;
	sd->saturation = SATURATION_DEF;
	sd->hue = HUE_DEF;
	if (sd->sensor == SENSOR_OV7630_3)	/* jfm: from win trace */
		reg_w(gspca_dev, 0x01, probe_ov7630, sizeof probe_ov7630);
	return 0;
}

/* this function is called at open time */
static int sd_open(struct gspca_dev *gspca_dev)
{
	reg_r(gspca_dev, 0x00);
	if (gspca_dev->usb_buf[0] != 0x10)
		return -ENODEV;
	return 0;
}

static void pas106_i2cinit(struct gspca_dev *gspca_dev)
{
	int i;
	const __u8 *data;
	__u8 i2c1[] = { 0xa1, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14 };

	i = ARRAY_SIZE(pas106_data);
	data = pas106_data[0];
	while (--i >= 0) {
		memcpy(&i2c1[2], data, 2);
					/* copy 2 bytes from the template */
		if (i2c_w(gspca_dev, i2c1) < 0)
			PDEBUG(D_ERR, "i2c error pas106");
		data += 2;
	}
}

/* -- start the camera -- */
static void sd_start(struct gspca_dev *gspca_dev)
{
	struct sd *sd = (struct sd *) gspca_dev;
	int mode, l;
	const __u8 *sn9c10x;
	__u8 reg01, reg17;
	__u8 reg17_19[3];

	mode = gspca_dev->cam.cam_mode[(int) gspca_dev->curr_mode].priv;
	switch (sd->sensor) {
	case SENSOR_HV7131R:
		sn9c10x = initHv7131;
		reg17_19[0] = 0x60;
		reg17_19[1] = (mode << 4) | 0x8a;
		reg17_19[2] = 0x20;
		break;
	case SENSOR_OV6650:
		sn9c10x = initOv6650;
		reg17_19[0] = 0x68;
		reg17_19[1] = (mode << 4) | 0x8b;
		reg17_19[2] = 0x20;
		break;
	case SENSOR_OV7630:
		sn9c10x = initOv7630;
		reg17_19[0] = 0x68;
		reg17_19[1] = (mode << 4) | COMP2;
		reg17_19[2] = MCK_INIT1;
		break;
	case SENSOR_OV7630_3:
		sn9c10x = initOv7630_3;
		reg17_19[0] = 0x68;
		reg17_19[1] = (mode << 4) | COMP2;
		reg17_19[2] = MCK_INIT1;
		break;
	case SENSOR_PAS106:
		sn9c10x = initPas106;
		reg17_19[0] = 0x24;		/* 0x28 */
		reg17_19[1] = (mode << 4) | COMP1;
		reg17_19[2] = MCK_INIT1;
		break;
	case SENSOR_PAS202:
		sn9c10x = initPas202;
		reg17_19[0] = mode ? 0x24 : 0x20;
		reg17_19[1] = (mode << 4) | 0x89;
		reg17_19[2] = 0x20;
		break;
	case SENSOR_TAS5110:
		sn9c10x = initTas5110;
		reg17_19[0] = 0x60;
		reg17_19[1] = (mode << 4) | 0x86;
		reg17_19[2] = 0x2b;		/* 0xf3; */
		break;
	default:
/*	case SENSOR_TAS5130CXX: */
		sn9c10x = initTas5130;
		reg17_19[0] = 0x60;
		reg17_19[1] = (mode << 4) | COMP;
		reg17_19[2] = mode ? 0x23 : 0x43;
		break;
	}
	switch (sd->sensor) {
	case SENSOR_OV7630:
		reg01 = 0x06;
		reg17 = 0x29;
		l = sizeof initOv7630;
		break;
	case SENSOR_OV7630_3:
		reg01 = 0x44;
		reg17 = 0x68;
		l = sizeof initOv7630_3;
		break;
	default:
		reg01 = sn9c10x[0];
		reg17 = sn9c10x[0x17 - 1];
		l = 0x1f;
		break;
	}

	/* reg 0x01 bit 2 video transfert on */
	reg_w(gspca_dev, 0x01, &reg01, 1);
	/* reg 0x17 SensorClk enable inv Clk 0x60 */
	reg_w(gspca_dev, 0x17, &reg17, 1);
/*fixme: for ov7630 102
	reg_w(gspca_dev, 0x01, {0x06, sn9c10x[1]}, 2); */
	/* Set the registers from the template */
	reg_w_big(gspca_dev, 0x01, sn9c10x, l);
	switch (sd->sensor) {
	case SENSOR_HV7131R:
		i2c_w_vector(gspca_dev, hv7131_sensor_init,
				sizeof hv7131_sensor_init);
		break;
	case SENSOR_OV6650:
		i2c_w_vector(gspca_dev, ov6650_sensor_init,
				sizeof ov6650_sensor_init);
		break;
	case SENSOR_OV7630:
		i2c_w_vector(gspca_dev, ov7630_sensor_init_com,
				sizeof ov7630_sensor_init_com);
		msleep(200);
		i2c_w_vector(gspca_dev, ov7630_sensor_init,
				sizeof ov7630_sensor_init);
		break;
	case SENSOR_OV7630_3:
		i2c_w_vector(gspca_dev, ov7630_sensor_init_com,
				sizeof ov7630_sensor_init_com);
		msleep(200);
		i2c_w(gspca_dev, ov7630_sensor_init_3[mode]);
		break;
	case SENSOR_PAS106:
		pas106_i2cinit(gspca_dev);
		break;
	case SENSOR_PAS202:
		i2c_w_vector(gspca_dev, pas202_sensor_init,
				sizeof pas202_sensor_init);
		break;
	case SENSOR_TAS5110:
		i2c_w_vector(gspca_dev, tas5110_sensor_init,
				sizeof tas5110_sensor_init);
		break;
	default:
/*	case SENSOR_TAS5130CXX: */
		i2c_w_vector(gspca_dev, tas5130_sensor_init,
				sizeof tas5130_sensor_init);
		break;
	}
	/* H_size V_size 0x28, 0x1e -> 640x480. 0x16, 0x12 -> 352x288 */
	reg_w(gspca_dev, 0x15, &sn9c10x[0x15 - 1], 2);
	/* compression register */
	reg_w(gspca_dev, 0x18, &reg17_19[1], 1);
	/* H_start */
	reg_w(gspca_dev, 0x12, &sn9c10x[0x12 - 1], 1);
	/* V_START */
	reg_w(gspca_dev, 0x13, &sn9c10x[0x13 - 1], 1);
	/* reset 0x17 SensorClk enable inv Clk 0x60 */
				/*fixme: ov7630 [17]=68 8f (+20 if 102)*/
	reg_w(gspca_dev, 0x17, &reg17_19[0], 1);
	/*MCKSIZE ->3 */	/*fixme: not ov7630*/
	reg_w(gspca_dev, 0x19, &reg17_19[2], 1);
	/* AE_STRX AE_STRY AE_ENDX AE_ENDY */
	reg_w(gspca_dev, 0x1c, &sn9c10x[0x1c - 1], 4);
	/* Enable video transfert */
	reg_w(gspca_dev, 0x01, &sn9c10x[0], 1);
	/* Compression */
	reg_w(gspca_dev, 0x18, &reg17_19[1], 2);
	msleep(20);

	setgain(gspca_dev);
	setbrightness(gspca_dev);
	setexposure(gspca_dev);
	setfreq(gspca_dev);
	setsaturation(gspca_dev);
	sethue(gspca_dev);
	setcontrast(gspca_dev);

	sd->autogain_ignore_frames = 0;
	atomic_set(&sd->avg_lum, -1);
}

static void sd_stopN(struct gspca_dev *gspca_dev)
{
	__u8 ByteSend;

	ByteSend = 0x09;	/* 0X00 */
	reg_w(gspca_dev, 0x01, &ByteSend, 1);
}

static void sd_stop0(struct gspca_dev *gspca_dev)
{
}

static void sd_close(struct gspca_dev *gspca_dev)
{
}

static void sd_pkt_scan(struct gspca_dev *gspca_dev,
			struct gspca_frame *frame,	/* target */
			unsigned char *data,		/* isoc packet */
			int len)			/* iso packet length */
{
	int i;
	struct sd *sd = (struct sd *) gspca_dev;

	/* frames start with:
	 *	ff ff 00 c4 c4 96	synchro
	 *	00		(unknown)
	 *	xx		(frame sequence / size / compression)
	 *	(xx)		(idem - extra byte for sn9c103)
	 *	ll mm		brightness sum inside auto exposure
	 *	ll mm		brightness sum outside auto exposure
	 *	(xx xx xx xx xx)	audio values for snc103
	 */
	if (len > 6 && len < 24) {
		for (i = 0; i < len - 6; i++) {
			if (data[0 + i] == 0xff
			    && data[1 + i] == 0xff
			    && data[2 + i] == 0x00
			    && data[3 + i] == 0xc4
			    && data[4 + i] == 0xc4
			    && data[5 + i] == 0x96) {	/* start of frame */
				frame = gspca_frame_add(gspca_dev, LAST_PACKET,
							frame, data, 0);
				if (len - i < sd->fr_h_sz) {
					atomic_set(&sd->avg_lum, -1);
					PDEBUG(D_STREAM, "packet too short to"
						" get avg brightness");
				} else if (sd->fr_h_sz == 12) {
					atomic_set(&sd->avg_lum,
						data[i + 8] +
							(data[i + 9] << 8));
				} else {
					atomic_set(&sd->avg_lum,
						data[i + 9] +
							(data[i + 10] << 8));
				}
				data += i + sd->fr_h_sz;
				len -= i + sd->fr_h_sz;
				gspca_frame_add(gspca_dev, FIRST_PACKET,
						frame, data, len);
				return;
			}
		}
	}
	gspca_frame_add(gspca_dev, INTER_PACKET,
			frame, data, len);
}

static int sd_setbrightness(struct gspca_dev *gspca_dev, __s32 val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	sd->brightness = val;
	if (gspca_dev->streaming)
		setbrightness(gspca_dev);
	return 0;
}

static int sd_getbrightness(struct gspca_dev *gspca_dev, __s32 *val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	*val = sd->brightness;
	return 0;
}

static int sd_setgain(struct gspca_dev *gspca_dev, __s32 val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	sd->gain = val;
	if (gspca_dev->streaming)
		setgain(gspca_dev);
	return 0;
}

static int sd_getgain(struct gspca_dev *gspca_dev, __s32 *val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	*val = sd->gain;
	return 0;
}

static int sd_setexposure(struct gspca_dev *gspca_dev, __s32 val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	sd->exposure = val;
	if (gspca_dev->streaming)
		setexposure(gspca_dev);
	return 0;
}

static int sd_getexposure(struct gspca_dev *gspca_dev, __s32 *val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	*val = sd->exposure;
	return 0;
}

static int sd_setautogain(struct gspca_dev *gspca_dev, __s32 val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	sd->autogain = val;
	/* when switching to autogain set defaults to make sure
	   we are on a valid point of the autogain gain /
	   exposure knee graph, and give this change time to
	   take effect before doing autogain. */
	if (sd->autogain) {
		sd->exposure = EXPOSURE_DEF;
		sd->gain = GAIN_DEF;
		if (gspca_dev->streaming) {
			sd->autogain_ignore_frames = AUTOGAIN_IGNORE_FRAMES;
			setexposure(gspca_dev);
			setgain(gspca_dev);
		}
	}

	return 0;
}

static int sd_getautogain(struct gspca_dev *gspca_dev, __s32 *val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	*val = sd->autogain;
	return 0;
}

static int sd_setfreq(struct gspca_dev *gspca_dev, __s32 val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	sd->freq = val;
	if (gspca_dev->streaming)
		setfreq(gspca_dev);
	return 0;
}

static int sd_getfreq(struct gspca_dev *gspca_dev, __s32 *val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	*val = sd->freq;
	return 0;
}

static int sd_setsaturation(struct gspca_dev *gspca_dev, __s32 val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	sd->saturation = val;
	if (gspca_dev->streaming)
		setsaturation(gspca_dev);
	return 0;
}

static int sd_getsaturation(struct gspca_dev *gspca_dev, __s32 *val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	*val = sd->saturation;
	return 0;
}

static int sd_sethue(struct gspca_dev *gspca_dev, __s32 val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	sd->hue = val;
	if (gspca_dev->streaming)
		sethue(gspca_dev);
	return 0;
}

static int sd_gethue(struct gspca_dev *gspca_dev, __s32 *val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	*val = sd->hue;
	return 0;
}

static int sd_setcontrast(struct gspca_dev *gspca_dev, __s32 val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	sd->contrast = val;
	if (gspca_dev->streaming)
		setcontrast(gspca_dev);
	return 0;
}

static int sd_getcontrast(struct gspca_dev *gspca_dev, __s32 *val)
{
	struct sd *sd = (struct sd *) gspca_dev;

	*val = sd->contrast;
	return 0;
}

static int sd_querymenu(struct gspca_dev *gspca_dev,
			struct v4l2_querymenu *menu)
{
	switch (menu->id) {
	case V4L2_CID_POWER_LINE_FREQUENCY:
		switch (menu->index) {
		case 0:		/* V4L2_CID_POWER_LINE_FREQUENCY_DISABLED */
			strcpy((char *) menu->name, "NoFliker");
			return 0;
		case 1:		/* V4L2_CID_POWER_LINE_FREQUENCY_50HZ */
			strcpy((char *) menu->name, "50 Hz");
			return 0;
		case 2:		/* V4L2_CID_POWER_LINE_FREQUENCY_60HZ */
			strcpy((char *) menu->name, "60 Hz");
			return 0;
		}
		break;
	}
	return -EINVAL;
}

/* sub-driver description */
static const struct sd_desc sd_desc = {
	.name = MODULE_NAME,
	.ctrls = sd_ctrls,
	.nctrls = ARRAY_SIZE(sd_ctrls),
	.config = sd_config,
	.open = sd_open,
	.start = sd_start,
	.stopN = sd_stopN,
	.stop0 = sd_stop0,
	.close = sd_close,
	.pkt_scan = sd_pkt_scan,
	.querymenu = sd_querymenu,
};

/* -- module initialisation -- */
#define DVNM(name) .driver_info = (kernel_ulong_t) name
static __devinitdata struct usb_device_id device_table[] = {
#ifndef CONFIG_USB_SN9C102
	{USB_DEVICE(0x0c45, 0x6001), DVNM("Genius VideoCAM NB")},
	{USB_DEVICE(0x0c45, 0x6005), DVNM("Sweex Tas5110")},
	{USB_DEVICE(0x0c45, 0x6007), DVNM("Sonix sn9c101 + Tas5110D")},
	{USB_DEVICE(0x0c45, 0x6009), DVNM("spcaCam@120")},
	{USB_DEVICE(0x0c45, 0x600d), DVNM("spcaCam@120")},
#endif
	{USB_DEVICE(0x0c45, 0x6011), DVNM("MAX Webcam Microdia")},
#ifndef CONFIG_USB_SN9C102
	{USB_DEVICE(0x0c45, 0x6019), DVNM("Generic Sonix OV7630")},
	{USB_DEVICE(0x0c45, 0x6024), DVNM("Generic Sonix Tas5130c")},
	{USB_DEVICE(0x0c45, 0x6025), DVNM("Xcam Shanga")},
	{USB_DEVICE(0x0c45, 0x6028), DVNM("Sonix Btc Pc380")},
	{USB_DEVICE(0x0c45, 0x6029), DVNM("spcaCam@150")},
	{USB_DEVICE(0x0c45, 0x602c), DVNM("Generic Sonix OV7630")},
	{USB_DEVICE(0x0c45, 0x602d), DVNM("LIC-200 LG")},
	{USB_DEVICE(0x0c45, 0x602e), DVNM("Genius VideoCam Messenger")},
	{USB_DEVICE(0x0c45, 0x60af), DVNM("Trust WB3100P")},
	{USB_DEVICE(0x0c45, 0x60b0), DVNM("Genius VideoCam Look")},
#endif
	{}
};
MODULE_DEVICE_TABLE(usb, device_table);

/* -- device connect -- */
static int sd_probe(struct usb_interface *intf,
			const struct usb_device_id *id)
{
	return gspca_dev_probe(intf, id, &sd_desc, sizeof(struct sd),
				THIS_MODULE);
}

static struct usb_driver sd_driver = {
	.name = MODULE_NAME,
	.id_table = device_table,
	.probe = sd_probe,
	.disconnect = gspca_disconnect,
};

/* -- module insert / remove -- */
static int __init sd_mod_init(void)
{
	if (usb_register(&sd_driver) < 0)
		return -1;
	PDEBUG(D_PROBE, "v%s registered", version);
	return 0;
}
static void __exit sd_mod_exit(void)
{
	usb_deregister(&sd_driver);
	PDEBUG(D_PROBE, "deregistered");
}

module_init(sd_mod_init);
module_exit(sd_mod_exit);