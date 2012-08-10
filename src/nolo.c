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
#define NOLO_SET		16
#define NOLO_GET		17
#define NOLO_REQUEST_VERSION	18
#define NOLO_RESPONCE_VERSION	20
#define NOLO_BOOT		130
#define NOLO_REBOOT		131

/* Index */
#define NOLO_RD_MODE		0
#define NOLO_ROOT_DEVICE	1
#define NOLO_USB_HOST_MODE	2

int nolo_init(struct usb_device_info * dev) {

	uint32_t val = 1;

	printf("Initializing NOLO...\n");
	while ( val != 0 )
		if ( usb_control_msg(dev->udev, NOLO_QUERY, NOLO_STATUS, 0, 0, (char *)&val, 4, 2000) == -1 )
			ERROR_RETURN("NOLO_STATUS failed", -1);

	return 0;
}

int nolo_identify_string(struct usb_device_info * dev, const char * str, char * out, size_t size) {

	char buf[512];
	char * ptr;
	int ret;

	memset(buf, 0, sizeof(buf));

	ret = usb_control_msg(dev->udev, NOLO_QUERY, NOLO_IDENTIFY, 0, 0, (char *)buf, sizeof(buf), 2000);
	if ( ret < 0 )
		ERROR_RETURN("NOLO_IDENTIFY failed", -1);

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

enum device nolo_get_device(struct usb_device_info * dev) {

	char buf[20];
	if ( nolo_identify_string(dev, "prod_code", buf, sizeof(buf)) < 0 )
		return DEVICE_UNKNOWN;
	else
		return device_from_string(buf);

}

int nolo_load_image(struct usb_device_info * dev, struct image * image) {

}

int nolo_flash_image(struct usb_device_info * dev, struct image * image) {

}

int nolo_boot(struct usb_device_info * dev, char * cmdline) {

	int size = 0;

	if ( cmdline && cmdline[0] ) {
		printf("Booting kernel with cmdline: '%s'...\n", cmdline);
		size = strlen(cmdline)+1;
	} else {
		printf("Booting kernel with default cmdline...\n");
		cmdline = NULL;
	}

	if ( usb_control_msg(dev->udev, NOLO_WRITE, NOLO_BOOT, 0, 0, cmdline, size, 2000) < 0 )
		ERROR_RETURN("NOLO_BOOT failed", -1);

	return 0;

}

int nolo_boot_to_update_mode(struct usb_device_info * dev) {

}

int nolo_reboot_device(struct usb_device_info * dev) {

	printf("Rebooting device...\n");
	if ( usb_control_msg(dev->udev, NOLO_WRITE, NOLO_REBOOT, 0, 0, NULL, 0, 2000) < 0 )
		ERROR_RETURN("NOLO_REBOOT failed", -1);
	return 0;

}

int nolo_get_root_device(struct usb_device_info * dev) {

	uint8_t device;
	if ( usb_control_msg(dev->udev, NOLO_QUERY, NOLO_GET, 0, NOLO_ROOT_DEVICE, (char *)&device, 1, 2000) < 0 )
		ERROR_RETURN("Cannot get root device", -1);
	return device;

}

int nolo_set_root_device(struct usb_device_info * dev, int device) {

	printf("Setting root device to %d\n", device);
	if ( usb_control_msg(dev->udev, NOLO_WRITE, NOLO_SET, device, NOLO_ROOT_DEVICE, NULL, 0, 2000) < 0 )
		ERROR_RETURN("Cannot set root device", -1);
	return 0;

}

int nolo_get_usb_host_mode(struct usb_device_info * dev) {

	uint32_t enabled;
	if ( usb_control_msg(dev->udev, NOLO_QUERY, NOLO_GET, 0, NOLO_USB_HOST_MODE, (void *)&enabled, 4, 2000) < 0 )
		ERROR_RETURN("Cannot get USB host mode status", -1);
	return enabled ? 1 : 0;

}

int nolo_set_usb_host_mode(struct usb_device_info * dev, int enable) {

	printf("%s USB host mode\n", enable ? "Enabling" : "Disabling");
	if ( usb_control_msg(dev->udev, NOLO_WRITE, NOLO_SET, enable, NOLO_USB_HOST_MODE, NULL, 0, 2000) < 0 )
		ERROR_RETURN("Cannot change USB host mode status", -1);
	return 0;

}

int nolo_get_rd_mode(struct usb_device_info * dev) {

	uint8_t enabled;
	if ( usb_control_msg(dev->udev, NOLO_QUERY, NOLO_GET, 0, NOLO_RD_MODE, (char *)&enabled, 1, 2000) < 0 )
		ERROR_RETURN("Cannot get R&D mode status", -1);
	return enabled ? 1 : 0;

}

int nolo_set_rd_mode(struct usb_device_info * dev, int enable) {

	printf("%s R&D mode\n", enable ? "Enabling" : "Disabling");
	if ( usb_control_msg(dev->udev, NOLO_WRITE, NOLO_SET, enable, NOLO_RD_MODE, NULL, 0, 2000) < 0 )
		ERROR_RETURN("Cannot chnage R&D mode status", -1);
	return 0;

}

int nolo_get_rd_flags(struct usb_device_info * dev, char * flags, size_t size) {

}

int nolo_set_rd_flags(struct usb_device_info * dev, const char * flags) {

}

int nolo_get_hwrev(struct usb_device_info * dev, char * hwrev, size_t size) {

	return nolo_identify_string(dev, "hw_rev", hwrev, size);

}

int nolo_set_hwrev(struct usb_device_info * dev, const char * hwrev) {

}

int nolo_get_kernel_ver(struct usb_device_info * dev, char * ver, size_t size) {

}

int nolo_set_kernel_ver(struct usb_device_info * dev, const char * ver) {

}

int nolo_get_nolo_ver(struct usb_device_info * dev, char * ver, size_t size) {

	uint32_t version;

	if ( usb_control_msg(dev->udev, NOLO_QUERY, NOLO_GET_NOLO_VERSION, 0, 0, (char *)&version, 4, 2000) < 0 )
		ERROR_RETURN("Cannot get NOLO version", -1);

	if ( (version & 255) > 1 )
		ERROR_RETURN("Invalid NOLO version", -1);

	return snprintf(ver, size, "%d.%d.%d", version >> 20 & 15, version >> 16 & 15, version >> 8 & 255);

}

int nolo_set_nolo_ver(struct usb_device_info * dev, const char * ver) {

}

int nolo_get_sw_ver(struct usb_device_info * dev, char * ver, size_t size) {

	char buf[512];
	strcpy(buf, "version:sw-release");

	if ( usb_control_msg(dev->udev, NOLO_WRITE, NOLO_REQUEST_VERSION, 0, 0, (char *)&buf, strlen(buf), 2000) < 0 )
		ERROR_RETURN("Cannot request SW Release version", -1);

	memset(buf, 0, sizeof(buf));

	if ( usb_control_msg(dev->udev, NOLO_QUERY, NOLO_RESPONCE_VERSION, 0, 0, (char *)&buf, sizeof(buf), 2000) < 0 )
		ERROR_RETURN("Cannot get SW Release version", -1);

	if ( ! buf[0] )
		return -1;

	strncpy(ver, buf, size-1);
	ver[size-1] = 0;
	return strlen(ver);

}

int nolo_set_sw_ver(struct usb_device_info * dev, const char * ver) {

}

int nolo_get_content_ver(struct usb_device_info * dev, char * ver, size_t size) {

}

int nolo_set_content_ver(struct usb_device_info * dev, const char * ver) {

}


/**** OLD CODE ****/


//#define CMD_WRITE 64
//#define CMD_QUERY 192

//struct usb_dev_handle *dev;

//static void query_error_message()
//{
	/* query error message */
/*	int len = 0;
	char nolomsg[2048];
	memset(nolomsg, '\0', 2048);
	usb_control_msg(dev, 192, 5, 0, 0, nolomsg, 2048, 2000);
	nolomsg[2047] = '\0';
	printf("\nNOLO says:\n");
	if (nolomsg[0] == '\0') {
		printf(" (.. silence ..)\n");
	} else {
//		dump_bytes((unsigned char *)nolomsg, 128);
		do {
			printf(" - %s\n", nolomsg+len);
			len+=strlen(nolomsg+len)+1;
		} while(nolomsg[len]!='\0');
	}
}*/

//void flash_image(struct image * image)
//{
//	FILE *fd;
//	int vlen = 0;
//	int request;
	/*/ n800 flash queries have a variable size */
//	unsigned char fquery[256]; /* flash query */
/*	unsigned long long size, off;
	unsigned char bsize[4], tmp;
	unsigned char nolofiller[128];
	const char * piece = image_type_to_string(image->type);
	const char * version = image->version;
	ushort hash = image->hash;

//	if (piece == NULL) {
		//exit(1);
//		piece = fpid_file(filename);
		if (piece == NULL) {
			printf("Unknown piece type\n");
			return;
		}
//		printf("Piece type: %s\n", piece);
//	}*/

/*	if (piece != NULL) {
		if (!strcmp(piece, "fiasco")) {
			fiasco_flash(filename);
			return;
		}
	}*/

/*	if (version)
		vlen = strlen(version)+1;

//	fd = fopen(filename, "rb");
//	if (fd == NULL) {
//		printf("Cannot open file\n");
//		exit(1);
//	}*/
	/* cook flash query */
/*	memset(fquery, '\x00', 27);              // reset buffer
	memcpy(fquery, "\x2e\x19\x01\x01", 4);   // header
	//memcpy(fquery+5, "\xbf\x6b", 2); // some magic (modified crc16???)
	memcpy(fquery+5, &hash, 2);
	tmp = fquery[5]; fquery[5] = fquery[6]; fquery[6] = tmp;
	memcpy(fquery+7, piece, strlen(piece));  // XXX ??!??

	printf("| hash: 0x%hhx%hhx ", fquery[5], fquery[6]);
//	size = get_file_size(filename);
	size = image->size;
	bsize[0] = (size & 0xff000000) >> 24;
	bsize[1] = (size & 0x00ff0000) >> 16;
	bsize[2] = (size & 0x0000ff00) >> 8;
	bsize[3] = (size & 0x000000ff);
	printf("size: %lld (%02x %02x %02x %02x)\n",
		size, bsize[0], bsize[1], bsize[2], bsize[3]);
	memcpy(fquery+0x13, &bsize, 4);
	if (vlen) memcpy(fquery+27, version, vlen);
	
	if (!strcmp(piece, "rootfs"))
		request = 85;
	else    request = 68;

	//dump_bytes(fquery, 27+vlen);
	if (usb_control_msg(dev, CMD_WRITE, request, 0, 0, (char *)fquery, 27+vlen, 2000) <0) {
		query_error_message();
		perror("flash_image.header");
		exit(1);
	}*/

	/*/ cook and bulk nollo filler */
/*	memset(&nolofiller, '\xff', 128);
	memcpy(nolofiller+0x00, "NOLO filler", 11);
	memcpy(nolofiller+0x40, "NOLO filler", 11);
	usb_bulk_write(dev, 2, (char *)nolofiller, 128, 5000);
	usb_bulk_write(dev, 2, (char *)nolofiller, 0,   5000);*/

	/*/ bulk write image here */
/*	printf("[=] Bulkwriting the %s piece...\n", piece);
	fflush(stdout);

	#define BSIZE 0x20000

	for(off = 0; off<size; off += BSIZE) {
		char buf[BSIZE];
		int bread, bsize = size-off;
		if (bsize>BSIZE) bsize = BSIZE;
//		bread = fread(buf, bsize, 1, fd);
		bread = image_read(image, buf, bsize);
		if (bread != 1)
			printf("WARNING: Oops wrong read %d vs %d \n", bread, bsize);
		bread = usb_bulk_write(dev, 2, buf, bsize, 5000);
		if (bread == 64) {
			query_error_message();
//			fclose(fd);
			return;
		}
		printf_progressbar(off, size);
		if (bread<0) {
			printf("\n");
			perror(" -ee- ");
//			fclose(fd);
			return;
		}
		fflush(stdout);
	}
//	fclose(fd);*/
	/*/ EOF */
/*	usb_bulk_write(dev, 2, (char *)nolofiller, 0, 1000);
	printf_progressbar(1, 1);
	printf("\n");

	// index = 4????
	if (!strcmp(piece, "rootfs")) {
		if (usb_control_msg(dev, CMD_WRITE, 82, 0, 0, (char *)fquery, 0, 30000)<0) {
			fprintf(stderr, "Oops. Invalid checksum?\n");
			exit(1);
		}
	} else {
		int t = 0;
		if (!strcmp(piece, "secondary"))
			t = 1;
		else
		if (!strcmp(piece, "kernel"))
			t = 3;
		else
		if (!strcmp(piece, "initfs"))
			t = 4;
		if (!strcmp(piece, "xloader"))
			printf("xloader flashed not commiting until secondary arrives...\n");
		else
		if (usb_control_msg(dev, CMD_WRITE, 80, 0, t, (char *)fquery, 0, 10000)<0) {
			fprintf(stderr, "Oops. Invalid checksum?\n");
			exit(1);
		}
	}

	// unknown query !! :""
	if (usb_control_msg(dev, CMD_WRITE, 67, 0, 0, (char *)fquery, 0, 2000)<0) {
		fprintf(stderr, "Oops, the flash was denied or so :/\n");
		exit(1);
	}
	printf("Flash done succesfully.\n");
}*/

//char strbuf[1024];

/**
 * Set flags:
 * request type: CMD_WRITE
 * request     : 16
 * value       : flags to set, see below
 * index       : 3
 * 
 * Clear flags:
 * request type: CMD_WRITE
 * request     : 16
 * value       : flags to clear, see below
 * index       : 4
 
 * Flags are composed of:
 * 0x02 - disable OMAP watchdog (possibly)
 * 0x04 - disable RETU watchdog (possibly)
 * 0x08 - disable lifeguard reset
 * 0x10 - enable serial console
 * 0x20 - disable USB timeout
 * 
 * The function encapsule the NOLO API in a way that it works like
 * a chmod 707. The bits that are not set will be cleared after the
 * call. This is done by issuing a 'clear flags' command with all the
 * non set bits.  
 * 
 */
/*
int set_rd_flags(unsigned short flags)
{
	unsigned short reverse_flags = 0;
	
	if (flags & ~(0x02 | 0x04 | 0x08 | 0x10 | 0x20)) {
		printf("Invalid rd flags specified '%x'.\n", flags);
		return -1;
	}
	
	if (!(flags & 0x02))
	  reverse_flags |= 0x02;

	if (!(flags & 0x04))
	  reverse_flags |= 0x04;

	if (!(flags & 0x08))
	  reverse_flags |= 0x08;

	if (!(flags & 0x10))
	  reverse_flags |= 0x10;

	if (!(flags & 0x20))
	  reverse_flags |= 0x20;
	  
	if (usb_control_msg(dev, CMD_WRITE, 16, flags, 3, 0, 0, 2000) == -1) {
		fprintf(stderr, "Cannot set rd flags\n");
		return -1;
	}

	if (usb_control_msg(dev, CMD_WRITE, 16, reverse_flags, 4, 0, 0, 2000) == -1) {
		fprintf(stderr, "Cannot set rd flags\n");
		return -1;
	}

	printf("Set rd flags successfully!\n");
	printf("disable OMAP watchdog  : %s\n", (flags & 0x02) ? "set" : "not set"); 
	printf("disable RETU watchdog  : %s\n", (flags & 0x04) ? "set" : "not set"); 
	printf("disable lifeguard reset: %s\n", (flags & 0x08) ? "set" : "not set"); 
	printf("enable serial console  : %s\n", (flags & 0x10) ? "set" : "not set"); 
	printf("disable USB timeout    : %s\n", (flags & 0x20) ? "set" : "not set"); 

	return 0;
}

int get_rd_flags()
{
	unsigned short flags = 0;
	
	if (usb_control_msg (dev, CMD_QUERY, NOLO_GET_RDFLAGS, 0, 3, (void *) &flags,sizeof(flags), 2000) == -1) {
		fprintf(stderr, "Cannot get rd flags\n");
		sprintf(strbuf, "error: Cannot read rd flags\n");
		return -1;
	}
	
	sprintf (strbuf,
	"Current rd flag setting:\n"
	"disable OMAP watchdog  : %s\n"
	"disable RETU watchdog  : %s\n"
	"disable lifeguard reset: %s\n"
	"enable serial console  : %s\n"
	"disable USB timeout    : %s\n"
		, (flags & 0x02) ? "set" : "not set"
		, (flags & 0x04) ? "set" : "not set"
		, (flags & 0x08) ? "set" : "not set"
		, (flags & 0x10) ? "set" : "not set"
		, (flags & 0x20) ? "set" : "not set");
	puts (strbuf);
	
	return 0; 
}
*/
