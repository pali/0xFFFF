/*
    cold-flasher-rx51.c - Cold flasher for Nokia RX51
    Copyright (C) 2011  Pali Rohár <pali.rohar@gmail.com>

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

/* compile: gcc cold-flasher-rx51.c -o cold-flasher-rx51 -W -Wall -O2 -lusb */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <usb.h>

#define READ_DEV		0x81
#define WRITE_DEV		0x01
#define READ_TIMEOUT		500
#define WRITE_TIMEOUT		3000

#define USB_VENDOR		0x0421
#define USB_PRODUCT		0x0106

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

/* Omap download message - same as in pusb.c */
static const uint8_t omap_download_msg[4] = { 0x02, 0x00, 0x03, 0xF0 };

/* Structure of X-Loader message */
struct xloader_msg {
	uint8_t type[4]; /* 4 bytes - type of message */
	uint8_t size[4]; /* 4 bytes - size of file */
	uint8_t crc1[4]; /* 4 bytes - crc32 of file */
	uint8_t crc2[4]; /* 4 bytes - crc32 of first 12 bytes of message */
};

#define XLOADER_MSG_TYPE_PING	0x6301326E
#define XLOADER_MSG_TYPE_SEND	0x6302326E

struct xloader_msg xloader_msg_create(uint32_t type, uint32_t size, FILE * file) {

	struct xloader_msg msg;
	uint8_t buffer[1024];
	uint32_t crc = 0;

	memcpy(msg.type, &type, 4);
	memcpy(msg.size, &size, 4);

	while ( file && ( size = fread(buffer, 1, sizeof(buffer), file) ) > 0 )
		crc = crc32(buffer, size, crc);

	if ( file )
		rewind(file);

	memcpy(msg.crc1, &crc, 4);

	crc = crc32((unsigned char *)&msg, 12, 0);
	memcpy(msg.crc2, &crc, 4);

	return msg;

}

static struct usb_device * search_device(struct usb_device *dev, int level) {

	int i;
	struct usb_device * ret = NULL;

	if ( ! dev )
		return NULL;

	if ( ( dev->descriptor.idVendor == USB_VENDOR ) && ( dev->descriptor.idProduct == USB_PRODUCT ) )
		return dev;

	for ( i = 0; i < dev->num_children; i++ ) {
		ret = search_device(dev->children[i], level + 1);
		if ( ret )
			break;
	}

	return ret;

}

static usb_dev_handle * find_device(void) {

	struct usb_bus * bus;
	struct usb_device * ret = NULL;

	usb_init();
	usb_find_busses();

	printf("Waiting for USB device\n");

	while ( ! ret ) {

		usb_find_devices();

		for ( bus = usb_get_busses(); bus; bus = bus->next ) {

			if ( bus->root_dev )
				ret = search_device(bus->root_dev, 0);
			else {
				struct usb_device *dev;
				for ( dev = bus->devices; dev; dev = dev->next ) {
					ret = search_device(dev, 0);
					if ( ret )
						break;
				}
			}

			if ( ret )
				break;

		}

	}

	if ( ret )
		return usb_open(ret);
	else
		return NULL;

}

static int read_asic(usb_dev_handle * udev) {

	uint8_t asic_buffer[127];
	int asic_size = 69;
	int i;
	int ret;

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

static int send_2nd(usb_dev_handle * udev, FILE * file) {

	uint8_t buffer[1024];
	uint32_t size32;
	size_t size;
	int ret;

	fseek(file, 0, SEEK_END);
	size32 = size = ftell(file);
	rewind(file);

	if ( size == 0 ) {
		fprintf(stderr, "2nd X-Loader image has zero size\n");
		return 0;
	}

	if ( size > UINT32_MAX ) {
		fprintf(stderr, "2nd X-Loader image is too big\n");
		return 0;
	}

	printf("Sending OMAP download message...\n");
	ret = usb_bulk_write(udev, WRITE_DEV, (char *)omap_download_msg, sizeof(omap_download_msg), WRITE_TIMEOUT);
	usleep(5000);
	if ( ret != sizeof(omap_download_msg) ) {
		fprintf(stderr, "Error while sending OMAP download message\n");
		return 0;
	}

	printf("Sending 2nd X-Loader image size...\n");
	ret = usb_bulk_write(udev, WRITE_DEV, (char *)&size32, sizeof(size32), WRITE_TIMEOUT);
	usleep(5000);
	if ( ret != sizeof(size32) ) {
		fprintf(stderr, "Error while sending 2nd X-Loader image size\n");
		return 0;
	}

	printf("Sending 2nd X-Loader image...\n");
	while ( ( ret = fread(buffer, 1, sizeof(buffer), file) ) > 0 ) {
		if ( usb_bulk_write(udev, WRITE_DEV, (char *)buffer, ret, WRITE_TIMEOUT) != ret ) {
			fprintf(stderr, "Error while sending 2nd X-Loader image\n");
			return 0;
		}
	}
	usleep(50000);

	return 1;

}

static int send_secondary(usb_dev_handle * udev, FILE * file) {

	struct xloader_msg init_msg;
	uint8_t buffer[1024];
	uint32_t size32;
	size_t size;
	int ret;

	fseek(file, 0, SEEK_END);
	size32 = size = ftell(file);
	rewind(file);

	if ( size == 0 ) {
		fprintf(stderr, "2nd Secondary image has zero size\n");
		return 0;
	}

	if ( size > UINT32_MAX ) {
		fprintf(stderr, "2nd Secondary image is too big\n");
		return 0;
	}

	init_msg = xloader_msg_create(XLOADER_MSG_TYPE_SEND, size32, file);

	printf("Sending X-Loader init message...\n");
	ret = usb_bulk_write(udev, WRITE_DEV, (char *)&init_msg, sizeof(init_msg), WRITE_TIMEOUT);
	usleep(5000);
	if ( ret != sizeof(init_msg) ) {
		fprintf(stderr, "Error while sending X-Loader init message\n");
		return 0;
	}

	printf("Waiting for X-Loader response...\n");
	ret = usb_bulk_read(udev, READ_DEV, (char *)&size32, sizeof(size32), READ_TIMEOUT); /* size32 - dummy value */
	if ( ret != sizeof(size32) ) {
		fprintf(stderr, "Error no response\n");
		return 0;
	}

	printf("Sending Secondary image...\n");
	while ( ( ret = fread(buffer, 1, sizeof(buffer), file) ) > 0 ) {
		if ( usb_bulk_write(udev, WRITE_DEV, (char *)buffer, ret, WRITE_TIMEOUT) != ret ) {
			fprintf(stderr, "Error while sending Secondary image\n");
			return 0;
		}
	}
	usleep(5000);

	printf("Waiting for X-Loader response...\n");
	ret = usb_bulk_read(udev, READ_DEV, (char *)&size32, sizeof(size32), READ_TIMEOUT); /* size32 - dummy value */
	if ( ret != sizeof(size32) ) {
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

		struct xloader_msg ping_msg = xloader_msg_create(XLOADER_MSG_TYPE_PING, 0, NULL);
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

static void usage(char * arg) {
	printf("Usage: \n");
	printf("%s 2nd.bin secondary.bin\n", arg);
	printf("    * Send 2nd X-Loader (2nd.bin) via USB OMAP download command, boot it and wait\n");
	printf("    * Send Secondary NOLO (secondary.bin) via USB X-Loader commands and flash it into NAND\n");
	printf("%s -s secondary.bin\n", arg);
	printf("    * Expect that 2nd X-Loader is already booted, send only secondary.bin and flash it\n");
}

int main(int argc, char * argv[]) {

	FILE * file1 = NULL;
	FILE * file2 = NULL;
	int secondary = 0;
	usb_dev_handle * udev = NULL;

	if ( argc != 3 ) {
		usage(argv[0]);
		return 1;
	}

	if ( strncmp(argv[1], "-s", 2) == 0 )
		secondary = 1;

	if ( ! secondary ) {

		file1 = fopen(argv[1], "r");
		if ( ! file1 ) {
			fprintf(stderr, "Cannot open 2nd X-Loader file '%s': %s\n", argv[1], strerror(errno));
			return 1;
		}

	}

	file2 = fopen(argv[2], "r");
	if ( ! file2 ) {
		fprintf(stderr, "Cannot open Secondary file '%s': %s\n", argv[2], strerror(errno));
		return 1;
	}

	udev = find_device();
	if ( ! udev ) {
		fprintf(stderr, "Cannot find USB device '0x%x:0x%x'\n", USB_VENDOR, USB_PRODUCT);
		return 1;
	}

	printf("Found USB device\n");

	usb_set_configuration(udev, 1);
	usb_claim_interface(udev, 1);

	if ( ! secondary ) {

		if ( ! read_asic(udev) ) {
			fprintf(stderr, "Reading ASIC ID failed\n");
			return 1;
		}

		if ( ! send_2nd(udev, file1) ) {
			fprintf(stderr, "Sending 2nd X-Loader image failed\n");
			return 1;
		}

		if ( ! ping_timeout(udev) ) {
			fprintf(stderr, "Sending X-Loader ping failed\n");
			return 1;
		}

	}

	if ( ! send_secondary(udev, file2) ) {
		fprintf(stderr, "Sending Secondary image failed\n");
		return 1;
	}

	printf("Done\n");

	return 0;

}
