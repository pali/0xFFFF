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

#ifndef OPERATIONS_H
#define OPERATIONS_H

#include "image.h"
#include "usb-device.h"

enum connection_method {
	METHOD_UNKNOWN = 0,
	METHOD_USB,
	METHOD_LOCAL,
};

struct device_info {
	enum connection_method method;
	enum device detected_device;
	int16_t detected_hwrev;
	struct usb_device_info * usb;
};

struct device_info * dev_detect(void);
void dev_free(struct device_info * dev);

enum device dev_get_device(struct device_info * dev);

int dev_cold_flash_images(struct device_info * dev, struct image * x2nd, struct image * secondary);
int dev_load_image(struct device_info * dev, struct image * image);
int dev_flash_image(struct device_info * dev, struct image * image);
struct image * dev_dump_image(struct device_info * dev, enum image_type image);
int dev_check_badblocks(struct device_info * dev, const char * device);

int dev_boot_device(struct device_info * dev, const char * cmdline);
int dev_reboot_device(struct device_info * dev);

int dev_get_root_device(struct device_info * dev);
int dev_set_root_device(struct device_info * dev, int device);

int dev_get_usb_host_mode(struct device_info * dev);
int dev_set_usb_host_mode(struct device_info * dev, int enable);

int dev_get_rd_mode(struct device_info * dev);
int dev_set_rd_mode(struct device_info * dev, int enable);

int dev_get_rd_flags(struct device_info * dev, char * flags, size_t size);
int dev_set_rd_flags(struct device_info * dev, const char * flags);

int16_t dev_get_hwrev(struct device_info * dev);
int dev_set_hwrev(struct device_info * dev, int16_t hwrev);

int dev_get_kernel_ver(struct device_info * dev, char * ver, size_t size);
int dev_set_kernel_ver(struct device_info * dev, const char * ver);

int dev_get_nolo_ver(struct device_info * dev, char * ver, size_t size);
int dev_set_nolo_ver(struct device_info * dev, const char * ver);

int dev_get_sw_ver(struct device_info * dev, char * ver, size_t size);
int dev_set_sw_ver(struct device_info * dev, const char * ver);

int dev_get_content_ver(struct device_info * dev, char * ver, size_t size);
int dev_set_content_ver(struct device_info * dev, const char * ver);

#endif
