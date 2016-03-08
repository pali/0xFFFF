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
#include <signal.h>

#include <libusb-1.0/libusb.h>

#include "global.h"
#include "device.h"
#include "usb-device.h"
#include "printf-utils.h"
#include "nolo.h"
#include "cold-flash.h"
#include "mkii.h"

static struct usb_flash_device usb_devices[] = {
	{ 0x0421, 0x0096, -1, -1, -1, FLASH_DISK, { DEVICE_RX_44, 0 } },
	{ 0x0421, 0x0105,  2,  1, -1, FLASH_NOLO, { DEVICE_SU_18, DEVICE_RX_44, DEVICE_RX_48, DEVICE_RX_51, 0 } },
	{ 0x0421, 0x0106,  0, -1, -1, FLASH_COLD, { DEVICE_RX_51, 0 } },
	{ 0x0421, 0x0189, -1, -1, -1, FLASH_DISK, { DEVICE_RX_48, 0 } },
	{ 0x0421, 0x01c7, -1, -1, -1, FLASH_DISK, { DEVICE_RX_51, 0 } },
	{ 0x0421, 0x01c8,  1,  1, -1, FLASH_MKII, { DEVICE_RX_51, 0 } },
	{ 0x0421, 0x0431, -1, -1, -1, FLASH_DISK, { DEVICE_SU_18, DEVICE_RX_34, 0 } },
	{ 0x0421, 0x04c3, -1, -1, -1, FLASH_DISK, { DEVICE_RX_34, 0 } },
	{ 0x0421, 0x3f00,  2,  1, -1, FLASH_NOLO, { DEVICE_RX_34, 0 } },
};

static const char * usb_flash_protocols[] = {
	[FLASH_NOLO] = "NOLO",
	[FLASH_COLD] = "Cold flashing",
	[FLASH_MKII] = "Mk II protocol",
	[FLASH_DISK] = "RAW disk",
};

const char * usb_flash_protocol_to_string(enum usb_flash_protocol protocol) {

	if ( protocol >= sizeof(usb_flash_protocols)/sizeof(usb_flash_protocols[0]) )
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

static void usb_reattach_kernel_driver(libusb_device_handle * udev, int interface) {

	PRINTF_LINE("Reattach kernel driver to USB interface...");
	PRINTF_END();
	libusb_release_interface(udev, interface);
	libusb_attach_kernel_driver(udev, interface);

}

static void usb_descriptor_info_print(libusb_device_handle * udev, struct libusb_device * dev, char * product, size_t size) {

	struct libusb_device_descriptor desc;
	char buf[1024];
	char buf2[1024];
	unsigned int x;
	int ret;
	int i;

	if ( libusb_get_device_descriptor(dev, &desc) < 0 ) {
		PRINTF_LINE("libusb_get_device_descriptor() failed");
		PRINTF_END();
		return;
	}
	memset(buf, 0, sizeof(buf));
	libusb_get_string_descriptor_ascii(udev, desc.iProduct, (unsigned char *)buf, sizeof(buf));
	PRINTF_LINE("USB device product string: %s", buf[0] ? buf : "(not detected)");
	PRINTF_END();

	if ( product && buf[0] )
		strncpy(product, buf, size);

	memset(buf, 0, sizeof(buf));
	memset(buf2, 0, sizeof(buf2));
	ret = libusb_get_string_descriptor_ascii(udev, desc.iSerialNumber, (unsigned char *)buf, sizeof(buf));
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

static struct usb_device_info * usb_device_is_valid(struct libusb_device * dev) {

	int err, i;
	char product[1024];
	libusb_device_handle * udev;
	struct usb_device_info * ret = NULL;
	struct libusb_device_descriptor desc;

	if ( libusb_get_device_descriptor(dev, &desc) < 0 ) {
		PRINTF_LINE("libusb_get_device_descriptor failed");
		PRINTF_END();
		return NULL;
	}

	for ( i = 0; usb_devices[i].vendor; ++i ) {

		if ( desc.idVendor == usb_devices[i].vendor && desc.idProduct == usb_devices[i].product ) {

			printf("\b\b  ");
			PRINTF_END();
			PRINTF_ADD("Found ");
			usb_flash_device_info_print(&usb_devices[i]);
			PRINTF_END();

			PRINTF_LINE("Opening USB...");

			err = libusb_open(dev, &udev);
			if ( err < 0 ) {
				PRINTF_ERROR("libusb_open failed");
				fprintf(stderr, "\n");
				return NULL;
			}

			usb_descriptor_info_print(udev, dev, product, sizeof(product));

			if ( usb_devices[i].interface >= 0 ) {

				PRINTF_LINE("Detaching kernel from USB interface...");
				libusb_detach_kernel_driver(udev, usb_devices[i].interface);

				PRINTF_LINE("Claiming USB interface...");
				if ( libusb_claim_interface(udev, usb_devices[i].interface) < 0 ) {
					PRINTF_ERROR("libusb_claim_interface failed");
					fprintf(stderr, "\n");
					usb_reattach_kernel_driver(udev, usb_devices[i].interface);
					libusb_close(udev);
					return NULL;
				}

			}

			if ( usb_devices[i].alternate >= 0 ) {
				PRINTF_LINE("Setting alternate USB interface...");
				if ( libusb_set_interface_alt_setting(udev, usb_devices[i].interface, usb_devices[i].alternate) < 0 ) {
					PRINTF_ERROR("libusb_claim_interface failed");
					fprintf(stderr, "\n");
					usb_reattach_kernel_driver(udev, usb_devices[i].interface);
					libusb_close(udev);
					return NULL;
				}
			}

			if ( usb_devices[i].configuration >= 0 ) {
				PRINTF_LINE("Setting USB configuration...");
				if ( libusb_set_configuration(udev, usb_devices[i].configuration) < 0 ) {
					PRINTF_ERROR("libusb_set_configuration failed");
					fprintf(stderr, "\n");
					usb_reattach_kernel_driver(udev, usb_devices[i].interface);
					libusb_close(udev);
					return NULL;
				}
			}

			ret = calloc(1, sizeof(struct usb_device_info));
			if ( ! ret ) {
				ALLOC_ERROR();
				usb_reattach_kernel_driver(udev, usb_devices[i].interface);
				libusb_close(udev);
				return NULL;
			}

			if ( strncmp(product, "Nokia 770", sizeof("Nokia 770")-1) == 0 )
				ret->device = DEVICE_SU_18;
			else if ( strstr(product, "N900") )
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
					fprintf(stderr, "\n");
					usb_reattach_kernel_driver(udev, usb_devices[i].interface);
					libusb_close(udev);
					free(ret);
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

static volatile sig_atomic_t signal_quit;

static void signal_handler(int signum) {

	signal_quit = 1;
	(void)signum;

}

struct usb_device_info * usb_open_and_wait_for_device(void) {

	libusb_device **devs;
	libusb_device **dev;
	struct usb_device_info * ret = NULL;
	int i = 0;
	void (*prev)(int);
	static char progress[] = {'/','-','\\', '|'};

	if ( libusb_init(NULL) < 0 ) {
		PRINTF_LINE("libusb_init failed");
		PRINTF_END();
		return NULL;
	}

	PRINTF_BACK();
	printf("\n");

	signal_quit = 0;

	prev = signal(SIGINT, signal_handler);

	while ( ! signal_quit ) {

		PRINTF_LINE("Waiting for USB device... %c", progress[++i%sizeof(progress)]);

		if ( libusb_get_device_list(NULL, &devs) < 0 ) {
			PRINTF_LINE("Listing USB devices failed");
			PRINTF_END();
			break;
		}

		for ( dev = devs; *dev != NULL; ++dev ) {
			ret = usb_device_is_valid(*dev);
			if ( ret )
				break;
		}

		libusb_free_device_list(devs, 1);

		if ( ret )
			break;

		SLEEP(0xc350); // 0.5s

	}

	if ( prev != SIG_ERR )
		signal(SIGINT, prev);

	PRINTF_BACK();
	printf("\n");

	if ( ! ret )
		return NULL;

	return ret;

}

void usb_close_device(struct usb_device_info * dev) {

	usb_reattach_kernel_driver(dev->udev, dev->flash_device->interface);
	libusb_close(dev->udev);
	libusb_exit(NULL);
	free(dev);

}

void usb_switch_to_nolo(struct usb_device_info * dev) {

	printf("\nSwitching to NOLO mode...\n");

	if ( dev->flash_device->protocol == FLASH_COLD )
		leave_cold_flash(dev);
	else if ( dev->flash_device->protocol == FLASH_MKII )
		mkii_reboot_device(dev);
	else if ( dev->flash_device->protocol == FLASH_DISK )
		printf_and_wait("Unplug USB cable, turn device off, press ENTER and plug USB cable again");

}

void usb_switch_to_cold(struct usb_device_info * dev) {

	printf("\nSwitching to Cold Flash mode...\n");

	if ( dev->flash_device->protocol == FLASH_NOLO )
		nolo_reboot_device(dev);
	else if ( dev->flash_device->protocol == FLASH_MKII )
		mkii_reboot_device(dev);
	else if ( dev->flash_device->protocol == FLASH_DISK )
		printf_and_wait("Unplug USB cable, turn device off, press ENTER and plug USB cable again");

}

void usb_switch_to_update(struct usb_device_info * dev) {

	printf("\nSwitching to Update mode...\n");

	if ( dev->flash_device->protocol == FLASH_COLD )
		leave_cold_flash(dev);
	else if ( dev->flash_device->protocol == FLASH_NOLO )
		nolo_boot_device(dev, "update");
	else if ( dev->flash_device->protocol == FLASH_MKII && ! ( dev->data & ( 1UL << 31 ) ) )
		mkii_reboot_device(dev);
	else if ( dev->flash_device->protocol == FLASH_DISK )
		printf_and_wait("Unplug USB cable, turn device off, press ENTER and plug USB cable again");

}

void usb_switch_to_disk(struct usb_device_info * dev) {

	printf("\nSwitching to RAW disk mode...\n");

	if ( dev->flash_device->protocol == FLASH_COLD )
		leave_cold_flash(dev);
	else if ( dev->flash_device->protocol == FLASH_NOLO ) {
		nolo_boot_device(dev, NULL);
		printf_and_wait("Wait until device start, choose USB Mass Storage Mode and press ENTER");
	} else if ( dev->flash_device->protocol == FLASH_MKII ) {
		if ( dev->data & ( 1UL << 31 ) )
			mkii_reboot_device(dev);
		else
			printf_and_wait("Unplug USB cable, plug again, choose USB Mass Storage Mode and press ENTER");
	}

}
