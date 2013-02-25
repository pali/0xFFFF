/*
 *  0xFFFF - Open Free Fiasco Firmware Flasher
 *  Copyright (C) 2007-2009  pancake <pancake@youterm.com>
 *  Copyright (C) 2012  Pali Rohár <pali.rohar@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include <usb.h>

#include "nolo.h"
#include "image.h"
#include "global.h"
#include "printf-utils.h"

/* Request type */
#define NOLO_WRITE		64
#define NOLO_QUERY		192

/* Request */
#define NOLO_STATUS		1
#define NOLO_GET_NOLO_VERSION	3
#define NOLO_IDENTIFY		4
#define NOLO_ERROR_LOG		5
#define NOLO_SET		16
#define NOLO_GET		17
#define NOLO_STRING		18
#define NOLO_SET_STRING		19
#define NOLO_GET_STRING		20
#define NOLO_SEND_IMAGE		66
#define NOLO_SET_SW_RELEASE	67
#define NOLO_FLASH_IMAGE	80
#define NOLO_SEND_FLASH_FINISH	82
#define NOLO_SEND_FLASH_IMAGE	84
#define NOLO_BOOT		130
#define NOLO_REBOOT		131

/* Indexes - Flash image */
#define NOLO_IMAGE_BOOTLOADER	1
#define NOLO_IMAGE_KERNEL	3
#define NOLO_IMAGE_INITFS	4
#define NOLO_IMAGE_CMT		66

/* Indexes - Set & Get */
#define NOLO_RD_MODE		0
#define NOLO_ROOT_DEVICE	1
#define NOLO_USB_HOST_MODE	2
#define NOLO_ADD_RD_FLAGS	3
#define NOLO_DEL_RD_FLAGS	4

/* Values - R&D flags */
#define NOLO_RD_FLAG_NO_OMAP_WD		0x002
#define NOLO_RD_FLAG_NO_EXT_WD		0x004
#define NOLO_RD_FLAG_NO_LIFEGUARD	0x008
#define NOLO_RD_FLAG_SERIAL_CONSOLE	0x010
#define NOLO_RD_FLAG_NO_USB_TIMEOUT	0x020
#define NOLO_RD_FLAG_STI_CONSOLE	0x040
#define NOLO_RD_FLAG_NO_CHARGING	0x080
#define NOLO_RD_FLAG_FORCE_POWER_KEY	0x100

/* Values - Boot mode */
#define NOLO_BOOT_MODE_NORMAL		0
#define NOLO_BOOT_MODE_UPDATE		1

#define NOLO_ERROR_RETURN(str, ...) do { nolo_error_log(dev, str == NULL); ERROR_RETURN(str, __VA_ARGS__); } while (0)

static void nolo_error_log(struct usb_device_info * dev, int only_clear) {

	char buf[2048];
	size_t i, count;

	for ( count = 0; count < 20; ++count ) {

		memset(buf, 0, sizeof(buf));

		if ( usb_control_msg(dev->udev, NOLO_QUERY, NOLO_ERROR_LOG, 0, 0, buf, sizeof(buf), 2000) <= 0 )
			break;

		if ( ! only_clear ) {

			for ( i = 0; i < sizeof(buf)-2; ++i )
				if ( buf[i] == 0 && buf[i+1] != 0 )
					buf[i] = '\n';

			buf[sizeof(buf)-1] = 0;
			puts(buf);

		}

	}

}

static int nolo_identify_string(struct usb_device_info * dev, const char * str, char * out, size_t size) {

	char buf[512];
	char * ptr;
	int ret;

	memset(buf, 0, sizeof(buf));

	ret = usb_control_msg(dev->udev, NOLO_QUERY, NOLO_IDENTIFY, 0, 0, (char *)buf, sizeof(buf), 2000);
	if ( ret < 0 )
		NOLO_ERROR_RETURN("NOLO_IDENTIFY failed", -1);

	ptr = memmem(buf, ret, str, strlen(str));
	if ( ! ptr )
		ERROR_RETURN("Substring was not found", -1);

	ptr += strlen(str) + 1;

	while ( ptr-buf < ret && *ptr < 32 )
		++ptr;

	if ( ! *ptr )
		return -1;

	strncpy(out, ptr, size-1);
	out[size-1] = 0;
	return strlen(out);

}

static int nolo_set_string(struct usb_device_info * dev, char * str, char * arg) {

	if ( simulate )
		return 0;

	if ( usb_control_msg(dev->udev, NOLO_WRITE, NOLO_STRING, 0, 0, str, strlen(str), 2000) < 0 )
		NOLO_ERROR_RETURN("NOLO_STRING failed", -1);

	if ( usb_control_msg(dev->udev, NOLO_WRITE, NOLO_SET_STRING, 0, 0, arg, strlen(arg), 2000) < 0 )
		NOLO_ERROR_RETURN("NOLO_SET_STRING failed", -1);

	return 0;

}

static int nolo_get_string(struct usb_device_info * dev, char * str, char * out, size_t size) {

	int ret = 0;

	if ( usb_control_msg(dev->udev, NOLO_WRITE, NOLO_STRING, 0, 0, str, strlen(str), 2000) < 0 )
		return -1;

	if ( ( ret = usb_control_msg(dev->udev, NOLO_QUERY, NOLO_GET_STRING, 0, 0, out, size-1, 2000) ) < 0 )
		return -1;

	out[size-1] = 0;
	out[ret] = 0;
	return strlen(out);

}

static int nolo_get_version_string(struct usb_device_info * dev, const char * str, char * out, size_t size) {

	int ret;
	char buf[512];

	if ( strlen(str) > 500 )
		return -1;

	sprintf(buf, "version:%s", str);

	ret = nolo_get_string(dev, buf, out, size);

	nolo_error_log(dev, 1);

	if ( ret < 0 )
		return ret;

	if ( ! out[0] )
		return -1;

	return ret;

}

int nolo_init(struct usb_device_info * dev) {

	uint32_t val = 1;
	enum device device;

	printf("Initializing NOLO...\n");

	while ( val != 0 )
		if ( usb_control_msg(dev->udev, NOLO_QUERY, NOLO_STATUS, 0, 0, (char *)&val, 4, 2000) == -1 )
			NOLO_ERROR_RETURN("NOLO_STATUS failed", -1);

	/* clear error log */
	nolo_error_log(dev, 1);

	device = nolo_get_device(dev);

	if ( ! dev->device )
		dev->device = device;

	if ( dev->device && device && dev->device != device ) {
		ERROR("Device mishmash, expected %s, got %s", device_to_string(dev->device), device_to_string(device));
		return -1;
	}

	dev->hwrev = nolo_get_hwrev(dev);

	return 0;
}

enum device nolo_get_device(struct usb_device_info * dev) {

	char buf[20];
	if ( nolo_identify_string(dev, "prod_code", buf, sizeof(buf)) < 0 )
		return DEVICE_UNKNOWN;
	else if ( buf[0] )
		return device_from_string(buf);
	else
		return DEVICE_UNKNOWN;

}

static int nolo_send_image(struct usb_device_info * dev, struct image * image, int flash) {

	char buf[0x20000];
	char * ptr;
	const char * type;
	uint8_t len;
	uint16_t hash;
	uint32_t size;
	uint32_t need;
	uint32_t readed;
	int request;
	int ret;

	if ( flash )
		printf("Send and flash image:\n");
	else
		printf("Load image:\n");
	image_print_info(image);

	if ( image->type == IMAGE_2ND || image->type == IMAGE_MMC )
		ERROR_RETURN("Sending 2nd and mmc images are not supported", -1);

	if ( ! flash && image->type == IMAGE_ROOTFS )
		ERROR_RETURN("Rootfs image must be sent in flash mode", -1);

	ptr = buf;

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
			snprintf(buf, 8, "%d", dev->hwrev);

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

	if ( flash )
		request = NOLO_SEND_FLASH_IMAGE;
	else
		request = NOLO_SEND_IMAGE;

	printf("Sending image header...\n");

	if ( ! simulate ) {
		if ( usb_control_msg(dev->udev, NOLO_WRITE, request, 0, 0, buf, ptr-buf, 2000) < 0 )
			NOLO_ERROR_RETURN("Sending image header failed", -1);
	}

	if ( flash )
		printf("Sending and flashing image...\n");
	else
		printf("Sending image...\n");
	printf_progressbar(0, image->size);
	image_seek(image, 0);
	readed = 0;
	while ( readed < image->size ) {
		need = image->size - readed;
		if ( need > sizeof(buf) )
			need = sizeof(buf);
		ret = image_read(image, buf, need);
		if ( ret == 0 )
			break;
		if ( ! simulate ) {
			if ( usb_bulk_write(dev->udev, 2, buf, ret, 5000) != ret ) {
				PRINTF_END();
				NOLO_ERROR_RETURN("Sending image failed", -1);
			}
		}
		readed += ret;
		printf_progressbar(readed, image->size);
	}

	if ( flash ) {
		printf("Finishing flashing...\n");
		if ( ! simulate ) {
			if ( usb_control_msg(dev->udev, NOLO_WRITE, NOLO_SEND_FLASH_FINISH, 0, 0, NULL, 0, 30000) < 0 )
				NOLO_ERROR_RETURN("Finishing failed", -1);
		}
	}

	printf("Done\n");

	return 0;

}

int nolo_load_image(struct usb_device_info * dev, struct image * image) {

	if ( image->type != IMAGE_KERNEL && image->type != IMAGE_INITFS )
		ERROR_RETURN("Only kernel or initfs image can be loaded", -1);

	return nolo_send_image(dev, image, 0);

}

int nolo_flash_image(struct usb_device_info * dev, struct image * image) {

	int ret;
	int flash;
	int index;
	unsigned long long int part;
	unsigned long long int total;
	unsigned long long int last_total;
	char status[20];
	char buf[128];

	if ( image->type == IMAGE_ROOTFS )
		flash = 1;
	else
		flash = 0;

	ret = nolo_send_image(dev, image, flash);
	if ( ret < 0 )
		return ret;

	if ( image->type == IMAGE_SECONDARY )
		index = NOLO_IMAGE_BOOTLOADER;
	else if ( image->type == IMAGE_KERNEL )
		index = NOLO_IMAGE_KERNEL;
	else if ( image->type == IMAGE_CMT_MCUSW )
		index = NOLO_IMAGE_CMT;
	else
		index = -1;

	if ( image->type == IMAGE_CMT_MCUSW )
		nolo_set_string(dev, "cmt:verify", "1");

	if ( ! flash && index >= 0 ) {

		printf("Flashing image...\n");

		if ( ! simulate ) {
			if ( usb_control_msg(dev->udev, NOLO_WRITE, NOLO_FLASH_IMAGE, 0, index, NULL, 0, 10000) )
				NOLO_ERROR_RETURN("Flashing failed", -1);
		}

		printf("Done\n");

	}

	if ( image->type == IMAGE_CMT_MCUSW ) {

		int state = 0;
		last_total = 0;

		if ( nolo_get_string(dev, "cmt:status", buf, sizeof(buf)) < 0 )
			NOLO_ERROR_RETURN("cmt:status failed", -1);

		if ( strncmp(buf, "idle", strlen("idle")) == 0 )
			state = 4;
		else
			printf("Erasing CMT...\n");

		while ( state != 4 ) {

			if ( nolo_get_string(dev, "cmt:status", buf, sizeof(buf)) < 0 ) {
				PRINTF_END();
				NOLO_ERROR_RETURN("cmt:status failed", -1);
			}

			if ( strncmp(buf, "finished", strlen("finished")) == 0 ) {

				if ( state <= 0 ) {
					printf_progressbar(last_total, last_total);
					printf("Done\n");
				}
				if ( state <= 1 )
					printf("Programming CMT...\n");
				if ( state <= 2 ) {
					printf_progressbar(last_total, last_total);
					printf("Done\n");
				}

				state = 4;

			} else {

				if ( sscanf(buf, "%s:%llu/%llu", status, &part, &total) != 3 )
					PRINTF_ERROR_RETURN("cmt:status unknown", -1);

				if ( strcmp(status, "program") == 0 && state <= 0 ) {
					printf_progressbar(last_total, last_total);
					printf("Done\n");
					state = 1;
				}

				if ( strcmp(status, "program") == 0 && state <= 1 ) {
					printf("Programming CMT...\n");
					state = 2;
				}

				printf_progressbar(part, total);
				last_total = total;

				if ( strcmp(status, "erase") == 0 && state <= 0 && part == total ) {
					printf("Done\n");
					state = 1;
				}

				if ( strcmp(status, "program") == 0 && state <= 2 && part == total ) {
					printf("Done\n");
					state = 3;
				}

			}

			usleep(0xc350); // 0.5s

		}

	}

	return 0;

}

int nolo_boot_device(struct usb_device_info * dev, const char * cmdline) {

	int size = 0;
	int mode = NOLO_BOOT_MODE_NORMAL;

	if ( cmdline && strncmp(cmdline, "update", strlen("update")) == 0 && cmdline[strlen("update")] <= 32 ) {
		mode = NOLO_BOOT_MODE_UPDATE;
		cmdline += strlen("update");
		if ( *cmdline ) ++cmdline;
		while ( *cmdline && *cmdline <= 32 )
			++cmdline;
		if ( *cmdline ) {
			printf("Booting kernel to update mode with cmdline: %s...\n", cmdline);
			size = strlen(cmdline);
		} else {
			printf("Booting kernel to update mode with default cmdline...\n");
			cmdline = NULL;
		}
	} else if ( cmdline && cmdline[0] ) {
		printf("Booting kernel with cmdline: '%s'...\n", cmdline);
		size = strlen(cmdline)+1;
	} else {
		printf("Booting kernel with default cmdline...\n");
		cmdline = NULL;
	}

	if ( usb_control_msg(dev->udev, NOLO_WRITE, NOLO_BOOT, mode, 0, (char *)cmdline, size, 2000) < 0 )
		NOLO_ERROR_RETURN("Booting failed", -1);

	return 0;

}

int nolo_reboot_device(struct usb_device_info * dev) {

	printf("Rebooting device...\n");
	if ( usb_control_msg(dev->udev, NOLO_WRITE, NOLO_REBOOT, 0, 0, NULL, 0, 2000) < 0 )
		NOLO_ERROR_RETURN("NOLO_REBOOT failed", -1);
	return 0;

}

int nolo_get_root_device(struct usb_device_info * dev) {

	uint8_t device = 0;
	if ( usb_control_msg(dev->udev, NOLO_QUERY, NOLO_GET, 0, NOLO_ROOT_DEVICE, (char *)&device, 1, 2000) < 0 )
		NOLO_ERROR_RETURN("Cannot get root device", -1);
	return device;

}

int nolo_set_root_device(struct usb_device_info * dev, int device) {

	printf("Setting root device to %d...\n", device);
	if ( simulate )
		return 0;
	if ( usb_control_msg(dev->udev, NOLO_WRITE, NOLO_SET, device, NOLO_ROOT_DEVICE, NULL, 0, 2000) < 0 )
		NOLO_ERROR_RETURN("Cannot set root device", -1);
	return 0;

}

int nolo_get_usb_host_mode(struct usb_device_info * dev) {

	uint32_t enabled = 0;
	if ( usb_control_msg(dev->udev, NOLO_QUERY, NOLO_GET, 0, NOLO_USB_HOST_MODE, (void *)&enabled, 4, 2000) < 0 )
		NOLO_ERROR_RETURN("Cannot get USB host mode status", -1);
	return enabled ? 1 : 0;

}

int nolo_set_usb_host_mode(struct usb_device_info * dev, int enable) {

	printf("%s USB host mode...\n", enable ? "Enabling" : "Disabling");
	if ( simulate )
		return 0;
	if ( usb_control_msg(dev->udev, NOLO_WRITE, NOLO_SET, enable, NOLO_USB_HOST_MODE, NULL, 0, 2000) < 0 )
		NOLO_ERROR_RETURN("Cannot change USB host mode status", -1);
	return 0;

}

int nolo_get_rd_mode(struct usb_device_info * dev) {

	uint8_t enabled = 0;
	if ( usb_control_msg(dev->udev, NOLO_QUERY, NOLO_GET, 0, NOLO_RD_MODE, (char *)&enabled, 1, 2000) < 0 )
		NOLO_ERROR_RETURN("Cannot get R&D mode status", -1);
	return enabled ? 1 : 0;

}

int nolo_set_rd_mode(struct usb_device_info * dev, int enable) {

	printf("%s R&D mode...\n", enable ? "Enabling" : "Disabling");
	if ( simulate )
		return 0;
	if ( usb_control_msg(dev->udev, NOLO_WRITE, NOLO_SET, enable, NOLO_RD_MODE, NULL, 0, 2000) < 0 )
		NOLO_ERROR_RETURN("Cannot change R&D mode status", -1);
	return 0;

}

#define APPEND_STRING(ptr, buf, size, string) do { if ( (int)size > ptr-buf ) ptr += snprintf(ptr, size-(ptr-buf), "%s,", string); } while (0)

int nolo_get_rd_flags(struct usb_device_info * dev, char * flags, size_t size) {

	uint16_t add_flags = 0;
	char * ptr = flags;

	if ( usb_control_msg(dev->udev, NOLO_QUERY, NOLO_GET, 0, NOLO_ADD_RD_FLAGS, (char *)&add_flags, 2, 2000) < 0 )
		NOLO_ERROR_RETURN("Cannot get R&D flags", -1);

	if ( add_flags & NOLO_RD_FLAG_NO_OMAP_WD )
		APPEND_STRING(ptr, flags, size, "no-omap-wd");
	if ( add_flags & NOLO_RD_FLAG_NO_EXT_WD )
		APPEND_STRING(ptr, flags, size, "no-ext-wd");
	if ( add_flags & NOLO_RD_FLAG_NO_LIFEGUARD )
		APPEND_STRING(ptr, flags, size, "no-lifeguard-reset");
	if ( add_flags & NOLO_RD_FLAG_SERIAL_CONSOLE )
		APPEND_STRING(ptr, flags, size, "serial-console");
	if ( add_flags & NOLO_RD_FLAG_NO_USB_TIMEOUT )
		APPEND_STRING(ptr, flags, size, "no-usb-timeout");
	if ( add_flags & NOLO_RD_FLAG_STI_CONSOLE )
		APPEND_STRING(ptr, flags, size, "sti-console");
	if ( add_flags & NOLO_RD_FLAG_NO_CHARGING )
		APPEND_STRING(ptr, flags, size, "no-charging");
	if ( add_flags & NOLO_RD_FLAG_FORCE_POWER_KEY )
		APPEND_STRING(ptr, flags, size, "force-power-key");

	if ( ptr != flags && *(ptr-1) == ',' )
		*(--ptr) = 0;

	return ptr-flags;

}

#undef APPEND_STRING

int nolo_set_rd_flags(struct usb_device_info * dev, const char * flags) {

	const char * ptr = flags;
	const char * endptr = ptr;

	int add_flags = 0;
	int del_flags = 0;

	if ( flags && flags[0] )
		printf("Setting R&D flags to: %s...\n", flags);
	else
		printf("Clearing all R&D flags...\n");

	while ( ptr && *ptr ) {

		while ( *ptr <= 32 )
			++ptr;

		if ( ! *ptr )
			break;

		endptr = strchr(ptr, ',');
		if ( endptr )
			endptr -= 1;
		else
			endptr = ptr+strlen(ptr);

		if ( strncmp("no-omap-wd", ptr, endptr-ptr) == 0 )
			add_flags |= NOLO_RD_FLAG_NO_OMAP_WD;
		if ( strncmp("no-ext-wd", ptr, endptr-ptr) == 0 )
			add_flags |= NOLO_RD_FLAG_NO_EXT_WD;
		if ( strncmp("no-lifeguard-reset", ptr, endptr-ptr) == 0 )
			add_flags |= NOLO_RD_FLAG_NO_LIFEGUARD;
		if ( strncmp("serial-console", ptr, endptr-ptr) == 0 )
			add_flags |= NOLO_RD_FLAG_SERIAL_CONSOLE;
		if ( strncmp("no-usb-timeout", ptr, endptr-ptr) == 0 )
			add_flags |= NOLO_RD_FLAG_NO_USB_TIMEOUT;
		if ( strncmp("sti-console", ptr, endptr-ptr) == 0 )
			add_flags |= NOLO_RD_FLAG_STI_CONSOLE;
		if ( strncmp("no-charging", ptr, endptr-ptr) == 0 )
			add_flags |= NOLO_RD_FLAG_NO_CHARGING;
		if ( strncmp("force-power-key", ptr, endptr-ptr) == 0 )
			add_flags |= NOLO_RD_FLAG_FORCE_POWER_KEY;

		if ( *(endptr+1) )
			ptr = endptr+2;
		else
			break;

	}

	if ( ! ( add_flags & NOLO_RD_FLAG_NO_OMAP_WD ) )
		del_flags |= NOLO_RD_FLAG_NO_OMAP_WD;
	if ( ! ( add_flags & NOLO_RD_FLAG_NO_EXT_WD ) )
		del_flags |= NOLO_RD_FLAG_NO_EXT_WD;
	if ( ! ( add_flags & NOLO_RD_FLAG_NO_LIFEGUARD ) )
		del_flags |= NOLO_RD_FLAG_NO_LIFEGUARD;
	if ( ! ( add_flags & NOLO_RD_FLAG_SERIAL_CONSOLE ) )
		del_flags |= NOLO_RD_FLAG_SERIAL_CONSOLE;
	if ( ! ( add_flags & NOLO_RD_FLAG_NO_USB_TIMEOUT ) )
		del_flags |= NOLO_RD_FLAG_NO_USB_TIMEOUT;
	if ( ! ( add_flags & NOLO_RD_FLAG_STI_CONSOLE ) )
		del_flags |= NOLO_RD_FLAG_STI_CONSOLE;
	if ( ! ( add_flags & NOLO_RD_FLAG_NO_CHARGING ) )
		del_flags |= NOLO_RD_FLAG_NO_CHARGING;
	if ( ! ( add_flags & NOLO_RD_FLAG_FORCE_POWER_KEY ) )
		del_flags |= NOLO_RD_FLAG_FORCE_POWER_KEY;

	if ( simulate )
		return 0;

	if ( usb_control_msg(dev->udev, NOLO_WRITE, NOLO_SET, add_flags, NOLO_ADD_RD_FLAGS, NULL, 0, 2000) < 0 )
		NOLO_ERROR_RETURN("Cannot add R&D flags", -1);

	if ( usb_control_msg(dev->udev, NOLO_WRITE, NOLO_SET, del_flags, NOLO_DEL_RD_FLAGS, NULL, 0, 2000) < 0 )
		NOLO_ERROR_RETURN("Cannot del R&D flags", -1);

	return 0;

}

int16_t nolo_get_hwrev(struct usb_device_info * dev) {

	char buf[10];
	if ( nolo_identify_string(dev, "hw_rev", buf, sizeof(buf)) <= 0 )
		return -1;
	return atoi(buf);

}

int nolo_set_hwrev(struct usb_device_info * dev, int16_t hwrev) {

	char buf[9];
	memset(buf, 0, sizeof(buf));
	snprintf(buf, 8, "%d", hwrev);
	printf("Setting HW revision to: %s\n", buf);
	return nolo_set_string(dev, "hw_rev", buf);

}

int nolo_get_kernel_ver(struct usb_device_info * dev, char * ver, size_t size) {

	return nolo_get_version_string(dev, "kernel", ver, size);

}

int nolo_set_kernel_ver(struct usb_device_info * dev, const char * ver) {

	printf("nolo_set_kernel_ver is not implemented yet\n");
	(void)dev;
	(void)ver;
	return -1;

}

int nolo_get_initfs_ver(struct usb_device_info * dev, char * ver, size_t size) {

	return nolo_get_version_string(dev, "initfs", ver, size);

}

int nolo_set_initfs_ver(struct usb_device_info * dev, const char * ver) {

	printf("nolo_set_initfs_ver is not implemented yet\n");
	(void)dev;
	(void)ver;
	return -1;

}

int nolo_get_nolo_ver(struct usb_device_info * dev, char * ver, size_t size) {

	uint32_t version = 0;

	if ( usb_control_msg(dev->udev, NOLO_QUERY, NOLO_GET_NOLO_VERSION, 0, 0, (char *)&version, 4, 2000) < 0 )
		NOLO_ERROR_RETURN("Cannot get NOLO version", -1);

	if ( (version & 255) > 1 )
		NOLO_ERROR_RETURN("Invalid NOLO version", -1);

	return snprintf(ver, size, "%d.%d.%d", version >> 20 & 15, version >> 16 & 15, version >> 8 & 255);

}

int nolo_set_nolo_ver(struct usb_device_info * dev, const char * ver) {

	printf("nolo_set_nolo_ver is not implemented yet\n");
	(void)dev;
	(void)ver;
	return -1;

}

int nolo_get_sw_ver(struct usb_device_info * dev, char * ver, size_t size) {

	return nolo_get_version_string(dev, "sw-release", ver, size);

}

int nolo_set_sw_ver(struct usb_device_info * dev, const char * ver) {

	char buf[512];
	char * ptr;
	uint8_t len;
	const char * str = "OSSO UART+USB";

	printf("Setting Software release string to: %s\n", ver);

	if ( strlen(ver) > UINT8_MAX )
		NOLO_ERROR_RETURN("Software release string is too long", -1);

	ptr = buf;

	memcpy(ptr, "\xe8", 1);
	ptr += 1;

	len = strlen(str)+1;
	memcpy(ptr, &len, 1);
	ptr += 1;

	memcpy(ptr, str, len);
	ptr += len;

	memcpy(ptr, "\x31", 1);
	ptr += 1;

	len = strlen(ver)+1;
	memcpy(ptr, &len, 1);
	ptr += 1;

	memcpy(ptr, ver, len);
	ptr += len;

	if ( usb_control_msg(dev->udev, NOLO_WRITE, NOLO_SET_SW_RELEASE, 0, 0, buf, ptr-buf, 2000) < 0 )
		NOLO_ERROR_RETURN("NOLO_SET_SW_RELEASE failed", -1);

	return 0;

}

int nolo_get_content_ver(struct usb_device_info * dev, char * ver, size_t size) {

	return nolo_get_version_string(dev, "content", ver, size);

}

int nolo_set_content_ver(struct usb_device_info * dev, const char * ver) {

	printf("nolo_set_content_ver is not implemented yet\n");
	(void)dev;
	(void)ver;
	return -1;

}
