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

#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <libgen.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

#include "local.h"
#include "global.h"
#include "device.h"
#include "image.h"
#include "cal.h"
#include "disk.h"

static int failed;

static enum device device = DEVICE_UNKNOWN;
static int16_t hwrev = -1;
static char kernel_ver[256];
static char initfs_ver[256];
static char nolo_ver[256];
static char sw_ver[256];
static char content_ver[256];
static char rd_mode[256];
static int usb_host_mode = -1;
static int root_device = -1;

#define min(a, b) (a < b ? a : b)
#define local_cal_copy(dest, from, len) strncpy(dest, from, min(len, sizeof(dest)-1))
#define local_cal_read(cal, str, ptr, len) ( cal_read_block(cal, str, &ptr, &len, 0) == 0 && ptr )
#define local_cal_readcopy(cal, str, dest) do { void * ptr; unsigned long int len; if ( local_cal_read(cal, str, ptr, len) ) local_cal_copy(dest, ptr, len); } while ( 0 )

#if defined(__linux__) && defined(__arm__)

static void local_cal_parse(void) {

	struct cal * cal = NULL;
	char buf[128];

	printf("Reading CAL...\n");

	if ( cal_init(&cal) < 0 || ! cal )
		return;

	local_cal_readcopy(cal, "kernel-ver", kernel_ver);
	local_cal_readcopy(cal, "initfs-ver", kernel_ver);
	local_cal_readcopy(cal, "nolo-ver", nolo_ver);
	local_cal_readcopy(cal, "sw-release-ver", sw_ver);
	local_cal_readcopy(cal, "content-ver", content_ver);
	local_cal_readcopy(cal, "r&d_mode", rd_mode);

	/* overwritten hw revision */
	memset(buf, 0, sizeof(buf));
	local_cal_readcopy(cal, "hw-ver", buf);

	if ( buf[0] && sscanf(buf, "%hd", &hwrev) != 1 )
		hwrev = -1;

	if ( hwrev == -1 ) {

		/* original hw revision */
		memset(buf, 0, sizeof(buf));
		local_cal_readcopy(cal, "phone-info", buf);
		buf[4] = 0;

		if ( buf[0] && sscanf(buf, "%hd", &hwrev) != 1 )
			hwrev = -1;

	}

	buf[0] = 0;
	local_cal_readcopy(cal, "usb_host_mode", buf);
	usb_host_mode = buf[0];

	memset(buf, 0, sizeof(buf));
	local_cal_readcopy(cal, "root_device", buf);

	if ( strcmp(buf, "mmc") == 0 )
		root_device = 1;
	else if ( strcmp(buf, "usb") == 0 )
		root_device = 2;
	else
		root_device = 0;

	cal_finish(cal);

}

#endif

int local_init(void) {

#if defined(__linux__) && defined(__arm__)
	char buf[1024];
	char * ptr;
	char * ptr2;
	FILE * file;
#endif

	if ( failed )
		return -1;

#if defined(__linux__) && defined(__arm__)

	file = fopen("/proc/cpuinfo", "r");
	if ( ! file ) {
		failed = 1;
		return -1;
	}

	while ( fgets(buf, sizeof(buf), file) ) {

		if ( strncmp(buf, "Hardware", sizeof("Hardware")-1) == 0 ) {

			ptr = buf + sizeof("Hardware")-1;

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

			if ( strcmp(ptr, "Nokia 770") == 0 )
				device = DEVICE_SU_18;
			else if ( strcmp(ptr, "Nokia N800") == 0 )
				device = DEVICE_RX_34;
			else if ( strcmp(ptr, "Nokia N810") == 0 )
				device = DEVICE_RX_44;
			else if ( strcmp(ptr, "Nokia N810 WiMAX") == 0 )
				device = DEVICE_RX_48;
			else if ( strcmp(ptr, "Nokia RX-51 board") == 0 )
				device = DEVICE_RX_51;

			if ( device ) {
				printf("Found local device: %s\n", device_to_long_string(device));
				fclose(file);
				local_cal_parse();
				return 0;
			}

		}

	}

	fclose(file);

#endif

	failed = 1;
	printf("Not a local device\n");
	return -1;

}

enum device local_get_device(void) {

	return device;

}

int local_flash_image(struct image * image) {

	ERROR("Not implemented yet");
	(void)image;
	return -1;

}

static int local_nanddump(const char * file, int mtd, int offset, int length) {

	struct statvfs buf;
	char * command;
	char * path;
	size_t size;
	int ret;

	path = strdup(file);
	if ( ! path )
		return 1;

	ret = statvfs(dirname(path), &buf);

	free(path);

	if ( ret == 0 && buf.f_bsize * buf.f_bfree < (long unsigned int)length ) {
		ERROR("Not enough free space (have: %lu, need: %d)", buf.f_bsize * buf.f_bfree, length);
		return 1;
	}

	size = snprintf(NULL, 0, "nanddump -o -b -s %d -l %d -f %s /dev/mtd%dro", offset, length, file, mtd);

	command = malloc(size+1);
	if ( ! command )
		return 1;

	snprintf(command, size+1, "nanddump -o -b -s %d -l %d -f %s /dev/mtd%dro", offset, length, file, mtd);

	ret = system(command);

	free(command);

	return ret;

}

struct nanddump_args {
	int valid;
	int mtd;
	int offset;
	int length;
};

static struct nanddump_args nanddump_rx51[] = {
	[IMAGE_XLOADER]   = { 1, 0, 0x00000000, 0x00004000 },
	[IMAGE_SECONDARY] = { 1, 0, 0x00004000, 0x0001C000 },
	[IMAGE_KERNEL]    = { 1, 3, 0x00000800, 0x001FF800 },
	[IMAGE_INITFS]    = { 1, 4, 0x00000000, 0x00200000 },
	[IMAGE_ROOTFS]    = { 1, 5, 0x00000000, 0x0fb40000 },
};

/* FIXME: Is this table correct? */
static struct nanddump_args nanddump_rx4x[] = {
	[IMAGE_XLOADER]   = { 1, 0, 0x00000200, 0x00003E00 },
	[IMAGE_SECONDARY] = { 1, 0, 0x00004000, 0x0001C000 },
	[IMAGE_KERNEL]    = { 1, 2, 0x00000800, 0x0021F800 },
	[IMAGE_INITFS]    = { 1, 3, 0x00000000, 0x00400000 },
	[IMAGE_ROOTFS]    = { 1, 4, 0x00000000, 0x0f960000 },
};

/* FIXME: Is this table correct? */
static struct nanddump_args nanddump_old[] = {
	[IMAGE_XLOADER]   = { 1, 0, 0x00000200, 0x00003E00 },
	[IMAGE_SECONDARY] = { 1, 0, 0x00004000, 0x0001C000 },
	[IMAGE_KERNEL]    = { 1, 2, 0x00000800, 0x001FF800 },
	[IMAGE_INITFS]    = { 1, 3, 0x00000000, 0x00200000 },
	[IMAGE_ROOTFS]    = { 1, 4, 0x00000000, 0x0fb80000 },
};

struct nanddump_device {
	size_t count;
	struct nanddump_args * args;
};

#define NANDDUMP(device, array) [device] = { .count = sizeof(array)/sizeof(array[0]), .args = array }

static struct nanddump_device nanddump[] = {
	NANDDUMP(DEVICE_SU_18, nanddump_old),
	NANDDUMP(DEVICE_RX_34, nanddump_old),
	NANDDUMP(DEVICE_RX_44, nanddump_rx4x),
	NANDDUMP(DEVICE_RX_48, nanddump_rx4x),
	NANDDUMP(DEVICE_RX_51, nanddump_rx51),
};

#undef NANDDUMP

static void local_find_internal_mydocs(int * maj, int * min) {

#ifdef __linux__

	int fd;
	DIR * dir;
	DIR * dir2;
	FILE * f;
	struct dirent * dirent;
	struct dirent * dirent2;
	char buf[1024];

	/* Find min & maj id for block device MyDocs (mmc device, partition 1) */

	dir = opendir("/sys/class/mmc_host/");
	if ( ! dir ) {
		ERROR("Cannot find MyDocs mmc device: Opening '/sys/class/mmc_host/' failed");
		return;
	}

	while ( ( dirent = readdir(dir) ) ) {

		if ( strncmp(dirent->d_name, ".", sizeof(".")) == 0 || strncmp(dirent->d_name, "..", sizeof("..")) == 0 )
			continue;

		if ( snprintf(buf, sizeof(buf), "/sys/class/mmc_host/%s/slot_name", dirent->d_name) <= 0 )
			continue;

		fd = open(buf, O_RDONLY);
		if ( fd < 0 )
			continue;

		memset(buf, 0, sizeof(buf));
		if ( read(fd, buf, sizeof(buf)-1) < 0 )
			buf[0] = 0;
		close(fd);
		fd = -1;

		if ( strncmp(buf, "internal", sizeof("internal")-1) != 0 )
			continue;

		if ( snprintf(buf, sizeof(buf), "/sys/class/mmc_host/%s/%s:0001/", dirent->d_name, dirent->d_name) <= 0 )
			continue;

		dir2 = opendir(buf);
		if ( ! dir2 )
			continue;

		while ( ( dirent2 = readdir(dir2) ) ) {

			if ( strncmp(dirent2->d_name, "block:mmcblk", sizeof("block:mmcblk")-1) != 0 )
				continue;

			if ( snprintf(buf, sizeof(buf), "/sys/class/mmc_host/%s/%s:0001/%s/dev", dirent->d_name, dirent->d_name, dirent2->d_name) <= 0 )
				continue;

			f = fopen(buf, "r");
			if ( ! f )
				continue;

			if ( fscanf(f, "%d:%d", maj, min) != 2 ) {
				*maj = -1;
				*min = -1;
				fclose(f);
				continue;
			}

			fclose(f);
			break;

		}

		closedir(dir2);

		if ( *maj != -1 && *min != -1 )
			break;

	}

	closedir(dir);

#endif

}

int local_dump_image(enum image_type image, const char * file) {

	int ret = -1;
	int fd = -1;
	unsigned char * addr = NULL;
	off_t nlen, len;
	int align;
	int maj, min;

	printf("Dump %s image to file %s...\n", image_type_to_string(image), file);

	if ( image == IMAGE_MMC ) {

		maj = -1;
		min = -1;

		local_find_internal_mydocs(&maj, &min);
		if ( maj == -1 || min == -1 ) {
			ERROR("Cannot find MyDocs mmc device: Slot 'internal' was not found");
			goto clean;
		}

		VERBOSE("Detected internal MyDocs mmc device: major=%d minor=%d\n", maj, min);

		fd = disk_open_dev(maj, min, 1, 1);
		if ( fd < 0 ) {
			ERROR("Cannot open MyDocs mmc device in /dev/");
			goto clean;
		}

		ret = disk_dump_dev(fd, file);

		close(fd);
		fd = -1;

	} else {

		if ( device >= sizeof(nanddump)/sizeof(nanddump[0]) ) {
			ERROR("Unsupported device");
			goto clean;
		}

		if ( image >= nanddump[device].count ) {
			ERROR("Unsupported image type: %s", image_type_to_string(image));
			goto clean;
		}

		ret = local_nanddump(file, nanddump[device].args[image].mtd, nanddump[device].args[image].offset, nanddump[device].args[image].length);

	}

	if ( ret != 0 ) {
		ret = -1;
		goto clean;
	}

	fd = open(file, O_RDWR);
	if ( fd < 0 )
		goto clean;

	len = lseek(fd, 0, SEEK_END);
	if ( len == (off_t)-1 || len == 0 )
		goto clean;

	addr = (unsigned char *)mmap(NULL, len, PROT_READ, MAP_PRIVATE, fd, 0);

	if ( addr == MAP_FAILED )
		addr = NULL;

	if ( ! addr )
		goto clean;

	for ( nlen = len; nlen > 0; --nlen )
		if ( addr[nlen-1] != 0xFF )
			break;

	for ( ; nlen > 0; --nlen )
		if ( addr[nlen-1] != 0x00 )
			break;

	if ( image == IMAGE_MMC )
		align = 8;
	else
		align = 7;

	if ( ( nlen & ( ( 1ULL << align ) - 1 ) ) != 0 )
		nlen = ((nlen >> align) + 1) << align;

	if ( nlen == 0 ) {
		printf("File %s is empty, removing it...\n", file);
		unlink(file);
	} else if ( nlen != len ) {
		printf("Truncating file %s to %d bytes...\n", file, (int)nlen);
		if ( ftruncate(fd, nlen) < 0 )
			ERROR_INFO("Cannot truncate file %s", file);
	}

clean:
	if ( addr )
		munmap((void *)addr, len);

	if ( fd >= 0 )
		close(fd);

	printf("\n");
	return ret;

}

int local_check_badblocks(const char * device) {

	ERROR("Not implemented yet");
	(void)device;
	return -1;

}

int local_reboot_device(void) {

	if ( system("dsmetool -b") == 0 )
		return 0;
	return system("reboot");

}

int local_get_root_device(void) {

	return root_device;

}

int local_set_root_device(int device) {

	ERROR("Not implemented yet");
	(void)device;
	return -1;

}

int local_get_usb_host_mode(void) {

	return usb_host_mode;

}

int local_set_usb_host_mode(int enable) {

	ERROR("Not implemented yet");
	(void)enable;
	return -1;

}

int local_get_rd_mode(void) {

	if ( strncmp(rd_mode, "master", sizeof("master")-1) == 0 )
		return 1;
	else
		return 0;

}

int local_set_rd_mode(int enable) {

	ERROR("Not implemented yet");
	(void)enable;
	return -1;

}

int local_get_rd_flags(char * flags, size_t size) {

	const char * ptr;

	if ( strncmp(rd_mode, "master", sizeof("master")-1) == 0 )
		ptr = rd_mode + sizeof("master")-1;
	else
		ptr = rd_mode;

	if ( *ptr == ',' )
		++ptr;

	if ( ! *ptr ) {
		flags[0] = 0;
		return 0;
	}

	strncpy(flags, ptr, size);
	flags[size-1] = 0;
	return 0;

}

int local_set_rd_flags(const char * flags) {

	ERROR("Not implemented yet");
	(void)flags;
	return -1;

}

int16_t local_get_hwrev(void) {

	return hwrev;

}

int local_set_hwrev(int16_t hwrev) {

	ERROR("Not implemented yet");
	(void)hwrev;
	return -1;

}

int local_get_kernel_ver(char * ver, size_t size) {

	strncpy(ver, kernel_ver, size);
	ver[size-1] = 0;
	return 0;

}

int local_set_kernel_ver(const char * ver) {

	ERROR("Not implemented yet");
	(void)ver;
	return -1;

}

int local_get_initfs_ver(char * ver, size_t size) {

	strncpy(ver, initfs_ver, size);
	ver[size-1] = 0;
	return 0;

}

int local_set_initfs_ver(const char * ver) {

	ERROR("Not implemented yet");
	(void)ver;
	return -1;

}

int local_get_nolo_ver(char * ver, size_t size) {

	strncpy(ver, nolo_ver, size);
	ver[size-1] = 0;
	return 0;

}

int local_set_nolo_ver(const char * ver) {

	ERROR("Not implemented yet");
	(void)ver;
	return -1;

}

int local_get_sw_ver(char * ver, size_t size) {

	strncpy(ver, sw_ver, size);
	ver[size-1] = 0;
	return 0;

}

int local_set_sw_ver(const char * ver) {

	ERROR("Not implemented yet");
	(void)ver;
	return -1;

}

int local_get_content_ver(char * ver, size_t size) {

	strncpy(ver, content_ver, size);
	ver[size-1] = 0;
	return 0;

}

int local_set_content_ver(const char * ver) {

	ERROR("Not implemented yet");
	(void)ver;
	return -1;

}
