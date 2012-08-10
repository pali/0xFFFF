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
#include "printf-utils.h"

#define CMD_WRITE 64
#define CMD_QUERY 192

struct usb_dev_handle *dev;

/*void check_nolo_order_failed()
{
	fprintf(stderr, "\nERROR: Provide xloader before the secondary. NOLO disagrees anything else.\n");
	fprintf(stderr, "Use: -p xloader.bin -p secondary.bin\n");
	exit(1);
}*/

static void query_error_message()
{
	/* query error message */
	int len = 0;
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
}

/*void check_nolo_order()
{
	int i, xlo = 0;

	for(i=0;i<pcs_n;i++) {
		if (!strcmp(pcs[i].type, "xloader")) {
			if (!xlo && ++i<pcs_n) {
			printf("CHK%s\n", pcs[i].type);
				xlo++;
				if (!strcmp(pcs[i].type, "secondary")) {
					xlo = 0x11;
					break;
				} else check_nolo_order_failed();
			} check_nolo_order_failed();
		} else
		if (!strcmp(pcs[i].type, "secondary"))
			check_nolo_order_failed();
	}

	if (xlo && xlo != 0x11)
		check_nolo_order_failed();
}*/

void flash_image(struct image * image)
{
//	FILE *fd;
	int vlen = 0;
	int request;
	/*/ n800 flash queries have a variable size */
	unsigned char fquery[256]; /* flash query */
	unsigned long long size, off;
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
//	}

/*	if (piece != NULL) {
		if (!strcmp(piece, "fiasco")) {
			fiasco_flash(filename);
			return;
		}
	}*/

	if (version)
		vlen = strlen(version)+1;

//	fd = fopen(filename, "rb");
//	if (fd == NULL) {
//		printf("Cannot open file\n");
//		exit(1);
//	}
	/* cook flash query */
	memset(fquery, '\x00', 27);              // reset buffer
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
	}

	/*/ cook and bulk nollo filler */
	memset(&nolofiller, '\xff', 128);
	memcpy(nolofiller+0x00, "NOLO filler", 11);
	memcpy(nolofiller+0x40, "NOLO filler", 11);
	usb_bulk_write(dev, 2, (char *)nolofiller, 128, 5000);
	usb_bulk_write(dev, 2, (char *)nolofiller, 0,   5000);

	/*/ bulk write image here */
	printf("[=] Bulkwriting the %s piece...\n", piece);
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
//	fclose(fd);
	/*/ EOF */
	usb_bulk_write(dev, 2, (char *)nolofiller, 0, 1000);
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
}

char strbuf[1024];

/* wait for status = 0 */
int get_status()
{
	int ret = 0;
	if (usb_control_msg(dev, CMD_QUERY, 1, 0, 0, (char *)&ret, 4, 2000) == -1) {
		fprintf(stderr, "Cannot get device status.\n");
		exit(1);
	}

	return ret;
}

/**
 * request type: CMD_WRITE
 * request     : 16
 * value       : 0|1 (=> client|host)
 * index       : 2
 */
int set_usb_mode(unsigned int mode)
{
	if (mode > 1) {
		printf("Invalid USB mode specified '%d'.\n", mode);
		return -1;
	}
	
	if (usb_control_msg(dev, CMD_WRITE, 16, mode, 2, 0, 0, 2000) == -1) {
		fprintf(stderr, "Cannot set USB mode.\n");
		return -1;
	}

	printf("Set USB mode to: '%s'.\n", (mode) ? "host" : "client");

	return 0;
}

/**
 * request type: CMD_QUERY
 * request     : 17
 * index       : 2
 * 
 * return value: 0|1 (=> client|host)
 */
int get_usb_mode()
{
	unsigned short mode = 0;

	if (usb_control_msg(dev, CMD_QUERY, 17, 0, 2, (void *) &mode, sizeof(mode), 2000) == -1) {
		fprintf(stderr, "Cannot query device.\n");
		return -1;
	}

	sprintf(strbuf, "Device's USB mode is '%s'\n", (mode) ? "host" : "client");
	printf("%s", strbuf);

	return 0;
}

/*
 * Boots the Kernel with the given command-line.
 */
int boot_board(char *cmdline)
{
	if (usb_control_msg(dev, CMD_WRITE, 130, 0, 0, cmdline, strlen(cmdline), 2000) == -1) {
		fprintf(stderr, "Cannot boot kernel.\n");
		perror("boot_board");
		return -1;
	}

	printf("Booting kernel with arguments: '%s'.\n", cmdline);

	return 0;
}

/*
 * Reboots the omap mobo.
 */
int reboot_board()
{
	// 131 = reboot
	if (usb_control_msg(dev, CMD_WRITE, 131, 0, 0, 0, 0, 2000) == -1) {
		fprintf(stderr, "Cannot reboot board.\n");
		return -1;
	}

	printf("Mobo rebooted!\n");

	return 0;
}

int set_rd_mode(unsigned short mode)
{
	if (((short)mode)==-1)
		return 1;

	if (usb_control_msg(dev, CMD_WRITE, NOLO_SET_RDFLAGS, mode, 0, 0, 0, 2000) == -1) {
		fprintf(stderr, "Cannot set R&D flags.\n");
		return 1;
	}

	printf("rd mode changed to %s\n", mode?"on":"off");

	return 0;
}

/*
 * query root device
 */
int get_hw_revision(char *str, int len)
{
	unsigned char string[512];
	char tmpstr[64];
	int i = 0, j=0, mod=0;

	if (str == NULL)
		str = tmpstr;
	memset(string,'\0',512);
	if (usb_control_msg(dev, CMD_QUERY, NOLO_GET_HWVERSION, 0, 0, (char *)string, 512, 2000) == -1) {
		fprintf(stderr, "Cannot query hw revision.\n");
		return -1;
	}

	for(i=0;i<44&&i<len;i++) { // XXX ??
		if (string[i]>19) {
			if (i>0 && string[i-1]<19)
				str[j++] = (++mod%2)?':':',';
			str[j++] = string[i];
		}
	}
	printf("HW revision string: '%s'\n", str);

	return 0;
}

int get_rd_mode()
{
	char isrd = 1;

	strbuf[0]='\0';
	if (usb_control_msg(dev, CMD_QUERY, NOLO_GET_RDFLAGS, 0, 0, (char *)&isrd, 1, 2000) == -1) {
		fprintf(stderr, "Cannot query rd mode.\n");
		return -1;
	}
	sprintf(strbuf, "RD mode is: %s\n", isrd?"on":"off");

	return isrd;
}

int get_root_device()
{
	unsigned char opcode;

	if (usb_control_msg(dev, CMD_QUERY, NOLO_GET_RDFLAGS, 0, 1, (char *)&opcode, 1, 2000) < 0) {
		fprintf(stderr, "Cannot query root device\n");
		return -1;
	}

	if (opcode>2) { // use sizeof || enum
		fprintf(stderr, "Invalid root device received from the device '%d'.\n", opcode);
		return -1;
	}

//	printf("Root device is: %s\n", root_devices[opcode]);

	return 0;
}

/**
 * request type: CMD_WRITE
 * request     : 16
 * value       : 0|1|2 (=> flash|mmc|usb)
 * index       : 1
 */
int set_root_device(unsigned short root_device)
{
	if (root_device>2) {
		printf("Invalid root device specified '%d'.\n", root_device);
		return -1;
	}

	if (usb_control_msg(dev, CMD_WRITE, NOLO_SET_RDFLAGS, root_device, 1, 0, 0, 2000) == -1) {
		fprintf(stderr, "Cannot set root device\n");
		return -1;
	}

//	printf("Root device set to: %s\n", root_devices[root_device]);

	return 0;
}

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

int get_nolo_version()
{
	unsigned int version; // ensure uint is at least 32 bits

	//if (usb_control_msg(dev, CMD_QUERY, 3, 0, 1, (char *)&version, 4 , 2000) < 0) {
	if (usb_control_msg(dev, CMD_QUERY, NOLO_GET_VERSION, 0, 0, (char *)&version, 4 , 2000) < 0) {
		fprintf(stderr, "Cannot query nolo version. Old bootloader version?\n");
		sprintf(strbuf, "error: Cannot query nolo version. Old bootloader?");
		return -1;
	}

	sprintf(strbuf,
		"NOLO Version %d.%d.%d\n",
		version >> 20 & 15,
		version >> 16 & 15,
		version >> 8 & 255);

	if ((version & 255)> 1)
		printf("Invalid API version (%d)\n", version&255);

	return 0;
}

int get_sw_version()
{
	int ret;
	char bytes[1024];

	strcpy(bytes, "version:sw-release");
	ret = usb_control_msg(dev, CMD_WRITE, 18, 0, 0, (char *)&bytes, 18, 2000);
	if (ret<0) {
		fprintf(stderr, "error: cannot write query 18\n");
		return 0;
	}
	bytes[0]='\0';
	ret = usb_control_msg(dev, CMD_QUERY, 20, 0, 0, (char *)&bytes, 512, 2000);
	if (ret<0) {
		fprintf(stderr, "error: b0rken swversion read!\n");
		return 0;
	}
	if (bytes[0]) {
		sprintf(strbuf, "Software Version: %s\n", bytes);
		printf("Software Version: %s\n", bytes); //???+strlen(bytes)+1));
	} else printf("No software version detected\n");

	return 1;
}
