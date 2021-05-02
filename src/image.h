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

#ifndef IMAGE_H
#define IMAGE_H

#include <stdint.h>
#include <sys/types.h>

#include "device.h"

enum image_type {
	IMAGE_UNKNOWN = 0,
	IMAGE_XLOADER,
	IMAGE_2ND,
	IMAGE_SECONDARY,
	IMAGE_KERNEL,
	IMAGE_INITFS,
	IMAGE_ROOTFS,
	IMAGE_MMC,
	IMAGE_CMT_2ND,
	IMAGE_CMT_ALGO,
	IMAGE_CMT_MCUSW,
	IMAGE_COUNT,
};

struct image_part {
	struct image_part * next;
	uint32_t offset;
	uint32_t size;
	char * name;
};

struct image {
	enum image_type type;
	struct device_list * devices;
	char * version;
	char * layout;
	uint16_t hash;
	uint32_t size;
	struct image_part * parts;

	int fd;
	int is_shared_fd;
	uint32_t align;
	size_t offset;
	size_t cur;
	size_t acur;
	char * orig_filename;
};

struct image_list {
	struct image * image;
	struct image_list * prev;
	struct image_list * next;
};

struct image * image_alloc_from_file(const char * file, const char * type, const char * device, const char * hwrevs, const char * version, const char * layout, struct image_part * parts);
struct image * image_alloc_from_fd(int fd, const char * orig_filename, const char * type, const char * device, const char * hwrevs, const char * version, const char * layout, struct image_part * parts);
struct image * image_alloc_from_shared_fd(int fd, size_t size, size_t offset, uint16_t hash, const char * type, const char * device, const char * hwrevs, const char * version, const char * layout, struct image_part * parts);
void image_free(struct image * image);
void image_seek(struct image * image, size_t whence);
size_t image_read(struct image * image, void * buf, size_t count);
void image_print_info(struct image * image);
void image_list_add(struct image_list ** list, struct image * image);
void image_list_del(struct image_list * list);
void image_list_unlink(struct image_list * list);

uint16_t image_hash_from_data(struct image * image);
enum image_type image_type_from_data(struct image * image);
char * image_name_alloc_from_values(struct image * image, int part_num);
enum image_type image_type_from_string(const char * type);
const char * image_type_to_string(enum image_type type);
int image_hwrev_is_valid(struct image * image, int16_t hwrev);

#endif
