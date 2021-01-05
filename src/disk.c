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
#include <sys/sysmacros.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <dirent.h>
#include <errno.h>
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
	int old_errno;
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

		fd = open(blkdev, (readonly ? O_RDONLY : O_RDWR) | O_EXCL | O_NONBLOCK);
		if ( fd < 0 ) {
			ERROR_INFO("Cannot open block device %s", blkdev);
			return -1;
		} else {
			if ( fstat(fd, &st) != 0 || ! S_ISBLK(st.st_mode) || makedev(maj, min) != st.st_rdev ) {
				ERROR("Block device %s does not have id %d:%d\n", blkdev, maj, min);
				close(fd);
				return -1;
			}
		}

	} else if ( partition > 0 ) {

		/* Select partition */

		len = strlen(blkdev);
		if ( sizeof(blkdev) <= len+2 ) {
			ERROR("Block device name is too long");
			return -1;
		}

		if ( snprintf(blkdev+len, sizeof(blkdev)-len, "p%d", partition) >= (int)(sizeof(blkdev)-len) ) {
			ERROR("Block device name is too long");
			return -1;
		}
		fd = open(blkdev, (readonly ? O_RDONLY : O_RDWR) | O_EXCL);
		if ( fd < 0 && errno == ENOENT ) {
			if ( snprintf(blkdev+len, sizeof(blkdev)-len, "%d", partition) >= (int)(sizeof(blkdev)-len) ) {
				ERROR("Block device name is too long");
				return -1;
			}
			fd = open(blkdev, (readonly ? O_RDONLY : O_RDWR) | O_EXCL);
		}
		if ( fd < 0 && errno == ENOENT ) {
			blkdev[len] = 0;
			fd = open(blkdev, O_RDONLY | O_NONBLOCK);
			if ( fd < 0 ) {
				ERROR_INFO("Cannot open block device %s", blkdev);
			} else {
				close(fd);
				ERROR("Block device %s does not have partitions", blkdev);
			}
			return -1;
		}
		if ( fd < 0 )
			old_errno = errno;

		printf("Found block device %s for partition %d\n", blkdev, partition);

		if ( fd < 0 ) {
			errno = old_errno;
			ERROR_INFO("Cannot open block device %s", blkdev);
		} else if ( fstat(fd, &st) != 0 || ! S_ISBLK(st.st_mode) ) {
			ERROR("Block device %s is not block device\n", blkdev);
			close(fd);
			return -1;
		}

	} else {

		ERROR("Invalid partition %d for block device %s", partition, blkdev);
		fd = -1;

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
	size_t need, sent;
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
	if ( (off_t)blksize == (off_t)-1 ) {
		ERROR_INFO("Cannot get size of block device");
		return -1;
	}

	if ( lseek(fd, 0, SEEK_SET) == (off_t)-1 ) {
		ERROR_INFO("Cannot seek to begin of block device");
		return -1;
	}

#endif

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

	sent = 0;
	printf_progressbar(0, blksize);

	while ( sent < blksize ) {
		need = blksize - sent;
		if ( need > sizeof(global_buf) )
			need = sizeof(global_buf);
		size = read(fd, global_buf, need);
		if ( size == 0 )
			break;
		if ( size < 0 ) {
			PRINTF_ERROR("Reading from block device failed");
			close(fd2);
			return -1;
		}
		if ( write(fd2, global_buf, size) != size ) {
			PRINTF_ERROR("Dumping image failed");
			close(fd2);
			return -1;
		}
		sent += size;
		printf_progressbar(sent, blksize);
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
	int maj1;
	int maj2;
	int min1;
	int min2;
	int tmp;

	maj1 = -1;
	maj2 = -1;
	min1 = -1;
	min2 = -1;

	FILE * f;
	DIR * dir;
	struct dirent * dirent;
	char buf[1024];
	unsigned int devnum;
	unsigned int busnum;

	struct usb_device * device;

	device = usb_device(dev->udev);
	if ( ! device || ! device->bus ) {
		ERROR_INFO("Cannot read usb devnum and busnum");
		return -1;
	}

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

		if ( device->devnum != devnum )
			continue;

		if ( device->bus->location ) {
			if ( device->bus->location != busnum )
				continue;
		} else if ( device->bus->dirname[0] ) {
			if ( atoi(device->bus->dirname) != (int)busnum )
				continue;
		}

		if ( sscanf(dirent->d_name, "%d:%d", &maj2, &min2) != 2 ) {
			maj2 = -1;
			min2 = -1;
			continue;
		}

		if ( maj1 != -1 && min1 != -1 && maj2 != -1 && min2 != -1 )
			break;

		maj1 = maj2;
		min1 = min2;
		maj2 = -1;
		min2 = -1;

	}

	closedir(dir);

	if ( maj1 == -1 || min1 == -1 ) {
		ERROR("Cannot find id for mmc block disk device");
		return -1;
	}

	/* Ensure that maj1:min1 is first device and maj2:min2 is second device */
	if ( min2 != -1 && min2 < min1 ) {
		tmp = min1;
		min1 = min2;
		min2 = tmp;
		tmp = maj1;
		maj1 = maj2;
		maj2 = tmp;
	}

	/* TODO: change 1 to 0 when disk_flash_dev will be implemented */

	/* RX-51, RM-680 and RM-696 export MyDocs in first usb device and just first partion, so host system see whole device without MBR table */
	if ( dev->device == DEVICE_RX_51 || dev->device == DEVICE_RM_680 || dev->device == DEVICE_RM_696 )
		fd = disk_open_dev(maj1, min1, -1, 1);
	/* Other devices can export SD card as first partition and export whole mmc device, so host system will see MBR table */
	else if ( maj2 != -1 && min2 != -1 )
		fd = disk_open_dev(maj2, min2, 1, 1);
	else
		fd = disk_open_dev(maj1, min1, 1, 1);

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

void disk_exit(struct usb_device_info * dev) {

	if ( dev->data >= 0 )
		close(dev->data);

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
