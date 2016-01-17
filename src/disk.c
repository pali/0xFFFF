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
#include <unistd.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

#ifdef __linux__
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <dirent.h>
#endif

#include "disk.h"
#include "global.h"
#include "image.h"
#include "device.h"
#include "usb-device.h"
#include "printf-utils.h"

static char global_buf[1UL << 22]; /* 4MB */

int disk_open_dev(int maj, int min, int partition, int readonly) {

#ifdef __linux__

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

		if ( strncmp(dirent->d_name, ".", sizeof(".")) == 0 || strncmp(dirent->d_name, "..", sizeof("..")) == 0 )
			continue;

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

#else

	ERROR("Not implemented yet");
	(void)min;
	(void)maj;
	(void)partition;
	(void)readonly;
	return -1;

#endif

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

#ifdef __linux__

	if ( ioctl(fd, BLKGETSIZE64, &blksize) != 0 ) {
		ERROR_INFO("Cannot get size of block device");
		return -1;
	}

#else

	blksize = lseek(fd, 0, SEEK_END);
	if ( blksize == (off_t)-1 ) {
		ERROR_INFO("Cannot get size of block device");
		return -1;
	}

	if ( lseek(fd, 0, SEEK_SET) == (off_t)-1 ) {
		ERROR_INFO("Cannot seek to begin of block device");
		return -1;
	}

#endif

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

#ifdef __linux__

	int fd;
	int maj;
	int min;

	maj = -1;
	min = -1;

	FILE * f;
	DIR * dir;
	struct dirent * dirent;
	char buf[1024];
	unsigned int devnum;
	unsigned int busnum;

	uint8_t usbdevnum;
	uint8_t usbbusnum;

	struct libusb_device * device;

	device = libusb_get_device(dev->udev);
	if ( ! device ) {
		ERROR_INFO("Cannot read usb devnum and busnum");
		return -1;
	}

	usbbusnum = libusb_get_bus_number(device);
	usbdevnum = libusb_get_port_number(device);

	dir = opendir("/sys/dev/block/");
	if ( ! dir ) {
		ERROR_INFO("Cannot open '/sys/dev/block/' directory");
		return -1;
	}

	while ( ( dirent = readdir(dir) ) ) {

		if ( strncmp(dirent->d_name, ".", sizeof(".")) == 0 || strncmp(dirent->d_name, "..", sizeof("..")) == 0 )
			continue;

		if ( snprintf(buf, sizeof(buf), "/sys/dev/block/%s/device/../../../../busnum", dirent->d_name) <= 0 )
			continue;

		f = fopen(buf, "r");
		if ( ! f )
			continue;

		if ( fscanf(f, "%u", &busnum) != 1 ) {
			fclose(f);
			continue;
		}

		fclose(f);

		if ( snprintf(buf, sizeof(buf), "/sys/dev/block/%s/device/../../../../devnum", dirent->d_name) <= 0 )
			continue;

		f = fopen(buf, "r");
		if ( ! f )
			continue;

		if ( fscanf(f, "%u", &devnum) != 1 ) {
			fclose(f);
			continue;
		}

		fclose(f);

		if ( devnum != usbdevnum || usbbusnum != busnum )
			continue;

		if ( sscanf(dirent->d_name, "%d:%d", &maj, &min) != 2 ) {
			maj = -1;
			min = -1;
			continue;
		}

		break;

	}

	closedir(dir);

	if ( maj == -1 || min == -1 ) {
		ERROR("Cannot find id for mmc block disk device");
		return -1;
	}

	/* TODO: change 1 to 0 when disk_flash_dev will be implemented */
	fd = disk_open_dev(maj, min, -1, 1);

	if ( fd < 0 )
		return -1;

	dev->data = fd;
	return 0;

#else

	ERROR("Not implemented yet");
	(void)dev;
	return -1;

#endif

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

	if ( image != IMAGE_MMC )
		ERROR_RETURN("Only mmc images are supported", -1);

	return disk_dump_dev(dev->data, file);

}

int disk_check_badblocks(struct usb_device_info * dev, const char * device) {

	ERROR("Not implemented yet");
	(void)dev;
	(void)device;
	return -1;

}
