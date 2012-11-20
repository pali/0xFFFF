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

#include "disk.h"
#include "global.h"
#include "image.h"
#include "device.h"
#include "usb-device.h"

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
