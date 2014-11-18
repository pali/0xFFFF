/*
    0xFFFF - Open Free Fiasco Firmware Flasher
    Copyright (C) 2012  Pali Rohár <pali.rohar@gmail.com>

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
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>

#include "mkii.h"
#include "global.h"
#include "image.h"
#include "device.h"
#include "usb-device.h"

#define MKII_PING		0x00000000
#define MKII_GET_PROTOCOL	0x01010000
#define MKII_TELL_PROTOCOL	0x02020000
#define MKII_GET_DEVICE		0x01030000
#define MKII_GET_HWREV		0x01040000
#define MKII_GET_IMAGES		0x01050000
#define MKII_REBOOT		0x0C060000
#define MKII_INIT_SEND		0x03010000
#define MKII_SEND_IMAGE		0x04020000

struct mkii_message {
	uint32_t header;
	uint16_t size;
	uint32_t type;
	char data[];
} __attribute__((__packed__));

static int mkii_send_receive(usb_dev_handle * udev, uint32_t type, struct mkii_message * in_msg, size_t data_size, struct mkii_message * out_msg, size_t out_size) {

	int ret;

	in_msg->header = 0x8810001B;
	in_msg->size = htons(data_size + 4);
	in_msg->type = type;

	ret = usb_bulk_write(udev, 1, (const char *)in_msg, data_size + sizeof(*in_msg), 5000);
	if ( ret < 0 )
		return ret;
	if ( (size_t)ret != data_size + sizeof(*in_msg) )
		return -1;

	ret = usb_bulk_read(udev, 129, (char *)out_msg, out_size, 5000);
	if ( ret < 0 )
		return ret;

	if ( out_msg->header != 0x8800101B )
		return -1;

	if ( out_msg->type != (type | 0x20000000) )
		return -1;

	if ( (size_t)ret < sizeof(*out_msg) )
		return -1;

	if ( ntohs(out_msg->size) != ret - sizeof(*out_msg) + 4 )
		return -1;

	return ret - sizeof(*out_msg);

}

int mkii_init(struct usb_device_info * dev) {

	char buf[2048];
	struct mkii_message * msg;
	enum device device;
	int ret;

	printf("Initializing Mk II protocol...\n");

	msg = (struct mkii_message *)buf;

	ret = mkii_send_receive(dev->udev, MKII_PING, msg, 0, msg, sizeof(buf));
	if ( ret != 0 )
		return -1;

	memcpy(msg->data, "/update/protocol_version", sizeof("/update/protocol_version")-1);
	ret = mkii_send_receive(dev->udev, MKII_GET_PROTOCOL, msg, sizeof("/update/protocol_version")-1, msg, sizeof(buf));
	if ( ret != 2 || msg->data[0] != 0 || msg->data[1] != 0x32 )
		return -1;

	memcpy(msg->data, "/update/host_protocol_version\x00\x32", sizeof("/update/host_protocol_version\x00\x32")-1);
	ret = mkii_send_receive(dev->udev, MKII_TELL_PROTOCOL, msg, sizeof("/update/host_protocol_version\x00\x32")-1, msg, sizeof(buf));
	if ( ret != 1 || msg->data[0] != 0 )
		return -1;

	device = mkii_get_device(dev);

	if ( ! dev->device )
		dev->device = device;

	if ( dev->device && device && dev->device != device ) {
		ERROR("Device mishmash, expected %s, got %s", device_to_string(dev->device), device_to_string(device));
		return -1;
	}

	dev->hwrev = mkii_get_hwrev(dev);

	return 0;

}

enum device mkii_get_device(struct usb_device_info * dev) {

	char buf[2048];
	struct mkii_message * msg;
	int ret;

	msg = (struct mkii_message *)buf;

	memcpy(msg->data, "/device/product_code", sizeof("/device/product_code")-1);
	ret = mkii_send_receive(dev->udev, MKII_GET_DEVICE, msg, sizeof("/device/product_code")-1, msg, sizeof(buf));
	if ( ret < 2 || msg->data[0] != 0 || msg->data[1] == 0 )
		return DEVICE_UNKNOWN;

	msg->data[ret] = 0;
	return device_from_string((char *)msg->data+1);

}

int mkii_flash_image(struct usb_device_info * dev, struct image * image) {

	char buf1[512];
	char buf[2048];
	struct mkii_message * msg1;
	struct mkii_message * msg;
	char * ptr;
	const char * type;
	uint8_t len;
	uint16_t hash;
	uint32_t size;
	int ret;

	ERROR("Not implemented yet");
	return -1;

	msg = (struct mkii_message *)buf;
	msg1 = (struct mkii_message *)buf1;
	ptr = msg->data;

	/* Signature */
	memcpy(ptr, "\x2E\x19\x01\x01", 4);
	ptr += 4;

	/* Space */
	memcpy(ptr, "\x00", 1);
	ptr += 1;

	/* Hash */
	hash = htons(image->hash);
	memcpy(ptr, &hash, 2);
	ptr += 2;

	/* Type */
	type = image_type_to_string(image->type);
	if ( ! type )
		ERROR_RETURN("Unknown image type", -1);
	memset(ptr, 0, 12);
	strncpy(ptr, type, 12);
	ptr += 12;

	/* Size */
	size = htonl(image->size);
	memcpy(ptr, &size, 4);
	ptr += 4;

	/* Space */
	memcpy(ptr, "\x00\x00\x00\x00", 4);
	ptr += 4;

	/* Device & hwrev */
	if ( image->devices ) {

		int i;
		uint8_t len;
		char buf[9];
		char ** bufs = NULL;
		struct device_list * device = image->devices;

		while ( device ) {
			if ( device->device == dev->device && hwrev_is_valid(device->hwrevs, dev->hwrev) )
				break;
			device = device->next;
		}

		if ( device )
			bufs = device_list_alloc_to_bufs(device);

		if ( bufs ) {

			memset(buf, 0, sizeof(buf));
			snprintf(buf, 8+1, "%d", dev->hwrev);

			for ( i = 0; bufs[i]; ++i ) {
				len = ((uint8_t*)bufs[i])[0];
				if ( memmem(bufs[i]+1, len, buf, strlen(buf)) )
					break;
			}

			if ( bufs[i] ) {
				/* Device & hwrev string header */
				memcpy(ptr, "\x32", 1);
				ptr += 1;
				/* Device & hwrev string size */
				memcpy(ptr, &len, 1);
				ptr += 1;
				/* Device & hwrev string */
				memcpy(ptr, bufs[i]+1, len);
				ptr += len;
			}

			free(bufs);

		}

	}

	/* Version */
	if ( image->version ) {
		len = strnlen(image->version, 255) + 1;
		/* Version string header */
		memcpy(ptr, "\x31", 1);
		ptr += 1;
		/* Version string size */
		memcpy(ptr, &len, 1);
		ptr += 1;
		/* Version string */
		memcpy(ptr, image->version, len);
		ptr += len;
	}

	/* append layout subsection */
	if ( image->layout ) {
		len = strlen(image->layout);
		/* Layout header */
		memcpy(ptr, "\x33", 1);
		ptr += 1;
		/* Layout size */
		memcpy(ptr, &len, 1);
		ptr += 1;
		/* Layout string */
		memcpy(ptr, image->layout, len);
		ptr += len;
	}

	/* end */
	memcpy(ptr, "\x00", 1);
	ptr += 1;

	ret = mkii_send_receive(dev->udev, MKII_INIT_SEND, msg1, 0, msg1, sizeof(buf1));
	if ( ret != 1 || msg1->data[0] != 0 )
		return -1;

	ret = mkii_send_receive(dev->udev, MKII_SEND_IMAGE, msg, ptr - msg->data, msg, sizeof(buf));
	if ( ret != 9 )
		return -1;

	/* TODO: send image itself */

	return 0;

}

int mkii_reboot_device(struct usb_device_info * dev) {

	char buf[2048];
	struct mkii_message * msg;
	int ret;

	msg = (struct mkii_message *)buf;

	printf("Rebooting device...\n");

	memcpy(msg->data, "reboot", sizeof("reboot")-1);
	ret = mkii_send_receive(dev->udev, MKII_GET_DEVICE, msg, sizeof("reboot")-1, msg, sizeof(buf));
	if ( ret != 1 || msg->data[0] != 0 )
		return -1;

	return 0;

}

int mkii_get_root_device(struct usb_device_info * dev) {

	ERROR("Not implemented yet");
	(void)dev;
	return -1;

}

int mkii_set_root_device(struct usb_device_info * dev, int device) {

	ERROR("Not implemented yet");
	(void)dev;
	(void)device;
	return -1;

}

int mkii_get_usb_host_mode(struct usb_device_info * dev) {

	ERROR("Not implemented yet");
	(void)dev;
	return -1;

}

int mkii_set_usb_host_mode(struct usb_device_info * dev, int enable) {

	ERROR("Not implemented yet");
	(void)dev;
	(void)enable;
	return -1;

}

int mkii_get_rd_mode(struct usb_device_info * dev) {

	ERROR("Not implemented yet");
	(void)dev;
	return -1;

}

int mkii_set_rd_mode(struct usb_device_info * dev, int enable) {

	ERROR("Not implemented yet");
	(void)dev;
	(void)enable;
	return -1;

}

int mkii_get_rd_flags(struct usb_device_info * dev, char * flags, size_t size) {

	ERROR("Not implemented yet");
	(void)dev;
	(void)flags;
	(void)size;
	return -1;

}

int mkii_set_rd_flags(struct usb_device_info * dev, const char * flags) {

	ERROR("Not implemented yet");
	(void)dev;
	(void)flags;
	return -1;

}

int16_t mkii_get_hwrev(struct usb_device_info * dev) {

	char buf[2048];
	struct mkii_message * msg;
	int ret;

	msg = (struct mkii_message *)buf;

	memcpy(msg->data, "/device/hw_build", sizeof("/device/hw_build")-1);
	ret = mkii_send_receive(dev->udev, MKII_GET_DEVICE, msg, sizeof("/device/hw_build")-1, msg, sizeof(buf));
	if ( ret < 2 || msg->data[0] != 0 || msg->data[1] == 0 )
		return -1;

	msg->data[ret] = 0;
	return atoi(msg->data+1);

}

int mkii_set_hwrev(struct usb_device_info * dev, int16_t hwrev) {

	ERROR("Not implemented yet");
	(void)dev;
	(void)hwrev;
	return -1;

}

int mkii_get_kernel_ver(struct usb_device_info * dev, char * ver, size_t size) {

	ERROR("Not implemented yet");
	(void)dev;
	(void)ver;
	(void)size;
	return -1;

}

int mkii_set_kernel_ver(struct usb_device_info * dev, const char * ver) {

	ERROR("Not implemented yet");
	(void)dev;
	(void)ver;
	return -1;

}

int mkii_get_initfs_ver(struct usb_device_info * dev, char * ver, size_t size) {

	ERROR("Not implemented yet");
	(void)dev;
	(void)ver;
	(void)size;
	return -1;

}

int mkii_set_initfs_ver(struct usb_device_info * dev, const char * ver) {

	ERROR("Not implemented yet");
	(void)dev;
	(void)ver;
	return -1;

}

int mkii_get_nolo_ver(struct usb_device_info * dev, char * ver, size_t size) {

	ERROR("Not implemented yet");
	(void)dev;
	(void)ver;
	(void)size;
	return -1;

}

int mkii_set_nolo_ver(struct usb_device_info * dev, const char * ver) {

	ERROR("Not implemented yet");
	(void)dev;
	(void)ver;
	return -1;

}

int mkii_get_sw_ver(struct usb_device_info * dev, char * ver, size_t size) {

	ERROR("Not implemented yet");
	(void)dev;
	(void)ver;
	(void)size;
	return -1;

}

int mkii_set_sw_ver(struct usb_device_info * dev, const char * ver) {

	ERROR("Not implemented yet");
	(void)dev;
	(void)ver;
	return -1;

}

int mkii_get_content_ver(struct usb_device_info * dev, char * ver, size_t size) {

	ERROR("Not implemented yet");
	(void)dev;
	(void)ver;
	(void)size;
	return -1;

}

int mkii_set_content_ver(struct usb_device_info * dev, const char * ver) {

	ERROR("Not implemented yet");
	(void)dev;
	(void)ver;
	return -1;

}
