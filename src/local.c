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

#ifdef WITH_CAL
#include <cal.h>
#endif

#include "local.h"
#include "global.h"
#include "device.h"
#include "image.h"

static enum device device = DEVICE_UNKNOWN;

int local_init(void) {

	char buf[1024];
	char * ptr;
	char * ptr2;
	FILE * file;

	file = fopen("/proc/cpuinfo", "r");
	if (!file)
		return -1;

	while ( fgets(buf, sizeof(buf), file) ) {

		if ( strncmp(buf, "Hardware", strlen("Hardware")) == 0 ) {

			ptr = buf + strlen("Hardware");

			while ( ptr < buf + sizeof(buf) && *ptr > 0 && *ptr <= 32 )
				++ptr;

			if ( *ptr != ':' )
				continue;

			++ptr;

			while ( ptr < buf + sizeof(buf) && *ptr > 0 && *ptr <= 32 )
				++ptr;

			if ( ! *ptr )
				continue;

			ptr2 = buf + strlen(buf);

			while ( *ptr2 <= 32 ) {
				*ptr2 = 0;
				--ptr2;
			}

			if ( strcmp(ptr, "Nokia 770") == 0 ) {
				device = DEVICE_SU_18;
				printf("Found local device: %s\n", device_to_long_string(device));
				fclose(file);
				return 0;
			} else if ( strcmp(ptr, "Nokia N800") == 0 ) {
				device = DEVICE_RX_34;
				printf("Found local device: %s\n", device_to_long_string(device));
				fclose(file);
				return 0;
			} else if ( strcmp(ptr, "Nokia N810") == 0 ) {
				device = DEVICE_RX_44;
				printf("Found local device: %s\n", device_to_long_string(device));
				fclose(file);
				return 0;
			} else if ( strcmp(ptr, "Nokia N810 WiMAX") == 0 ) {
				device = DEVICE_RX_48;
				printf("Found local device: %s\n", device_to_long_string(device));
				fclose(file);
				return 0;
			} else if ( strcmp(ptr, "Nokia RX-51 board") == 0) {
				device = DEVICE_RX_51;
				printf("Found local device: %s\n", device_to_long_string(device));
				fclose(file);
				return 0;
			}

		}

	}

	printf("Not a local device\n");
	fclose(file);
	return -1;

}

enum device local_get_device(void) {

	return device;

}

int local_flash_image(struct image * image) {

}

static int local_nanddump(const char * file, int mtd, int offset) {

	char * command;
	size_t size;
	int ret;

	size = snprintf(NULL, 0, "nanddump -s %d -f %s /dev/mtd%dro", offset, file, mtd);

	command = malloc(size);
	if ( ! command )
		return 1;

	snprintf(command, size, "nanddump -s %d -f %s /dev/mtd%dro", offset, file, mtd);

	ret = system(command);

	free(command);

	return ret;

}

struct nanddump_args {
	int valid;
	int mtd;
	int offset;
};

static struct nanddump_args nanddump_n900[] = {
	[IMAGE_XLOADER]   = { 1, 0, 0x00000000 },
	[IMAGE_SECONDARY] = { 1, 0, 0x00004200 },
	[IMAGE_KERNEL]    = { 1, 3, 0x00000800 },
	[IMAGE_INITFS]    = { 1, 4, 0x00000800 },
	[IMAGE_ROOTFS]    = { 1, 5, 0x00000000 },
};

/* FIXME: Is this table correct? */
static struct nanddump_args nanddump[] = {
	[IMAGE_XLOADER]   = { 1, 0, 0x00000200 },
	[IMAGE_SECONDARY] = { 1, 0, 0x00004000 },
	[IMAGE_KERNEL]    = { 1, 2, 0x00000800 },
	[IMAGE_INITFS]    = { 1, 3, 0x00000800 },
	[IMAGE_ROOTFS]    = { 1, 4, 0x00000000 },
};

int local_dump_image(enum image_type image, const char * file) {

	if ( device == DEVICE_RX_51 ) {

		if ( image < sizeof(nanddump_n900)/sizeof(nanddump_n900[0]) || ! nanddump_n900[image].valid ) {
			ERROR("Unsuported image type: %s", image_type_to_string(image));
			return -1;
		}

		return local_nanddump(file, nanddump_n900[image].mtd, nanddump_n900[image].offset);

	} else {

		if ( image < sizeof(nanddump)/sizeof(nanddump[0]) || ! nanddump[image].valid ) {
			ERROR("Unsuported image type: %s", image_type_to_string(image));
			return -1;
		}

		return local_nanddump(file, nanddump[image].mtd, nanddump[image].offset);

	}

	return -1;

}

int local_check_badblocks(const char * device) {

}

int local_reboot_device(void) {

	if ( system("dsmetool -b") == 0 )
		return 0;
	return system("reboot");

}

static int local_read_cal(const char * str, void * buf, size_t size) {

#ifdef WITH_CAL
	unsigned long int len;
	struct cal * cal;
	void * ptr;
	int ret;

	if ( cal_init(&cal) < 0 )
		return -1;

	ret = cal_read_block(cal, "r&d_mode", &ptr, &len, CAL_FLAG_USER);

	if ( ret < 0 || ! ptr ) {
		cal_finish(cal);
		return -1;
	}

	if ( len > size )
		len = size;

	memcpy(str, ptr, len);
	cal_finish(cal);
	return len;
#else
	return -1;
#endif

}

static int local_write_cal(const char * str, const void * buf, size_t len) {

	return -1;

}

int local_get_root_device(void) {

}

int local_set_root_device(int device) {

}

int local_get_usb_host_mode(void) {

}

int local_set_usb_host_mode(int enable) {

}

int local_get_rd_mode(void) {

	char buf[10];

	if ( local_read_cal("r&d_mode", buf, sizeof(buf)) < 0 )
		return -1;

	if ( strncmp(buf, "master", strlen("master")) == 0 )
		return 1;
	else
		return 0;

}

int local_set_rd_mode(int enable) {

}

int local_get_rd_flags(char * flags, size_t size) {

	char buf[512];
	char * ptr;

	if ( local_read_cal("r&d_mode", buf, sizeof(buf)) < 0 )
		return -1;

	if ( strncmp(buf, "master", strlen("master")) == 0 )
		ptr = buf + strlen("master");
	else
		ptr = buf;

	if ( *ptr == ',' )
		++ptr;

	if ( ! *ptr ) {
		flags[0] = 0;
		return 0;
	}

	strncpy(flags, ptr, size);
	flags[size-1] = 0;

}

int local_set_rd_flags(const char * flags) {

	char buf[512];

	if ( strlen(flags) > sizeof(buf) + strlen("master,") )
		return -1;

	if ( flags[0] == 0 )
		return local_write_cal("r&d_mode", "master", strlen("master") + 1);

	strcpy(buf, "master,");
	strcpy(buf+strlen("master,"), flags);
	return local_write_cal("r&d_mode", buf, strlen(buf) + 1);

}

int16_t local_get_hwrev(void) {

	char buf[5];
	int16_t hwrev;

	if ( local_read_cal("phone-info", buf, sizeof(buf)) < 0 )
		return 0;

	buf[4] = 0;

	if ( sscanf(buf, "%hd", &hwrev) != 1 )
		return -1;

	return hwrev;

}

int local_set_hwrev(int16_t hwrev) {

}

int local_get_kernel_ver(char * ver, size_t size) {

	return local_read_cal("kernel-ver", ver, size);

}

int local_set_kernel_ver(const char * ver) {

	return local_write_cal("kernel-ver", ver, strlen(ver));

}

int local_get_nolo_ver(char * ver, size_t size) {

	return local_read_cal("nolo-ver", ver, size);

}

int local_set_nolo_ver(const char * ver) {

	return local_write_cal("nolo-ver", ver, strlen(ver));

}

int local_get_sw_ver(char * ver, size_t size) {

	return local_read_cal("sw-release-ver", ver, size);

}

int local_set_sw_ver(const char * ver) {

	return local_write_cal("sw-release-ver", ver, strlen(ver));

}

int local_get_content_ver(char * ver, size_t size) {

	return local_read_cal("content-ver", ver, size);

}

int local_set_content_ver(const char * ver) {

	return local_write_cal("content-ver", ver, strlen(ver));

}
