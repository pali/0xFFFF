/*
    cold-flash.h - Cold flashing
    Copyright (C) 2011-2012  Pali Rohár <pali.rohar@gmail.com>

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

#ifndef COLD_FLASH_H
#define COLD_FLASH_H

#include "image.h"
#include "usb-device.h"

/* Initialize Cold Flash mde */
int init_cold_flash(struct usb_device_info * dev);

/* Flash 2nd and secondary image in Cold Flash mode. After flashing device will boot secondary image */
int cold_flash(struct usb_device_info * dev, struct image * x2nd, struct image * secondary);

/* Leave Cold Flashing mode and continue booting */
int leave_cold_flash(struct usb_device_info * dev);

#endif
