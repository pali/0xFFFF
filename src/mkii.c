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

#define MKII_OUT	0x8810001B
#define MKII_IN		0x8800101B

#define MKII_PING	0x00
#define MKII_GET	0x01
#define MKII_TELL	0x02
#define MKII_REBOOT	0x0C
#define MKII_RESPONCE	0x20

struct mkii_message {
	uint32_t header;
	uint16_t size;
	uint16_t zero;
	uint8_t num;
	uint8_t type;
	char data[];
} __attribute__((__packed__));


static int mkii_send_receive(libusb_device_handle * udev, uint8_t type, struct mkii_message * in_msg, size_t data_size, struct mkii_message * out_msg, size_t out_size) {

	int ret, transferred;
	static uint8_t number = 0;

	in_msg->header = MKII_OUT;
	in_msg->size = htons(data_size + 4);
	in_msg->zero = 0;
	in_msg->num = number++;
	in_msg->type = type;

	ret = libusb_bulk_transfer(udev, USB_WRITE_EP, (unsigned char *)in_msg, data_size + sizeof(*in_msg), &transferred, 5000);
	if ( ret < 0 )
		return ret;
	if ( (size_t)transferred != data_size + sizeof(*in_msg) )
		return -1;

	ret = libusb_bulk_transfer(udev, USB_READ_EP, (unsigned char *)out_msg, out_size, &transferred, 5000);
	if ( ret < 0 )
		return ret;
	if ( (size_t)transferred < sizeof(*out_msg) )
		return -1;

	if ( out_msg->header != MKII_IN )
		return -1;

	if ( out_msg->type != (type | MKII_RESPONCE) )
		return -1;

	if ( (size_t)transferred < sizeof(*out_msg) )
		return -1;

	if ( ntohs(out_msg->size) != transferred - sizeof(*out_msg) + 4 )
		return -1;

	return transferred - sizeof(*out_msg);

}

int mkii_init(struct usb_device_info * dev) {

	char buf[2048];
	struct mkii_message * msg;
	enum device device;
	int ret;
	char * newptr;
	char * ptr;
	enum image_type type;
	struct libusb_device *udev;
	struct libusb_config_descriptor *desc;

	printf("Initializing Mk II protocol...\n");

	msg = (struct mkii_message *)buf;

	ret = mkii_send_receive(dev->udev, MKII_PING, msg, 0, msg, sizeof(buf));
	if ( ret != 0 )
		return -1;

	memcpy(msg->data, "/update/protocol_version", sizeof("/update/protocol_version")-1);
	ret = mkii_send_receive(dev->udev, MKII_GET, msg, sizeof("/update/protocol_version")-1, msg, sizeof(buf));
	if ( ret != 2 || msg->data[0] != 0 || msg->data[1] != 0x32 )
		return -1;

	memcpy(msg->data, "/update/host_protocol_version\x00\x32", sizeof("/update/host_protocol_version\x00\x32")-1);
	ret = mkii_send_receive(dev->udev, MKII_TELL, msg, sizeof("/update/host_protocol_version\x00\x32")-1, msg, sizeof(buf));
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

	memcpy(msg->data, "/update/supported_images", sizeof("/update/supported_images")-1);
	ret = mkii_send_receive(dev->udev, MKII_GET, msg, sizeof("/update/supported_images")-1, msg, sizeof(buf));
	if ( ret < 2 || msg->data[0] != 0 ) {
		ERROR("Cannot get supported image types");
		return -1;
	}

	msg->data[ret] = 0;
	ptr = msg->data + 1;

	printf("Supported images by current device configuration:");

	while ( ptr && *ptr ) {
		newptr = strchr(ptr, ',');
		if ( newptr ) {
			*newptr = 0;
			++newptr;
		}
		type = image_type_from_string(ptr);
		if ( type != IMAGE_UNKNOWN ) {
			dev->data |= (1UL << type);
			printf(" %s", ptr);
		}
		ptr = newptr;
	}

	printf("\n");

	memset(buf, 0, sizeof(buf));

	udev = libusb_get_device(dev->udev);
	ret = libusb_get_active_config_descriptor(udev, &desc);
	if ( ret == 0 )
		libusb_get_string_descriptor_ascii(dev->udev, desc->iConfiguration, (unsigned char*)buf, sizeof(buf));
	if ( strncmp(buf, "Firmware Upgrade Configuration", sizeof("Firmware Upgrade Configuration")) == 0 )
		dev->data |= MKII_UPDATE_MODE;
	if ( ret == 0 )
		libusb_free_config_descriptor(desc);

	printf("Mode: %s\n", (dev->data & MKII_UPDATE_MODE) ? "Update" : "PC Suite");

	return 0;

}

enum device mkii_get_device(struct usb_device_info * dev) {

	char buf[2048];
	struct mkii_message * msg;
	int ret;

	msg = (struct mkii_message *)buf;

	memcpy(msg->data, "/device/product_code", sizeof("/device/product_code")-1);
	ret = mkii_send_receive(dev->udev, MKII_GET, msg, sizeof("/device/product_code")-1, msg, sizeof(buf));
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

	if ( ! ( dev->data & (1UL << image->type) ) ) {
		ERROR("Flashing image %s is not supported in current device configuration", image_type_to_string(image->type));
		return -1;
	}

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
				if ( MEMMEM(bufs[i]+1, len, buf, strlen(buf)) )
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

	ret = mkii_send_receive(dev->udev, 0x03, msg1, 0, msg1, sizeof(buf1));
	if ( ret != 1 || msg1->data[0] != 0 )
		return -1;

	ret = mkii_send_receive(dev->udev, 0x04, msg, ptr - msg->data, msg, sizeof(buf));
	if ( ret != 9 )
		return -1;

	/* TODO: send image itself */

	return 0;

}

int mkii_reboot_device(struct usb_device_info * dev, int update) {

	char buf[2048];
	struct mkii_message * msg;
	const char * str;
	int len;
	int ret;

	msg = (struct mkii_message *)buf;

	if ( update ) {
		printf("Rebooting device to Update mode...\n");
		len = sizeof("reboot=update");
		str = "reboot=update";
	} else {
		printf("Rebooting device...\n");
		len = sizeof("reboot");
		str = "reboot";
	}

	memcpy(msg->data, str, len);
	ret = mkii_send_receive(dev->udev, MKII_REBOOT, msg, len, msg, sizeof(buf));
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
	ret = mkii_send_receive(dev->udev, MKII_GET, msg, sizeof("/device/hw_build")-1, msg, sizeof(buf));
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

	char buf[2048];
	struct mkii_message * msg;
	int ret;

	msg = (struct mkii_message *)buf;

	memcpy(msg->data, "/version/sw_release", sizeof("/version/sw_release")-1);
	ret = mkii_send_receive(dev->udev, MKII_GET, msg, sizeof("/version/sw_release")-1, msg, sizeof(buf));
	if ( ret < 2 || msg->data[0] != 0 || msg->data[1] == 0 )
		return -1;

	msg->data[ret] = 0;
	strncpy(ver, msg->data+1, size);
	ver[size-1] = 0;
	return strlen(ver);

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
