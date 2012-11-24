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

#include <fcntl.h>
#include <libgen.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/statvfs.h>

#include <linux/fs.h>

#include "disk.h"
#include "global.h"
#include "image.h"
#include "device.h"
#include "usb-device.h"
#include "printf-utils.h"

static char global_buf[1 << 22]; /* 4MB */

int disk_init(struct usb_device_info * dev) {

	ERROR("RAW mode is not implemented yet");
	(void)dev;
	return -1;

}

enum device disk_get_device(struct usb_device_info * dev) {

	ERROR("Not implemented yet");
	(void)dev;
	return DEVICE_UNKNOWN;

}

int disk_flash_raw(const char * blkdev, const char * file) {

	ERROR("Not implemented yet");
	(void)blkdev;
	(void)file;
	return -1;

}

int disk_flash_image(struct usb_device_info * dev, struct image * image) {

	ERROR("Not implemented yet");
	(void)dev;
	(void)image;
	return -1;

}

int disk_dump_raw(const char * blkdev, const char * file) {

	int fd1, fd2;
	int ret;
	char * path;
	uint64_t blksize;
	size_t need, readed;
	ssize_t size;
	struct stat st;
	struct statvfs buf;

	printf("Dump block device %s to file %s...\n", blkdev, file);

	if ( stat(blkdev, &st) != 0 ) {
		ERROR_INFO("Cannot stat block device %s", blkdev);
		return -1;
	}

	if ( ! S_ISBLK(st.st_mode) ) {
		ERROR("Invalid block device %s", blkdev);
		return -1;
	}

	fd1 = open(blkdev, O_RDONLY);

	if ( fd1 < 0 ) {
		ERROR_INFO("Cannot open block device %s", blkdev);
		return -1;
	}

	if ( ioctl(fd1, BLKGETSIZE64, &blksize) != 0 ) {
		ERROR_INFO("Cannot get size of block device %s", blkdev);
		close(fd1);
		return -1;
	}

	if ( blksize > ULLONG_MAX ) {
		ERROR("Block device %s is too big", blkdev);
		close(fd1);
		return -1;
	}

	if ( blksize == 0 ) {
		ERROR("Block device %s has zero size", blkdev);
		close(fd1);
		return -1;
	}

	path = strdup(file);
	if ( ! path ) {
		ALLOC_ERROR();
		close(fd1);
		return -1;
	}

	ret = statvfs(dirname(path), &buf);

	free(path);

	if ( ret == 0 && buf.f_bsize * buf.f_bfree < blksize ) {
		ERROR("Not enought free space (have: %llu, need: %llu)", (unsigned long long int)(buf.f_bsize) * buf.f_bfree, (unsigned long long int)blksize);
		close(fd1);
		return -1;
	}

	fd2 = creat(file, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	if ( fd2 < 0 ) {
		ERROR_INFO("Cannot create file %s", file);
		close(fd1);
		return -1;
	}

	readed = 0;
	printf_progressbar(0, blksize);

	while ( readed < blksize ) {
		need = blksize - readed;
		if ( need > sizeof(global_buf) )
			need = sizeof(global_buf);
		size = read(fd1, global_buf, need);
		if ( size == 0 )
			break;
		if ( write(fd2, global_buf, size) != size ) {
			PRINTF_ERROR("Dumping image failed");
			close(fd1);
			close(fd2);
			return -1;
		}
		readed += size;
		printf_progressbar(readed, blksize);
	}

	close(fd1);
	close(fd2);
	return 0;

}

int disk_dump_image(struct usb_device_info * dev, enum image_type image, const char * file) {

	ERROR("Not implemented yet");
	(void)dev;
	(void)image;
	(void)file;
	return -1;

}

int disk_check_badblocks(struct usb_device_info * dev, const char * device) {

	ERROR("Not implemented yet");
	(void)dev;
	(void)device;
	return -1;

}
