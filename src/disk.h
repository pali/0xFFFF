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

#ifndef DISK_H
#define DISK_H

#include "image.h"
#include "device.h"
#include "usb-device.h"

int disk_init(struct usb_device_info * dev);

enum device disk_get_device(struct usb_device_info * dev);

int disk_flash_raw(const char * blkdev, const char * file);
int disk_dump_raw(const char * blkdev, const char * file);
int disk_open_dev(int maj, int min, int partition, int readonly);
int disk_dump_dev(int fd, const char * file);
int disk_flash_dev(int fd, const char * file);

int disk_flash_image(struct usb_device_info * dev, struct image * image);
int disk_dump_image(struct usb_device_info * dev, enum image_type image, const char * file);
int disk_check_badblocks(struct usb_device_info * dev, const char * device);

#endif
