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

#include "global.h"
#include "device.h"
#include "usb-device.h"
#include "printf-utils.h"
#include "nolo.h"
#include "cold-flash.h"
#include "mkii.h"

#define USB_ERROR(str, err) do { PRINTF_END(); ERROR("%s: %s", str, libusb_strerror(err)); } while (0)

static libusb_context * usb_ctx;

static struct usb_flash_device usb_devices[] = {
	/* vend    prod    i   a   protocol     devices */
	{ 0x0421, 0x0096, -1, -1, FLASH_DISK, { DEVICE_RX_44, 0 } },
	{ 0x0421, 0x0105,  2,  1, FLASH_NOLO, { DEVICE_SU_18, DEVICE_RX_34, DEVICE_RX_44, DEVICE_RX_48, DEVICE_RX_51, DEVICE_RM_680, DEVICE_RM_696, 0 } },
	{ 0x0421, 0x0106,  0,  0, FLASH_COLD, { DEVICE_RX_51, DEVICE_RM_680, DEVICE_RM_696, 0 } },
	{ 0x0421, 0x0189, -1, -1, FLASH_DISK, { DEVICE_RX_48, 0 } },
	{ 0x0421, 0x01c7, -1, -1, FLASH_DISK, { DEVICE_RX_51, 0 } },
	{ 0x0421, 0x01c8,  1,  1, FLASH_MKII, { DEVICE_RX_51, DEVICE_RM_680, 0 } },
	{ 0x0421, 0x01c8,  1,  0, FLASH_UNKN, { DEVICE_RX_51, 0 } }, /* U-Boot mode (TTY via USB) */
	{ 0x0421, 0x03d1, -1, -1, FLASH_DISK, { DEVICE_RM_680, 0 } },
	{ 0x0421, 0x03d2,  1,  1, FLASH_MKII, { DEVICE_RM_680, 0 } },
	{ 0x0421, 0x0431, -1, -1, FLASH_DISK, { DEVICE_SU_18, DEVICE_RX_34, 0 } },
	{ 0x0421, 0x04c3, -1, -1, FLASH_DISK, { DEVICE_RX_34, 0 } },
	{ 0x0421, 0x0518, -1, -1, FLASH_DISK, { DEVICE_RM_696, 0 } },
	{ 0x0421, 0x0519, -1, -1, FLASH_UNKN, { DEVICE_RM_696, 0 } }, /* RNDIS/Ethernet mode */
	{ 0x0421, 0x051a,  2,  1, FLASH_UNKN, { DEVICE_RM_696, 0 } }, /* Sync mode (ADL protocol) */
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

static void usb_reattach_kernel_driver(libusb_device_handle * udev, int interface, int detached) {

	if ( interface < 0 )
		return;

	libusb_release_interface(udev, interface);

	/* Reattach only driver detached by us, on macOS reattaching re-enumerates (resets) device */
	if ( detached )
		libusb_attach_kernel_driver(udev, interface);

}

static void usb_descriptor_info_print(libusb_device_handle * udev, const struct libusb_device_descriptor * desc, char * product, size_t size) {

	char buf[1024];
	char buf2[1024];
	unsigned int x;
	int ret;
	int i;

	memset(buf, 0, sizeof(buf));
	libusb_get_string_descriptor_ascii(udev, desc->iProduct, (unsigned char *)buf, sizeof(buf));
	PRINTF_LINE("USB device product string: %s", buf[0] ? buf : "(not detected)");
	PRINTF_END();

	if ( product && buf[0] )
		strncpy(product, buf, size);

	memset(buf, 0, sizeof(buf));
	memset(buf2, 0, sizeof(buf2));
	ret = libusb_get_string_descriptor_ascii(udev, desc->iSerialNumber, (unsigned char *)buf, sizeof(buf));
	if ( ! isalnum(buf[0]) )
		buf[0] = 0;
	for ( i = 0; i < ret; i+=2 ) {
		sscanf(buf+i, "%2x", &x);
		if ( x > 31 && x < 127 )
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

static int usb_device_has_interface(libusb_device * dev, int interface, int alternate) {

	struct libusb_config_descriptor * config;
	int ret = 1;

	if ( interface == -1 )
		return 1;

	if ( libusb_get_config_descriptor(dev, 0, &config) < 0 )
		return 0;

	if ( interface >= config->bNumInterfaces )
		ret = 0;
	else if ( alternate != -1 && alternate >= config->interface[interface].num_altsetting )
		ret = 0;

	libusb_free_config_descriptor(config);
	return ret;

}

static struct usb_device_info * usb_device_is_valid(libusb_device * dev) {

	size_t i;
	int err;
	int detached = 0;
	char product[1024];
	struct libusb_device_descriptor desc;
	struct usb_device_info * ret = NULL;

	if ( libusb_get_device_descriptor(dev, &desc) < 0 )
		return NULL;

	for ( i = 0; i < sizeof(usb_devices)/sizeof(usb_devices[0]); ++i ) {

		if ( desc.idVendor == usb_devices[i].vendor && desc.idProduct == usb_devices[i].product ) {

			if ( usb_devices[i].protocol == FLASH_UNKN )
				continue;

			if ( desc.bNumConfigurations < 1 )
				continue;

			if ( ! usb_device_has_interface(dev, usb_devices[i].interface, usb_devices[i].alternate) )
				continue;

			printf("\b\b  ");
			PRINTF_END();
			PRINTF_ADD("Found ");
			usb_flash_device_info_print(&usb_devices[i]);
			PRINTF_END();

			PRINTF_LINE("Opening USB...");
			libusb_device_handle * udev;
			err = libusb_open(dev, &udev);
			if ( err < 0 ) {
				USB_ERROR("libusb_open failed", err);
				fprintf(stderr, "\n");
				return NULL;
			}

			memset(product, 0, sizeof(product));
			usb_descriptor_info_print(udev, &desc, product, sizeof(product));

			if ( usb_devices[i].interface >= 0 ) {

				if ( libusb_kernel_driver_active(udev, usb_devices[i].interface) == 1 ) {
					PRINTF_LINE("Detaching kernel from USB interface...");
					err = libusb_detach_kernel_driver(udev, usb_devices[i].interface);
					if ( err == 0 )
						detached = 1;
					else {
						PRINTF_END();
						WARNING("Cannot detach kernel driver: %s", libusb_strerror(err));
					}
				}

				PRINTF_LINE("Claiming USB interface...");
				err = libusb_claim_interface(udev, usb_devices[i].interface);
				if ( err < 0 ) {
					USB_ERROR("libusb_claim_interface failed", err);
					fprintf(stderr, "\n");
					usb_reattach_kernel_driver(udev, usb_devices[i].interface, detached);
					libusb_close(udev);
					return NULL;
				}

			}

			if ( usb_devices[i].alternate >= 0 ) {
				PRINTF_LINE("Setting alternate USB interface...");
				err = libusb_set_interface_alt_setting(udev, usb_devices[i].interface, usb_devices[i].alternate);
				if ( err < 0 ) {
					USB_ERROR("libusb_set_interface_alt_setting failed", err);
					fprintf(stderr, "\n");
					usb_reattach_kernel_driver(udev, usb_devices[i].interface, detached);
					libusb_close(udev);
					return NULL;
				}
			}

			ret = calloc(1, sizeof(struct usb_device_info));
			if ( ! ret ) {
				ALLOC_ERROR();
				usb_reattach_kernel_driver(udev, usb_devices[i].interface, detached);
				libusb_close(udev);
				return NULL;
			}

			if ( strcmp(product, "Nokia 770") == 0 || strcmp(product, "Nokia 770 (Update mode)") == 0 )
				ret->device = DEVICE_SU_18;
			else if ( strcmp(product, "Nokia N800 Internet Tablet") == 0 || strcmp(product, "Nokia N800 (Update mode)") == 0 )
				ret->device = DEVICE_RX_34;
			else if ( strcmp(product, "Nokia N810 Internet Tablet") == 0 || strcmp(product, "Nokia N810 (Update mode)") == 0 )
				ret->device = DEVICE_RX_44;
			else if ( strcmp(product, "Nokia N810 Internet Tablet WiMAX Edition") == 0 || strcmp(product, "Nokia-RX48 (Update mode)") == 0 )
				ret->device = DEVICE_RX_48;
			else if ( strcmp(product, "N900 (Storage Mode)") == 0 || strcmp(product, "Nokia N900 (Update mode)") == 0 || strcmp(product, "N900 (PC-Suite Mode)") == 0 || strcmp(product, "N900 (U-Boot)") == 0 )
				ret->device = DEVICE_RX_51;
			else if ( strcmp(product, "Nokia N950") == 0 || strcmp(product, "N950 (Update mode)") == 0 )
				ret->device = DEVICE_RM_680;
			else if ( strcmp(product, "Nokia N9") == 0 || strcmp(product, "N9 (Update mode)") == 0 || strcmp(product, "Nokia N9 RNDIS/Ethernet") == 0 )
				ret->device = DEVICE_RM_696;
			else if ( strcmp(product, "Nokia USB ROM") == 0 || strcmp(product, "Sync Mode") == 0 || strcmp(product, "Nxy (Update mode)") == 0 )
				ret->device = DEVICE_ANY;
			else
				ret->device = DEVICE_UNKNOWN;

			if ( device_to_string(ret->device) )
				PRINTF_LINE("Detected USB device: %s", device_to_string(ret->device));
			else
				PRINTF_LINE("Detected USB device: (not detected)");
			PRINTF_END();

			if ( ! noverify && ret->device == DEVICE_UNKNOWN ) {
				ERROR("Device detection failed");
				fprintf(stderr, "\n");
				usb_reattach_kernel_driver(udev, usb_devices[i].interface, detached);
				libusb_close(udev);
				free(ret);
				return NULL;
			}

			if ( ! noverify && ret->device != DEVICE_ANY ) {
				enum device * device;
				for ( device = usb_devices[i].devices; *device; ++device )
					if ( *device == ret->device )
						break;
				if ( ! *device ) {
					ERROR("Device mishmash");
					fprintf(stderr, "\n");
					usb_reattach_kernel_driver(udev, usb_devices[i].interface, detached);
					libusb_close(udev);
					free(ret);
					return NULL;
				}
			}

			ret->hwrev = -1;
			ret->flash_device = &usb_devices[i];
			ret->udev = udev;
			ret->detached = detached;
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

static void usb_exit(void) {

	libusb_exit(usb_ctx);
	usb_ctx = NULL;

}

struct usb_device_info * usb_open_and_wait_for_device(void) {

	libusb_device ** devs;
	ssize_t count;
	ssize_t j;
	struct usb_device_info * ret = NULL;
	int i = 0;
	int err;
	void (*prev)(int);
	static char progress[] = {'/','-','\\', '|'};

	/* Context is kept until exit, so device list stays up to date between waits */
	if ( ! usb_ctx ) {
		err = libusb_init(&usb_ctx);
		if ( err < 0 ) {
			USB_ERROR("libusb_init failed", err);
			return NULL;
		}
		atexit(usb_exit);
	}

	PRINTF_BACK();
	printf("\n");

	signal_quit = 0;

	prev = signal(SIGINT, signal_handler);

	while ( ! signal_quit ) {

		PRINTF_LINE("Waiting for USB device... %c", progress[++i%sizeof(progress)]);

		count = libusb_get_device_list(usb_ctx, &devs);
		if ( count < 0 ) {
			USB_ERROR("libusb_get_device_list failed", (int)count);
			break;
		}

		for ( j = 0; j < count; ++j ) {
			ret = usb_device_is_valid(devs[j]);
			if ( ret )
				break;
		}

		/* Opened device handle holds its own reference */
		libusb_free_device_list(devs, 1);

		if ( ret )
			break;

		MSLEEP(50);

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

	if ( dev->flash_device->protocol != FLASH_COLD )
		usb_reattach_kernel_driver(dev->udev, dev->flash_device->interface, dev->detached);
	libusb_close(dev->udev);
	free(dev);

}

/* Translate libusb error, so error messages which print errno stay meaningful */
static int usb_error_to_errno(int err) {

	switch ( err ) {
	case LIBUSB_ERROR_INVALID_PARAM: return EINVAL;
	case LIBUSB_ERROR_ACCESS: return EACCES;
	case LIBUSB_ERROR_NO_DEVICE: return ENODEV;
	case LIBUSB_ERROR_NOT_FOUND: return ENOENT;
	case LIBUSB_ERROR_BUSY: return EBUSY;
	case LIBUSB_ERROR_TIMEOUT: return ETIMEDOUT;
	case LIBUSB_ERROR_OVERFLOW: return EOVERFLOW;
	case LIBUSB_ERROR_PIPE: return EPIPE;
	case LIBUSB_ERROR_INTERRUPTED: return EINTR;
	case LIBUSB_ERROR_NO_MEM: return ENOMEM;
	case LIBUSB_ERROR_NOT_SUPPORTED: return ENOTSUP;
	default: return EIO;
	}

}

/* Returns number of transferred bytes or negative libusb error code */
static int usb_bulk_transfer(libusb_device_handle * udev, unsigned char endpoint, unsigned char * data, int size, unsigned int timeout) {

	int transferred = 0;
	int ret;

	ret = libusb_bulk_transfer(udev, endpoint, data, size, &transferred, timeout);
	if ( ret == 0 )
		return transferred;

	errno = usb_error_to_errno(ret);

	/* Timed out transfer which moved some data is reported as short transfer, callers check its size */
	if ( ret == LIBUSB_ERROR_TIMEOUT && transferred > 0 )
		return transferred;

	return ret;

}

int usb_bulk_send(libusb_device_handle * udev, unsigned char endpoint, const void * data, int size, unsigned int timeout) {

	/* libusb does not modify buffer of OUT transfer */
	return usb_bulk_transfer(udev, endpoint, (unsigned char *)data, size, timeout);

}

int usb_bulk_recv(libusb_device_handle * udev, unsigned char endpoint, void * data, int size, unsigned int timeout) {

	return usb_bulk_transfer(udev, endpoint, data, size, timeout);

}

void usb_switch_to_nolo(struct usb_device_info * dev) {

	printf("\nSwitching to NOLO mode...\n");

	if ( dev->flash_device->protocol == FLASH_COLD )
		leave_cold_flash(dev);
	else if ( dev->flash_device->protocol == FLASH_MKII )
		mkii_reboot_device(dev, 0);
	else if ( dev->flash_device->protocol == FLASH_DISK )
		printf_and_wait("Unplug USB cable, turn device off, press ENTER and plug USB cable again");

}

void usb_switch_to_cold(struct usb_device_info * dev) {

	printf("\nSwitching to Cold Flash mode...\n");

	if ( dev->flash_device->protocol == FLASH_NOLO )
		nolo_reboot_device(dev);
	else if ( dev->flash_device->protocol == FLASH_MKII )
		mkii_reboot_device(dev, 0);
	else if ( dev->flash_device->protocol == FLASH_DISK )
		printf_and_wait("Unplug USB cable, turn device off, press ENTER and plug USB cable again");

}

void usb_switch_to_update(struct usb_device_info * dev) {

	printf("\nSwitching to Update mode...\n");

	if ( dev->flash_device->protocol == FLASH_COLD )
		leave_cold_flash(dev);
	else if ( dev->flash_device->protocol == FLASH_NOLO )
		nolo_boot_device(dev, "update");
	else if ( dev->flash_device->protocol == FLASH_MKII && ! ( dev->data & MKII_UPDATE_MODE ) )
		mkii_reboot_device(dev, 1);
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
		if ( dev->data & MKII_UPDATE_MODE )
			mkii_reboot_device(dev, 0);
		else
			printf_and_wait("Unplug USB cable, plug again, choose USB Mass Storage Mode and press ENTER");
	}

}
