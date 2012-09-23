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
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include <usb.h>

#include "global.h"
#include "device.h"
#include "usb-device.h"
#include "printf-utils.h"
#include "nolo.h"
#include "cold-flash.h"

static struct usb_flash_device usb_devices[] = {
	{ 0x0421, 0x0105,  2,  1, -1, FLASH_NOLO, { DEVICE_SU_18, DEVICE_RX_44, DEVICE_RX_48, DEVICE_RX_51, 0 } },
	{ 0x0421, 0x0106,  0, -1, -1, FLASH_COLD, { DEVICE_RX_51, 0 } },
	{ 0x0421, 0x01c7,  0, -1, -1, FLASH_DISK, { DEVICE_RX_51, 0 } },
	{ 0x0421, 0x01c8,  1,  1, -1, FLASH_MKII, { DEVICE_RX_51, 0 } },
	{ 0x0421, 0x0431,  0, -1, -1, FLASH_DISK, { DEVICE_SU_18, DEVICE_RX_34, 0 } },
	{ 0x0421, 0x3f00,  2,  1, -1, FLASH_NOLO, { DEVICE_RX_34, 0 } },
};

static const char * usb_flash_protocols[] = {
	[FLASH_NOLO] = "NOLO",
	[FLASH_COLD] = "Cold flashing",
	[FLASH_MKII] = "Mk II protocol",
	[FLASH_DISK] = "RAW disk",
};

const char * usb_flash_protocol_to_string(enum usb_flash_protocol protocol) {

	if ( protocol > sizeof(usb_flash_protocols) )
		return NULL;

	return usb_flash_protocols[protocol];

}

static void usb_flash_device_info_print(const struct usb_flash_device * dev) {

	int i;

	PRINTF_ADD("USB device: %s", device_to_string(dev->devices[0]));

	for ( i = 1; dev->devices[i]; ++i )
		PRINTF_ADD("/%s", device_to_string(dev->devices[i]));

	PRINTF_ADD(" (%#04x:%#04x) in %s mode", dev->vendor, dev->product, usb_flash_protocol_to_string(dev->protocol));

}

static void usb_descriptor_info_print(usb_dev_handle * udev, struct usb_device * dev, char * product, size_t size) {

	char buf[1024];
	char buf2[1024];
	unsigned int x;
	int ret;
	int i;

	memset(buf, 0, sizeof(buf));
	usb_get_string_simple(udev, dev->descriptor.iProduct, buf, sizeof(buf));
	PRINTF_LINE("USB device product string: %s", buf[0] ? buf : "(not detected)");
	PRINTF_END();

	if ( product && buf[0] )
		strncpy(product, buf, size);

	memset(buf, 0, sizeof(buf));
	memset(buf2, 0, sizeof(buf2));
	ret = usb_get_string_simple(udev, dev->descriptor.iSerialNumber, buf, sizeof(buf));
	if ( ! isalnum(buf[0]) )
		buf[0] = 0;
	for ( i = 0; i < ret; i+=2 ) {
		sscanf(buf+i, "%2x", &x);
		if ( x > 32 && x < 128 )
			buf2[i/2] = x;
		else {
			buf2[0] = 0;
			break;
		}
	}
	if ( ! isalnum(buf2[0]) )
		buf2[0] = 0;
	PRINTF_LINE("USB device serial number string: %s", buf2[0] ? buf2 : ( buf[0] ? buf : "(not detected)" ));
	PRINTF_END();

}

static struct usb_device_info * usb_device_is_valid(struct usb_device * dev) {

	int i;
	char product[1024];
	struct usb_device_info * ret = NULL;

	for ( i = 0; usb_devices[i].vendor; ++i ) {

		if ( dev->descriptor.idVendor == usb_devices[i].vendor && dev->descriptor.idProduct == usb_devices[i].product ) {

			printf("\b\b  ");
			PRINTF_END();
			PRINTF_ADD("Found ");
			usb_flash_device_info_print(&usb_devices[i]);
			PRINTF_END();

			PRINTF_LINE("Opening USB...");
			usb_dev_handle * udev = usb_open(dev);
			if ( ! udev ) {
				PRINTF_ERROR("usb_open failed");
				return NULL;
			}

			usb_descriptor_info_print(udev, dev, product, sizeof(product));

#if defined(LIBUSB_HAS_GET_DRIVER_NP) && defined(LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP)
			PRINTF_LINE("Detaching kernel from USB interface...");
			usb_detach_kernel_driver_np(udev, usb_devices[i].interface);
#endif

			PRINTF_LINE("Claiming USB interface...");
			if ( usb_claim_interface(udev, usb_devices[i].interface) < 0 ) {
				PRINTF_ERROR("usb_claim_interface failed");
				usb_close(udev);
				exit(1);
				return NULL;
			}

			if ( usb_devices[i].alternate >= 0 ) {
				PRINTF_LINE("Setting alternate USB interface...");
				if ( usb_set_altinterface(udev, usb_devices[i].alternate) < 0 ) {
					PRINTF_ERROR("usb_claim_interface failed");
					usb_close(udev);
					return NULL;
				}
			}

			if ( usb_devices[i].configuration >= 0 ) {
				PRINTF_LINE("Setting USB configuration...");
				if ( usb_set_configuration(udev, usb_devices[i].configuration) < 0 ) {
					PRINTF_ERROR("usb_set_configuration failed");
					usb_close(udev);
					return NULL;
				}
			}

			ret = calloc(1, sizeof(struct usb_device_info));
			if ( ! ret ) {
				ALLOC_ERROR();
				usb_close(udev);
				return NULL;
			}

			if ( strstr(product, "N900") )
				ret->device = DEVICE_RX_51;
			else
				ret->device = DEVICE_UNKNOWN;

			/* TODO: Autodetect more devices */

			if ( device_to_string(ret->device) )
				PRINTF_LINE("Detected USB device: %s", device_to_string(ret->device));
			else
				PRINTF_LINE("Detected USB device: (not detected)");
			PRINTF_END();

			if ( ret->device ) {
				enum device * device;
				for ( device = usb_devices[i].devices; *device; ++device )
					if ( *device == ret->device )
						break;
				if ( ! *device ) {
					ERROR("Device mishmash");
					usb_close(udev);
					return NULL;
				}
			}

			ret->hwrev = -1;
			ret->flash_device = &usb_devices[i];
			ret->udev = udev;
			break;
		}
	}

	return ret;

}

static struct usb_device_info * usb_search_device(struct usb_device * dev, int level) {

	int i;
	struct usb_device_info * ret = NULL;

	if ( ! dev )
		return NULL;

	ret = usb_device_is_valid(dev);
	if ( ret )
		return ret;

	for ( i = 0; i < dev->num_children; i++ ) {
		ret = usb_search_device(dev->children[i], level + 1);
		if ( ret )
			break;
	}

	return ret;

}

struct usb_device_info * usb_open_and_wait_for_device(void) {

	struct usb_bus * bus;
	struct usb_device_info * ret = NULL;
	int i = 0;
	static char progress[] = {'/','-','\\', '|'};

	usb_init();
	usb_find_busses();

	PRINTF_BACK();
	printf("\n");

	while ( 1 ) {

		PRINTF_LINE("Waiting for USB device... %c", progress[++i%sizeof(progress)]);

		usb_find_devices();

		for ( bus = usb_get_busses(); bus; bus = bus->next ) {

			if ( bus->root_dev )
				ret = usb_search_device(bus->root_dev, 0);
			else {
				struct usb_device *dev;
				for ( dev = bus->devices; dev; dev = dev->next ) {
					ret = usb_search_device(dev, 0);
					if ( ret )
						break;
				}
			}

			if ( ret )
				break;

		}

		if ( ret )
			break;

		usleep(0xc350); // 0.5s

	}

	PRINTF_BACK();
	printf("\n");

	if ( ! ret )
		return NULL;

	return ret;

}

void usb_close_device(struct usb_device_info * dev) {

	usb_close(dev->udev);
	free(dev);

}

void usb_switch_to_nolo(struct usb_device_info * dev) {

	printf("\nSwitching to NOLO mode...\n");

	if ( dev->flash_device->protocol == FLASH_COLD )
		leave_cold_flash(dev);
	else if ( dev->flash_device->protocol == FLASH_MKII )
		ERROR("Leaving Mk II protocol is not implemented yet");
	else if ( dev->flash_device->protocol == FLASH_DISK )
		printf_and_wait("Unplug USB cable, turn device off, press ENTER and plug USB cable again");

}

void usb_switch_to_cold(struct usb_device_info * dev) {

	printf("\nSwitching to Cold Flash mode...\n");
	printf_and_wait("Unplug USB cable, turn device off, press ENTER and plug USB cable again");

}

void usb_switch_to_mkii(struct usb_device_info * dev) {

	printf("\nSwitching to Update mode...\n");

	if ( dev->flash_device->protocol == FLASH_COLD )
		leave_cold_flash(dev);
	else if ( dev->flash_device->protocol == FLASH_NOLO )
		nolo_boot_device(dev, "update");
	else if ( dev->flash_device->protocol == FLASH_DISK )
		printf_and_wait("Unplug USB cable, turn device off, press ENTER and plug USB cable again");

}

void usb_switch_to_disk(struct usb_device_info * dev) {

	printf("\nSwitching to RAW disk mode...\n");
	ERROR("usb_switch_to_disk is not implemented yet");

}
