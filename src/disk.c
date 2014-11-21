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

int disk_open_dev(int maj, int min, int partition, int readonly) {

	int fd;
	struct stat st;
	DIR * dir;
	struct dirent * dirent;
	int found;
	size_t len;
	char blkdev[1024];

	dir = opendir("/dev/");
	if ( ! dir ) {
		ERROR_INFO("Cannot open '/dev/' directory");
		return -1;
	}

	found = 0;

	while ( ( dirent = readdir(dir) ) ) {

		if ( snprintf(blkdev, sizeof(blkdev), "/dev/%s", dirent->d_name) <= 0 )
			continue;

		if ( stat(blkdev, &st) != 0 )
			continue;

		if ( ! S_ISBLK(st.st_mode) )
			continue;

		if ( makedev(maj, min) != st.st_rdev )
			continue;

		found = 1;
		break;

	}

	closedir(dir);

	if ( ! found ) {
		ERROR("Cannot find block device with id %d:%d", maj, min);
		return -1;
	}

	printf("Found block device %s with id %d:%d\n", blkdev, maj, min);

	if ( partition == -1 ) {

		/* Check if block device does not have partitions */

		len = strlen(blkdev);
		if ( sizeof(blkdev) <= len+2 ) {
			ERROR("Block device name is too long");
			return -1;
		}

		memcpy(blkdev+len, "p1", 3);
		if ( stat(blkdev, &st) == 0 ) {
			ERROR("Block device has partitions");
			return -1;
		}

		memcpy(blkdev+len, "1", 2);
		if ( stat(blkdev, &st) == 0 ) {
			ERROR("Block device has partitions");
			return -1;
		}

		blkdev[len] = 0;

	} else if ( partition > 0 ) {

		/* Select partition */

		len = strlen(blkdev);
		if ( sizeof(blkdev) <= len+2 ) {
			ERROR("Block device name is too long");
			return -1;
		}

		memcpy(blkdev+len, "p1", 3);
		if ( stat(blkdev, &st) != 0 || ! S_ISBLK(st.st_mode) ) {
			memcpy(blkdev+len, "1", 2);
			if ( stat(blkdev, &st) != 0 || ! S_ISBLK(st.st_mode) ) {
				ERROR("Block device has partitions");
				return -1;
			}
		}

		printf("Found block device %s for partition %d\n", blkdev, partition);

	}

	fd = open(blkdev, (readonly ? O_RDONLY : O_RDWR) | O_EXCL);

	if ( fd < 0 ) {
		ERROR_INFO("Cannot open block device %s", blkdev);
		return -1;
	}

	return fd;

}

int disk_dump_dev(int fd, const char * file) {

	int fd2;
	int ret;
	char * path;
	uint64_t blksize;
	size_t need, readed;
	ssize_t size;
	struct statvfs buf;

	printf("Dump block device to file %s...\n", file);

	if ( ioctl(fd, BLKGETSIZE64, &blksize) != 0 ) {
		ERROR_INFO("Cannot get size of block device");
		return -1;
	}

	if ( blksize > ULLONG_MAX ) {
		ERROR("Block device is too big");
		return -1;
	}

	if ( blksize == 0 ) {
		ERROR("Block device has zero size");
		return -1;
	}

	path = strdup(file);
	if ( ! path ) {
		ALLOC_ERROR();
		return -1;
	}

	ret = statvfs(dirname(path), &buf);

	free(path);

	if ( ret == 0 && buf.f_bsize * buf.f_bfree < blksize ) {
		ERROR("Not enough free space (have: %llu, need: %llu)", (unsigned long long int)(buf.f_bsize) * buf.f_bfree, (unsigned long long int)blksize);
		return -1;
	}

	fd2 = creat(file, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	if ( fd2 < 0 ) {
		ERROR_INFO("Cannot create file %s", file);
		return -1;
	}

	readed = 0;
	printf_progressbar(0, blksize);

	while ( readed < blksize ) {
		need = blksize - readed;
		if ( need > sizeof(global_buf) )
			need = sizeof(global_buf);
		size = read(fd, global_buf, need);
		if ( size == 0 )
			break;
		if ( write(fd2, global_buf, size) != size ) {
			PRINTF_ERROR("Dumping image failed");
			close(fd2);
			return -1;
		}
		readed += size;
		printf_progressbar(readed, blksize);
	}

	close(fd2);
	return 0;

}

int disk_flash_dev(int fd, const char * file) {

	ERROR("Not implemented yet");
	(void)fd;
	(void)file;
	return -1;

}

int disk_init(struct usb_device_info * dev) {

	(void)dev;
	return 0;

}

enum device disk_get_device(struct usb_device_info * dev) {

	return dev->device;

}

int disk_flash_image(struct usb_device_info * dev, struct image * image) {

	ERROR("Not implemented yet");
	(void)dev;
	(void)image;
	return -1;

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
