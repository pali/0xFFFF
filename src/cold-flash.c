/*
    cold-flash.c - Cold flashing
    Copyright (C) 2011-2012  Pali Rohár <pali.rohar@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <usb.h>

#include "global.h"

#include "cold-flash.h"
#include "image.h"
#include "usb-device.h"
#include "printf-utils.h"

#define READ_DEV		0x81
#define WRITE_DEV		0x01
#define READ_TIMEOUT		500
#define WRITE_TIMEOUT		3000

static uint32_t tab[256];

static void crc32_gentab(void) {

	int i, j;
	uint32_t crc;
	uint32_t poly = 0xEDB88320L;

	for ( i = 0; i < 256; i++) {

		crc = i;

		for ( j = 8; j > 0; j-- ) {

			if (crc & 1)
				crc = (crc >> 1) ^ poly;
			else
				crc >>= 1;

		}

		tab[i] = crc;

	}

}

static uint32_t crc32(unsigned char * bytes, size_t size, uint32_t crc) {

	static int gen = 0;
	uint32_t i;

	if ( ! gen ) {
		crc32_gentab();
		gen = 1;
	}

	for ( i = 0; i < size; ++i )
		crc = (crc >> 8) ^ tab[(crc ^ bytes[i]) & 0xff];

	return crc;

}

/* Omap Boot Messages */
/* See spruf98v.pdf (page 3444): OMAP35x Technical Reference Manual - 25.4.5 Periheral Booting */

/* Omap Peripheral boot message */
static const uint32_t omap_peripheral_msg = 0xF0030002;

/* Omap Void (no device) boot message */
static const uint32_t omap_void_msg = 0xF0030006;

/* Omap XIP memory boot message */
static const uint32_t omap_xip_msg = 0xF0030106;

/* Omap Nand boot message */
static const uint32_t omap_nand_msg = 0xF0030206;

/* Omap OneNAND boot message */
static const uint32_t omap_onenand_msg = 0xF0030306;

/* Omap DOC boot message */
static const uint32_t omap_doc_msg = 0xF0030406;

/* Omap MMC/SD2 boot message */
static const uint32_t omap_mmc2_msg = 0xF0030506;

/* Omap MMC/SD1 boot message */
static const uint32_t omap_mmc1_msg = 0xF0030606;

/* Omap XIP memory with wait monitoring boot message */
static const uint32_t omap_xipwait_msg = 0xF0030706;

/* Omap UART boot message */
static const uint32_t omap_uart_msg = 0xF0031006;

/* Omap HS USB boot message */
static const uint32_t omap_hsusb_msg = 0xF0031106;

/* Omap next device boot message */
static const uint32_t omap_next_msg = 0xFFFFFFFF;

/* Omap memory boot message */
static const uint32_t omap_memory_msg = 0;


/* Nokia X-Loader messages */

/* Structure of X-Loader message */
struct xloader_msg {
	uint32_t type; /* 4 bytes - type of message */
	uint32_t size; /* 4 bytes - size of file */
	uint32_t crc1; /* 4 bytes - crc32 of file */
	uint32_t crc2; /* 4 bytes - crc32 of first 12 bytes of message */
};

#define XLOADER_MSG_TYPE_PING	0x6301326E
#define XLOADER_MSG_TYPE_SEND	0x6302326E

struct xloader_msg xloader_msg_create(uint32_t type, struct image * image) {

	struct xloader_msg msg;
	uint32_t need, readed;
	int ret;
	uint8_t buffer[1024];

	msg.type = type;
	msg.size = 0;
	msg.crc1 = 0;

	if ( image ) {
		msg.size = image->size;
		image_seek(image, 0);
		readed = 0;
		while ( readed < image->size ) {
			need = image->size - readed;
			if ( need > sizeof(buffer) )
				need = sizeof(buffer);
			ret = image_read(image, buffer, need);
			if ( ret == 0 )
				break;
			msg.crc1 = crc32(buffer, ret, msg.crc1);
			readed += ret;
		}
	}

	msg.crc2 = crc32((unsigned char *)&msg, 12, 0);

	return msg;

}

static int read_asic(usb_dev_handle * udev, uint8_t * asic_buffer, int size, int asic_size) {

	int ret;

	printf("Waiting for ASIC ID...\n");
	ret = usb_bulk_read(udev, READ_DEV, (char *)asic_buffer, size, READ_TIMEOUT);
	if ( ret != asic_size )
		ERROR_RETURN("Invalid size of ASIC ID", -1);

	return 0;

}

static int send_2nd(usb_dev_handle * udev, struct image * image) {

	uint8_t buffer[1024];
	uint32_t need, readed;
	int ret;

	printf("Sending OMAP peripheral boot message...\n");
	ret = usb_bulk_write(udev, WRITE_DEV, (char *)&omap_peripheral_msg, sizeof(omap_peripheral_msg), WRITE_TIMEOUT);
	usleep(5000);
	if ( ret != sizeof(omap_peripheral_msg) )
		ERROR_RETURN("Sending OMAP peripheral boot message failed", -1);

	printf("Sending 2nd X-Loader image size...\n");
	ret = usb_bulk_write(udev, WRITE_DEV, (char *)&image->size, 4, WRITE_TIMEOUT);
	usleep(5000);
	if ( ret != 4 )
		ERROR_RETURN("Sending 2nd X-Loader image size failed", -1);

	printf("Sending 2nd X-Loader image...\n");
	printf_progressbar(0, image->size);
	image_seek(image, 0);
	readed = 0;
	while ( readed < image->size ) {
		need = image->size - readed;
		if ( need > sizeof(buffer) )
			need = sizeof(buffer);
		ret = image_read(image, buffer, need);
		if ( ret == 0 )
			break;
		if ( usb_bulk_write(udev, WRITE_DEV, (char *)buffer, ret, WRITE_TIMEOUT) != ret )
			PRINTF_ERROR_RETURN("Sending 2nd X-Loader image failed", -1);
		readed += ret;
		printf_progressbar(readed, image->size);
	}
	usleep(50000);

	return 0;

}

static int send_secondary(usb_dev_handle * udev, struct image * image) {

	struct xloader_msg init_msg;
	uint8_t buffer[1024];
	uint32_t need, readed;
	int ret;

	init_msg = xloader_msg_create(XLOADER_MSG_TYPE_SEND, image);

	printf("Sending X-Loader init message...\n");
	ret = usb_bulk_write(udev, WRITE_DEV, (char *)&init_msg, sizeof(init_msg), WRITE_TIMEOUT);
	usleep(5000);
	if ( ret != sizeof(init_msg) )
		ERROR_RETURN("Sending X-Loader init message failed", -1);

	printf("Waiting for X-Loader response...\n");
	ret = usb_bulk_read(udev, READ_DEV, (char *)&buffer, 4, READ_TIMEOUT); /* 4 bytes - dummy value */
	if ( ret != 4 )
		ERROR_RETURN("No response", -1);

	printf("Sending Secondary image...\n");
	printf_progressbar(0, image->size);
	image_seek(image, 0);
	readed = 0;
	while ( readed < image->size ) {
		need = image->size - readed;
		if ( need > sizeof(buffer) )
			need = sizeof(buffer);
		ret = image_read(image, buffer, need);
		if ( ret == 0 )
			break;
		if ( usb_bulk_write(udev, WRITE_DEV, (char *)buffer, ret, WRITE_TIMEOUT) != ret )
			PRINTF_ERROR_RETURN("Sending Secondary image failed", -1);
		readed += ret;
		printf_progressbar(readed, image->size);
	}
	usleep(5000);

	printf("Waiting for X-Loader response...\n");
	ret = usb_bulk_read(udev, READ_DEV, (char *)&buffer, 4, READ_TIMEOUT); /* 4 bytes - dummy value */
	if ( ret != 4 )
		ERROR_RETURN("No response", -1);

	return 0;

}

static int ping_timeout(usb_dev_handle * udev) {

	int ret;
	int pong = 0;
	int try_ping = 10;

	while ( try_ping > 0 ) {

		struct xloader_msg ping_msg = xloader_msg_create(XLOADER_MSG_TYPE_PING, NULL);
		int try_read = 4;

		printf("Sending X-Loader ping message\n");
		ret = usb_bulk_write(udev, WRITE_DEV, (char *)&ping_msg, sizeof(ping_msg), WRITE_TIMEOUT);
		if ( ret != sizeof(ping_msg) )
			ERROR_RETURN("Sending X-Loader ping message faild", -1);

		printf("Waiting for X-Loader pong response...\n");
		while ( try_read > 0 ) {

			uint32_t ping_read;
			ret = usb_bulk_read(udev, READ_DEV, (char *)&ping_read, sizeof(ping_read), READ_TIMEOUT);
			if ( ret == sizeof(ping_read) ) {
				printf("Got it\n");
				pong = 1;
				break;
			}

			usleep(5000);
			--try_read;

		}

		if ( pong )
			break;

		printf("Responce timeout\n");
		--try_ping;

	}

	if (pong)
		return 0;
	else
		return -1;

}

int init_cold_flash(struct usb_device_info * dev) {

	uint8_t asic_buffer[127];
	int asic_size = 69;
	int i;

	if ( dev->flash_device->protocol != FLASH_COLD )
		ERROR_RETURN("Device is not in Cold Flash mode", -1);

	if ( read_asic(dev->udev, asic_buffer, sizeof(asic_buffer), asic_size) != 0 )
		ERROR_RETURN("Reading ASIC ID failed", -1);

	if ( verbose ) {
		printf("Got ASIC ID:");
		for ( i = 0; i < asic_size; ++i )
			printf(" 0x%.2X", (unsigned int)asic_buffer[i]);
		printf("\n");
	}

	/* TODO: Detect device from asic id */

	/* ASIC ID specification: http://processors.wiki.ti.com/index.php/OMAP35x_and_AM/DM37x_Initialization#UART.2FUSB_Booting */

	/* Number of subblocks */
	if ( asic_buffer[0] != 0x05 )
		ERROR_RETURN("Invalid ASIC ID", -1);

	/* ID Subblock - header */
	if ( memcmp(asic_buffer+1, "\x01\x05\x01", 3) != 0 )
		ERROR_RETURN("Invalid ASIC ID", -1);

	/* ID Subblock - OMAP chip version (check for OMAP35x) */
	if ( memcmp(asic_buffer+4, "\x34\x30\x07", 3) != 0 )
		ERROR_RETURN("Invalid ASIC ID", -1);

	/* Reserved1 - header */
	if ( memcmp(asic_buffer+8, "\x13\x02\x01", 3) != 0 )
		ERROR_RETURN("Invalid ASIC ID", -1);

	/* 2nd ID Subblock - header */
	if ( memcmp(asic_buffer+12, "\x12\x15\x01", 3) != 0 )
		ERROR_RETURN("Invalid ASIC ID", -1);

	/* Reserved2 - header */
	if ( memcmp(asic_buffer+35, "\x14\x15\x01", 3) != 0 )
		ERROR_RETURN("Invalid ASIC ID", -1);

	/* Checksum subblock - header */
	if ( memcmp(asic_buffer+58, "\x15\x09\x01", 3) != 0 )
		ERROR_RETURN("Invalid ASIC ID", -1);

	printf("Detected OMAP35x chip\n");

	return 0;

}

int cold_flash(struct usb_device_info * dev, struct image * x2nd, struct image * secondary) {

	if ( x2nd->type != IMAGE_2ND )
		ERROR_RETURN("Image type is not 2nd X-Loader", -1);

	if ( secondary->type != IMAGE_SECONDARY )
		ERROR_RETURN("Image type is not Secondary", -1);

	if ( send_2nd(dev->udev, x2nd) != 0 )
		ERROR_RETURN("Sending 2nd X-Loader image failed", -1);

	if ( ping_timeout(dev->udev) != 0 )
		ERROR_RETURN("Sending X-Loader ping message failed", -1);

	if ( send_secondary(dev->udev, secondary) != 0 )
		ERROR_RETURN("Sending Secondary image failed", -1);

	printf("Done\n");
	return 0;

}

int leave_cold_flash(struct usb_device_info * dev) {

	int ret;

	printf("Sending OMAP memory boot message...\n");
	ret = usb_bulk_write(dev->udev, WRITE_DEV, (char *)&omap_memory_msg, sizeof(omap_memory_msg), WRITE_TIMEOUT);
	usleep(5000);
	if ( ret != sizeof(omap_memory_msg) )
		ERROR_RETURN("Sending OMAP memory boot message failed", -1);

	usleep(500000);
	return 0;

}
