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

#include "image.h"

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

/* Omap OneNAND boot message */
static const uint32_t omap_onenand_msg = 0xF0030306;

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
			if ( ret < 0 )
				break;
			msg.crc1 = crc32(buffer, ret, msg.crc1);
			readed += ret;
		}
	}

	msg.crc2 = crc32((unsigned char *)&msg, 12, 0);

	return msg;

}

static int read_asic(usb_dev_handle * udev) {

	uint8_t asic_buffer[127];
	int asic_size = 69;
	int i, ret;

	printf("Waiting for ASIC ID...\n");
	ret = usb_bulk_read(udev, READ_DEV, (char *)asic_buffer, sizeof(asic_buffer), READ_TIMEOUT);
	if ( ret != asic_size ) {
		fprintf(stderr, "Invalid size of ASIC ID\n");
		return 0;
	}

	printf("Got ASIC ID:");
	for ( i = 0; i < asic_size; ++i )
		printf(" 0x%.2X", (unsigned int)asic_buffer[i]);
	printf("\n");

	return 1;

}

static int send_2nd(usb_dev_handle * udev, struct image * image) {

	uint8_t buffer[1024];
	uint32_t need, readed;
	int ret;

	printf("Sending OMAP peripheral boot message...\n");
	ret = usb_bulk_write(udev, WRITE_DEV, (char *)&omap_peripheral_msg, sizeof(omap_peripheral_msg), WRITE_TIMEOUT);
	usleep(5000);
	if ( ret != sizeof(omap_peripheral_msg) ) {
		fprintf(stderr, "Error while sending OMAP peripheral boot message\n");
		return 0;
	}

	printf("Sending 2nd X-Loader image size...\n");
	ret = usb_bulk_write(udev, WRITE_DEV, (char *)&image->size, 4, WRITE_TIMEOUT);
	usleep(5000);
	if ( ret != 4 ) {
		fprintf(stderr, "Error while sending 2nd X-Loader image size\n");
		return 0;
	}

	printf("Sending 2nd X-Loader image...\n");
	image_seek(image, 0);
	readed = 0;
	while ( readed < image->size ) {
		need = image->size - readed;
		if ( need > sizeof(buffer) )
			need = sizeof(buffer);
		ret = image_read(image, buffer, need);
		if ( ret < 0 )
			break;
		if ( usb_bulk_write(udev, WRITE_DEV, (char *)buffer, ret, WRITE_TIMEOUT) != ret ) {
			fprintf(stderr, "Error while sending 2nd X-Loader image\n");
			return 0;
		}
		readed += ret;
	}
	usleep(50000);

	return 1;

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
	if ( ret != sizeof(init_msg) ) {
		fprintf(stderr, "Error while sending X-Loader init message\n");
		return 0;
	}

	printf("Waiting for X-Loader response...\n");
	ret = usb_bulk_read(udev, READ_DEV, (char *)&buffer, 4, READ_TIMEOUT); /* 4 bytes - dummy value */
	if ( ret != 4 ) {
		fprintf(stderr, "Error no response\n");
		return 0;
	}

	printf("Sending Secondary image...\n");
	image_seek(image, 0);
	readed = 0;
	while ( readed < image->size ) {
		need = image->size - readed;
		if ( need > sizeof(buffer) )
			need = sizeof(buffer);
		ret = image_read(image, buffer, need);
		if ( ret < 0 )
			break;
		if ( usb_bulk_write(udev, WRITE_DEV, (char *)buffer, ret, WRITE_TIMEOUT) != ret ) {
			fprintf(stderr, "Error while sending Secondary image\n");
			return 0;
		}
		readed += ret;
	}
	usleep(5000);

	printf("Waiting for X-Loader response...\n");
	ret = usb_bulk_read(udev, READ_DEV, (char *)&buffer, 4, READ_TIMEOUT); /* 4 bytes - dummy value */
	if ( ret != 4 ) {
		fprintf(stderr, "Error no response\n");
		return 0;
	}

	return 1;

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
		if ( ret != sizeof(ping_msg) ) {
			fprintf(stderr, "Error while sending X-Loader ping message\n");
			return 0;
		}

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
		return 1;
	else
		return 0;

}

int cold_flash(usb_dev_handle * udev, struct image * x2nd, struct image * secondary) {

	if ( x2nd->type != IMAGE_2ND ) {
		fprintf(stderr, "Image type is not 2nd X-Loader\n");
		return 1;
	}

	if ( secondary->type != IMAGE_SECONDARY ) {
		fprintf(stderr, "Image type is not Secondary\n");
		return 1;
	}

#if defined(LIBUSB_HAS_GET_DRIVER_NP) && defined(LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP)
	usb_detach_kernel_driver_np(udev, 0);
#endif

	if ( usb_claim_interface(udev, 0) < 0 ) {
		fprintf(stderr, "usb_claim_interface failed: %s\n", strerror(errno));
		return 1;
	}

	if ( ! read_asic(udev) ) {
		fprintf(stderr, "Reading ASIC ID failed\n");
		return 1;
	}

	if ( ! send_2nd(udev, x2nd) ) {
		fprintf(stderr, "Sending 2nd X-Loader image failed\n");
		return 1;
	}

	if ( ! ping_timeout(udev) ) {
		fprintf(stderr, "Sending X-Loader ping message failed\n");
		return 1;
	}

	if ( ! send_secondary(udev, secondary) ) {
		fprintf(stderr, "Sending Secondary image failed\n");
		return 1;
	}

	printf("Done\n");
	return 0;

}

int leave_cold_flash(usb_dev_handle * udev) {

	int ret;

#if defined(LIBUSB_HAS_GET_DRIVER_NP) && defined(LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP)
	usb_detach_kernel_driver_np(udev, 0);
#endif

	if ( usb_claim_interface(udev, 0) < 0 ) {
		fprintf(stderr, "usb_claim_interface failed: %s\n", strerror(errno));
		return 1;
	}

	if ( ! read_asic(udev) ) {
		fprintf(stderr, "Reading ASIC ID failed\n");
		return 1;
	}

	printf("Sending OMAP memory boot message...\n");
	ret = usb_bulk_write(udev, WRITE_DEV, (char *)&omap_memory_msg, sizeof(omap_memory_msg), WRITE_TIMEOUT);
	usleep(5000);
	if ( ret != sizeof(omap_memory_msg) ) {
		fprintf(stderr, "Error while sending OMAP memory boot message\n");
		return 1;
	}

	usleep(500000);
	return 0;

}
