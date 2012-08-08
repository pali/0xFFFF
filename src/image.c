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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "device.h"
#include "image.h"

#define IMAGE_CHECK(image) do { if ( ! image || image->fd < 0 ) return; } while (0)
#define IMAGE_STORE_CUR(image) do { if ( image->is_shared_fd ) image->cur = lseek(image->fd, 0, SEEK_CUR) - image->offset; } while (0)
#define IMAGE_RESTORE_CUR(image) do { if ( image->is_shared_fd ) lseek(image->fd, image->offset + image->cur, SEEK_SET); } while (0)

static void image_missing_values_from_name(struct image * image, const char * name) {

	char * str;
	char * ptr;
	char * ptr2;
	char * type = NULL;
	char * device = NULL;
	char * hwrevs = NULL;
	char * version = NULL;

	str = strdup(name);
	if ( ! str )
		return;

	ptr = strchr(str, '_');
	if ( ptr ) {
		*ptr = 0;
		++ptr;
		version = strdup(ptr);
	}

	ptr = strchr(str, '-');
	if ( ptr ) {
		*ptr = 0;
		++ptr;
		type = strdup(str);
		ptr2 = strchr(ptr, ':');
		if ( ptr2 ) {
			*ptr2 = 0;
			++ptr2;
			hwrevs = strdup(ptr2);
		}
		device = strdup(ptr);
	}

	if ( ! image->type )
		image->type = image_type_from_string(type);
	free(type);

	if ( ! image->device )
		image->device = device_from_string(device);
	free(device);

	if ( ! image->hwrevs )
		image->hwrevs = hwrevs;
	else
		free(hwrevs);

	if ( ! image->version )
		image->version = version;
	else
		free(version);

}

/* format: type-device:hwrevs_version */
char * image_name_alloc_from_values(struct image * image) {

	char * name;
	char * ptr;
	size_t length;
	const char * type;
	const char * device;

	type = image_type_to_string(image->type);
	device = device_to_string(image->device);

	if ( ! type )
		type = "unknown";

	length = 1 + strlen(type);

	if ( device )
		length += 1 + strlen(device);
	if ( image->hwrevs )
		length += 1 + strlen(image->hwrevs);
	if ( image->version )
		length += 1 + strlen(image->version);

	name = calloc(1, length);
	if ( ! name ) {
		perror("Alloc error");
		return NULL;
	}

	strcpy(name, type);
	ptr = name + strlen(name);

	if ( device )
		ptr += sprintf(ptr, "-%s", device);
	if ( image->hwrevs )
		ptr += sprintf(ptr, ":%s", image->hwrevs);
	if ( image->version )
		ptr += sprintf(ptr, "_%s", image->version);

	return name;

}

static void image_append(struct image * image, const char * type, const char * device, const char * hwrevs, const char * version, const char * layout) {

	IMAGE_CHECK(image);

	image->hash = image_hash_from_data(image);
	image->device = device_from_string(device);

	if ( type && type[0] )
		image->type = image_type_from_string(type);
	else
		image->type = image_type_from_data(image);

	if ( hwrevs && hwrevs[0] )
		image->hwrevs = strdup(hwrevs);
	else
		image->hwrevs = NULL;

	if ( version && version[0] )
		image->version = strdup(version);
	else
		image->version = NULL;

	if ( layout && layout[0] )
		image->layout = strdup(layout);
	else
		image->layout = NULL;

}

static void image_align(struct image * image) {

	size_t align = 0;

	if ( image->type == IMAGE_KERNEL)
		align = 7;
	else if ( image->type == IMAGE_MMC )
		align = 8;

	if ( align == 0 )
		return;

	if ( ( image->size & ( ( 1ULL << align ) - 1 ) ) == 0 )
		return;

	align = ((image->size >> align) + 1) << align;

	image->align = align - image->size;

	image->hash = image_hash_from_data(image);

}

static struct image * image_alloc(void) {

	struct image * image = calloc(1, sizeof(struct image));
	if ( ! image ) {
		perror("Cannot allocate memory");
		return NULL;
	}
	return image;

}

struct image * image_alloc_from_file(const char * file, const char * type, const char * device, const char * hwrevs, const char * version, const char * layout) {

	struct image * image = image_alloc();
	if ( ! image )
		return NULL;

	image->is_shared_fd = 0;
	image->fd = open(file, O_RDONLY);
	if ( image->fd < 0 ) {
		perror("Cannot open file");
		free(image);
		return NULL;
	}

	image->size = lseek(image->fd, 0, SEEK_END);
	image->offset = 0;
	image->cur = 0;

	image_append(image, type, device, hwrevs, version, layout);

	if ( ( ! type || ! type[0] ) && ( ! device || ! device[0] ) && ( ! hwrevs || ! hwrevs[0] ) && ( ! version || ! version[0] ) )
		image_missing_values_from_name(image, file);

	image_align(image);

	return image;

}

struct image * image_alloc_from_shared_fd(int fd, size_t size, size_t offset, uint16_t hash, const char * type, const char * device, const char * hwrevs, const char * version, const char * layout) {

	struct image * image = image_alloc();
	if ( ! image )
		return NULL;

	image->is_shared_fd = 1;
	image->fd = fd;
	image->size = size;
	image->offset = offset;
	image->cur = 0;

	image_append(image, type, device, hwrevs, version, layout);
	image_align(image);

	if ( image->hash != hash ) {
		fprintf(stderr, "Error: Image hash mishmash (expected %#04x)\n", image->hash);
		image_free(image);
		return NULL;
	}

	return image;

}

void image_free(struct image * image) {

	IMAGE_CHECK(image);

	if ( ! image->is_shared_fd ) {
		close(image->fd);
		image->fd = -1;
	}

	free(image->hwrevs);
	free(image->version);
	free(image->layout);

	free(image);

}

void image_seek(struct image * image, off_t whence) {

	IMAGE_CHECK(image);

	if ( whence > image->size )
		return;

	if ( whence >= image->size - image->align ) {
		lseek(image->fd, image->size - image->align - 1, SEEK_SET);
		image->acur = whence - ( image->size - image->align );
	} else {
		lseek(image->fd, image->offset + whence, SEEK_SET);
		image->acur = 0;
	}

	IMAGE_STORE_CUR(image);

}

size_t image_read(struct image * image, void * buf, size_t count) {

	size_t new_count = 0;
	size_t ret_count = 0;

	IMAGE_CHECK(image);
	IMAGE_RESTORE_CUR(image);

	if ( image->size < image->cur )
		return 0;

	if ( image->cur < image->size - image->align ) {

		if ( image->cur + count > image->size - image->align )
			new_count = image->size - image->align - image->cur;
		else
			new_count = count;

		ret_count += read(image->fd, buf, new_count);

		IMAGE_STORE_CUR(image);

		if ( new_count != ret_count )
			return ret_count;

	}

	if ( ret_count == count )
		return ret_count;

	if ( image->align && image->cur == image->size - image->align - 1 && image->acur < image->align ) {

		if ( image->acur + count - ret_count > image->align )
			new_count = image->align - image->acur;
		else
			new_count = count - ret_count;

		memset(buf+ret_count, 0xFF, new_count);
		ret_count += new_count;
		image->acur += new_count;

	}

	return ret_count;

}

void image_list_add(struct image_list ** list, struct image * image) {

	IMAGE_CHECK(image);
	struct image_list * last = calloc(1, sizeof(struct image_list));
	if ( ! last )
		return;

	last->image = image;

	if ( ! *list ) {
		*list = last;
		return;
	}

	while ( (*list)->next )
		list = &((*list)->next);

	last->prev = *list;
	(*list)->next = last;

}

void image_list_del(struct image_list * list) {

	if ( ! list )
		return;

	if ( list->prev )
		list->prev->next = list->next;

	if ( list->next )
		list->next->prev = list->prev;

	image_free(list->image);
	free(list);

}

static uint16_t do_hash(uint16_t * b, int len) {

	uint16_t result = 0;

	for ( len >>= 1; len--; b = b+1 )
		result^=b[0];

	return result;

}

uint16_t image_hash_from_data(struct image * image) {

	unsigned char buf[0x20000];
	uint16_t hash = 0;
	int ret;

	while ( ( ret = image_read(image, &buf, sizeof(buf)) ) )
		hash ^= do_hash((uint16_t *)&buf, ret);

	return hash;
}

static const char * image_types[] = {
	[IMAGE_XLOADER] = "xloader",
	[IMAGE_2ND] = "2nd",
	[IMAGE_SECONDARY] = "secondary",
	[IMAGE_KERNEL] = "kernel",
	[IMAGE_INITFS] = "initfs",
	[IMAGE_ROOTFS] = "rootfs",
	[IMAGE_OMAP_NAND] = "omap-nand",
	[IMAGE_MMC] = "mmc",
	[IMAGE_CMT_2ND] = "cmt-2nd",
	[IMAGE_CMT_ALGO] = "cmt-algo",
	[IMAGE_CMT_MCUSW] = "cmt-mcusw",
};

enum image_type image_type_from_data(struct image * image) {

	IMAGE_CHECK(image);
	unsigned char buf[512];

	image_seek(image, 0);
	image_read(image, buf, sizeof(buf));

	if ( memcmp(buf+0x34, "2NDAPE", 6) == 0 )
		return IMAGE_2ND;
	else if ( memcmp(buf+0x14, "2ND", 3) == 0 )
		return IMAGE_2ND;
	else if ( memcmp(buf+0x04, "NOLOScnd", 8) == 0 )
		return IMAGE_SECONDARY;
	else if ( memcmp(buf+0x14, "X-LOADER", 8) == 0 )
		return IMAGE_XLOADER;
	else if ( memcmp(buf+0x0c, "NOLOXldr", 8) == 0 )
		return IMAGE_XLOADER;
	else if ( memcmp(buf+4, "NOLOXldr", 8) == 0 )
		return IMAGE_2ND;
	else if ( memcmp(buf, "\x00\x00\xa0\xe1\x00\x00\xa0\xe1", 8) == 0 )
		return IMAGE_KERNEL;
	else if ( memcmp(buf, "\x21\x01\x01", 3) == 0 )
		return IMAGE_KERNEL;
	else if ( memcmp(buf, "UBI#", 4) == 0 ) /* UBI EC header */
		return IMAGE_ROOTFS;
	else if ( memcmp(buf, "\xb0\x00\x01\x03\x9d\x00\x00\x00", 8) == 0 )
		return IMAGE_CMT_2ND;
	else if ( memcmp(buf, "\xb1\x00\x00\x00\x82\x00\x00\x00", 8) == 0 )
		return IMAGE_CMT_ALGO;
	else if ( memcmp(buf, "\xb2\x00\x00\x01\x44\x00\x00\x00", 8) == 0 )
		return IMAGE_CMT_MCUSW;
	else if ( memcmp(buf, "\x85\x19\x01\xe0", 2) == 0 ) { /* JFFS2 MAGIC */
		if ( image->size < 0x300000 )
			return IMAGE_INITFS;
		else
			return IMAGE_ROOTFS;
	}

	return IMAGE_UNKNOWN;

}

enum image_type image_type_from_string(const char * type) {

	size_t i;

	if ( ! type )
		return IMAGE_UNKNOWN;

	for ( i = 0; i < sizeof(image_types)/sizeof(image_types[0]); ++i )
		if ( image_types[i] && strcmp(image_types[i], type) == 0 )
			return i;

	return IMAGE_UNKNOWN;

}

const char * image_type_to_string(enum image_type type) {

	if ( type > sizeof(image_types)/sizeof(image_types[0]) )
		return NULL;

	return image_types[type];

}
