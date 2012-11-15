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

#ifndef MKII_H
#define MKII_H

#include "image.h"
#include "device.h"
#include "usb-device.h"

int mkii_init(struct usb_device_info * dev);

enum device mkii_get_device(struct usb_device_info * dev);

int mkii_flash_image(struct usb_device_info * dev, struct image * image);
int mkii_reboot_device(struct usb_device_info * dev);

int mkii_get_root_device(struct usb_device_info * dev);
int mkii_set_root_device(struct usb_device_info * dev, int device);

int mkii_get_usb_host_mode(struct usb_device_info * dev);
int mkii_set_usb_host_mode(struct usb_device_info * dev, int enable);

int mkii_get_rd_mode(struct usb_device_info * dev);
int mkii_set_rd_mode(struct usb_device_info * dev, int enable);

int mkii_get_rd_flags(struct usb_device_info * dev, char * flags, size_t size);
int mkii_set_rd_flags(struct usb_device_info * dev, const char * flags);

int16_t mkii_get_hwrev(struct usb_device_info * dev);
int mkii_set_hwrev(struct usb_device_info * dev, int16_t hwrev);

int mkii_get_kernel_ver(struct usb_device_info * dev, char * ver, size_t size);
int mkii_set_kernel_ver(struct usb_device_info * dev, const char * ver);

int mkii_get_nolo_ver(struct usb_device_info * dev, char * ver, size_t size);
int mkii_set_nolo_ver(struct usb_device_info * dev, const char * ver);

int mkii_get_sw_ver(struct usb_device_info * dev, char * ver, size_t size);
int mkii_set_sw_ver(struct usb_device_info * dev, const char * ver);

int mkii_get_content_ver(struct usb_device_info * dev, char * ver, size_t size);
int mkii_set_content_ver(struct usb_device_info * dev, const char * ver);

#endif
