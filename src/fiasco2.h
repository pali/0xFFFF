/*
    0xFFFF - Open Free Fiasco Firmware Flasher
    Copyright (C) 2007-2011  pancake <pancake@youterm.com>
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

#ifndef FIASCO_H
#define FIASCO_H

#include "image.h"

struct fiasco {
	char name[257];
	char swver[257];
	int fd;
	struct image_list * first;
};

struct fiasco * fiasco_alloc_empty(void);
struct fiasco * fiasco_alloc_from_file(const char * file);
void fiasco_free(struct fiasco * fiasco);
void fiasco_add_image(struct fiasco * fiasco, struct image * image);
int fiasco_write_to_file(struct fiasco * fiasco, const char * file);
int fiasco_unpack(struct fiasco * fiasco, const char * dir);

#endif
