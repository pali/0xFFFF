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

#ifndef LOCAL_H
#define LOCAL_H

#include "image.h"
#include "device.h"

int local_init(void);

enum device local_get_device(void);

int local_flash_image(struct image * image);
int local_dump_image(enum image_type image, const char * file);
int local_check_badblocks(const char * device);

int local_reboot_device(void);

int local_get_root_device(void);
int local_set_root_device(int device);

int local_get_usb_host_mode(void);
int local_set_usb_host_mode(int enable);

int local_get_rd_mode(void);
int local_set_rd_mode(int enable);

int local_get_rd_flags(char * flags, size_t size);
int local_set_rd_flags(const char * flags);

int16_t local_get_hwrev(void);
int local_set_hwrev(int16_t hwrev);

int local_get_kernel_ver(char * ver, size_t size);
int local_set_kernel_ver(const char * ver);

int local_get_initfs_ver(char * ver, size_t size);
int local_set_initfs_ver(const char * ver);

int local_get_nolo_ver(char * ver, size_t size);
int local_set_nolo_ver(const char * ver);

int local_get_sw_ver(char * ver, size_t size);
int local_set_sw_ver(const char * ver);

int local_get_content_ver(char * ver, size_t size);
int local_set_content_ver(const char * ver);

#endif
