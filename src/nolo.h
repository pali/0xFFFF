/*
 *  0xFFFF - Open Free Fiasco Firmware Flasher
 *  Copyright (C) 2007  pancake <pancake@youterm.com>
 *  Copyright (C) 2012  Pali Rohár <pali.rohar@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NOLO_H
#define NOLO_H

#include "image.h"

void nolo_flash_image(struct image * image);

extern char strbuf[1024];

int boot_board(char *cmdline);

int reboot_board();

int get_status();

int get_usb_mode();
int set_usb_mode(unsigned int);

int get_rd_mode();
int set_rd_mode(unsigned short mode);

int get_rd_flags();
int set_rd_flags(unsigned short flags);

int get_hw_revision(char *str, int len);

int get_root_device();
int set_root_device(unsigned short);

int get_nolo_version();

int get_sw_version();

#define NOLO_GET_STATUS   1
#define NOLO_GET_BOARD_ID 2
#define NOLO_GET_VERSION  3
#define NOLO_GET_HWVERSION 4
#define NOLO_SET_RDFLAGS   16
#define NOLO_GET_RDFLAGS  17

#endif
