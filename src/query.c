/*
 *  0xFFFF - Open Free Fiasco Firmware Flasher
 *  Copyright (C) 2007-2009  pancake <@youterm.com>
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

#if HAVE_USB

#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <usb.h>

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

	printf("Root device is: %s\n", root_devices[opcode]);

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

	printf("Root device set to: %s\n", root_devices[root_device]);

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

#endif
