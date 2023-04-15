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

#include "global.h"
#include "device.h"
#include "image.h"

/* format: type-device:hwrevs_version */
static void image_missing_values_from_name(struct image * image, const char * name) {

	char * str;
	char * ptr;
	char * ptr2;
	char * type = NULL;
	char * device = NULL;
	char * hwrevs = NULL;
	char * version = NULL;
	enum device new_device;

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
	} else {
		type = strdup(str);
	}

	if ( ! image->type )
		image->type = image_type_from_string(type);
	free(type);

	if ( ! image->devices || ! image->devices->device || image->devices->device == DEVICE_ANY ) {
		new_device = device_from_string(device);
		if ( new_device ) {
			if ( ! image->devices ) image->devices = calloc(1, sizeof(struct device_list));
			if ( image->devices )
				image->devices->device = new_device;
		}
	}
	free(device);

	if ( image->devices && image->devices->device && ! image->devices->hwrevs )
		image->devices->hwrevs = hwrevs_alloc_from_string(hwrevs);

	if ( ! image->version )
		image->version = version;
	else
		free(version);

	free(hwrevs);
	free(str);

}

/* format: type-device:hwrevs_version */
char * image_name_alloc_from_values(struct image * image, int part_num) {

	char * name;
	char * ptr;
	char * hwrevs;
	size_t length;
	const char * type;
	const char * device;
	struct image_part * part;
	int i;

	type = image_type_to_string(image->type);

	if ( ! type )
		type = "unknown";

	if ( image->devices )
		device = device_to_string(image->devices->device);
	else
		device = NULL;

	if ( image->devices && image->devices->hwrevs )
		hwrevs = hwrevs_alloc_to_string(image->devices->hwrevs);
	else
		hwrevs = NULL;

	part = image->parts;

	if ( part && ( !part->next || part_num < 0 ) )
		part = NULL;

	for ( i = 0; i < part_num && part; i++ )
		part = part->next;

	length = 1 + strlen(type);

	if ( device )
		length += 1 + strlen(device);
	if ( hwrevs )
		length += 1 + strlen(hwrevs);
	if ( image->version )
		length += 1 + strlen(image->version);
	if ( part )
		length += 4 + 3; /* 3 <= strlen(part_num) */
	if ( part && part->name )
		length += 1 + strlen(part->name);

	name = calloc(1, length);
	if ( ! name ) {
		free(hwrevs);
		ALLOC_ERROR_RETURN(NULL);
	}

	strcpy(name, type);
	ptr = name + strlen(name);

	if ( device )
		ptr += sprintf(ptr, "-%s", device);
	if ( hwrevs )
		ptr += sprintf(ptr, ":%s", hwrevs);
	if ( image->version )
		ptr += sprintf(ptr, "_%s", image->version);

	if ( part ) {
		ptr += sprintf(ptr, "_part%d", part_num+1);
		if ( part->name )
			ptr += sprintf(ptr, "_%s", part->name);
	}

	free(hwrevs);

	return name;

}

static int image_append(struct image * image, const char * type, const char * device, const char * hwrevs, const char * version, const char * layout, struct image_part * parts) {

	enum image_type detected_type;

	image->hash = image_hash_from_data(image);

	image->devices = calloc(1, sizeof(struct device_list));
	if ( ! image->devices ) {
		image_free(image);
		ALLOC_ERROR_RETURN(-1);
	}

	image->devices->device = DEVICE_ANY;

	if ( device && device[0] ) {
		image->devices->device = device_from_string(device);
		if ( ! noverify && image->devices->device == DEVICE_UNKNOWN ) {
			ERROR("Specified Device %s is unknown", device);
			image_free(image);
			return -1;
		}
	}

	detected_type = image_type_from_data(image);
	image->type = detected_type;

	if ( type && type[0] ) {
		image->type = image_type_from_string(type);
		if ( ! noverify && image->type == IMAGE_UNKNOWN ) {
			ERROR("Specified Image type %s is unknown", type);
			image_free(image);
			return -1;
		}
		if ( ! noverify && detected_type != image->type && detected_type != IMAGE_UNKNOWN ) {
			ERROR("Image type mishmash (detected %s, got %s)", image_type_to_string(detected_type), type);
			image_free(image);
			return -1;
		}
	}

	if ( hwrevs && hwrevs[0] )
		image->devices->hwrevs = hwrevs_alloc_from_string(hwrevs);
	else
		image->devices->hwrevs = NULL;

	if ( version && version[0] )
		image->version = strdup(version);
	else
		image->version = NULL;

	if ( layout && layout[0] )
		image->layout = strdup(layout);
	else
		image->layout = NULL;

	image->parts = parts;

	return 0;

}

static void image_align(struct image * image) {

	struct image_fd * image_fd = image->fds;
	size_t align, aligned_size;
	uint32_t total_size;

	if ( image->type == IMAGE_MMC )
		align = 8;
	else
		align = 7;

	if ( image_fd && ! image_fd->next && image->size == image_fd->size && ( image->size & ( ( 1ULL << align ) - 1 ) ) == 0 )
		return;

	total_size = 0;

	while ( image_fd ) {

		if ( ( image_fd->size & ( ( 1ULL << align ) - 1 ) ) != 0 ) {
			aligned_size = ((image_fd->size >> align) + 1) << align;
			image_fd->align = aligned_size - image_fd->size;
			image_fd->size = aligned_size;
		}

		total_size += image_fd->size;
		image_fd = image_fd->next;

	}

	image->size = total_size;

	image->hash = image_hash_from_data(image);

}

static struct image * image_alloc(void) {

	struct image * image = calloc(1, sizeof(struct image));
	if ( ! image )
		ALLOC_ERROR_RETURN(NULL);
	return image;

}

struct image * image_alloc_from_file(const char * file, const char * type, const char * device, const char * hwrevs, const char * version, const char * layout, struct image_part * parts) {

	return image_alloc_from_files(&file, 1, type, device, hwrevs, version, layout, parts);

}

struct image * image_alloc_from_files(const char ** files, int count, const char * type, const char * device, const char * hwrevs, const char * version, const char * layout, struct image_part * parts) {

	struct image * image;
	int i;
	int * fds = calloc(count, sizeof(int));
	if ( ! fds )
		ALLOC_ERROR_RETURN(NULL);

	for ( i = 0; i < count; ++i ) {
		fds[i] = open(files[i], O_RDONLY);
		if ( fds[i] < 0 ) {
			ERROR_INFO("Cannot open image file %s", files[i]);
			free(fds);
			return NULL;
		}
	}

	image = image_alloc_from_fds(fds, files, count, type, device, hwrevs, version, layout, parts);
	free(fds);
	return image;

}

struct image * image_alloc_from_fd(int fd, const char * orig_filename, const char * type, const char * device, const char * hwrevs, const char * version, const char * layout, struct image_part * parts) {

	return image_alloc_from_fds(&fd, &orig_filename, 1, type, device, hwrevs, version, layout, parts);

}

struct image * image_alloc_from_fds(int * fds, const char ** orig_filenames, int count, const char * type, const char * device, const char * hwrevs, const char * version, const char * layout, struct image_part * parts) {

	int i;
	struct image * image = image_alloc();
	if ( ! image ) {
		for ( i = 0; i < count; ++i )
			close(fds[i]);
		return NULL;
	}

	for ( i = 0; i < count; ++i ) {

		off_t offset;
		struct image_fd * image_fds = image->fds;
		struct image_fd * image_fd = calloc(1, sizeof(struct image_fd));
		if ( ! image_fd ) {
			image_free(image);
			return NULL;
		}

		if ( ! image_fds ) {
			image->fds = image_fd;
		} else {
			while ( image_fds->next )
				image_fds = image_fds->next;
			image_fds->next = image_fd;
		}

		image_fd->is_shared_fd = 0;
		image_fd->fd = fds[i];

		offset = lseek(image_fd->fd, 0, SEEK_END);
		if ( offset == (off_t)-1 ) {
			ERROR_INFO("Cannot seek to end of file %s", orig_filenames[i]);
			image_free(image);
			return NULL;
		}

		image_fd->size = offset;
		image_fd->offset = 0;
		image_fd->is_shared_fd = 0;
		image_fd->shared_cur = 0;
		image_fd->orig_filename = strdup(orig_filenames[i]);

		image->size += image_fd->size;

		if ( lseek(image_fd->fd, 0, SEEK_SET) == (off_t)-1 ) {
			ERROR_INFO("Cannot seek to begin of file %s", orig_filenames[i]);
			image_free(image);
			return NULL;
		}

	}

	if ( image_append(image, type, device, hwrevs, version, layout, parts) < 0 )
		return NULL;

	if ( ( ! type || ! type[0] ) && ( ! device || ! device[0] ) && ( ! hwrevs || ! hwrevs[0] ) && ( ! version || ! version[0] ) )
		image_missing_values_from_name(image, count > 0 ? orig_filenames[0] : NULL);

	image_align(image);

	return image;

}

struct image * image_alloc_from_shared_fd(int fd, size_t size, size_t offset, uint16_t hash, const char * type, const char * device, const char * hwrevs, const char * version, const char * layout, struct image_part * parts) {

	struct image * image = image_alloc();
	struct image_fd * image_fd = calloc(1, sizeof(struct image_fd));
	if ( ! image || ! image_fd ) {
		free(image);
		free(image_fd);
		return NULL;
	}

	image_fd->is_shared_fd = 1;
	image_fd->fd = fd;
	image_fd->size = size;
	image_fd->offset = offset;
	image_fd->shared_cur = 0;
	image->fds = image_fd;
	image->size = image_fd->size;

	if ( image_append(image, type, device, hwrevs, version, layout, parts) < 0 )
		return NULL;

	if ( ! noverify && image->hash != hash ) {
		ERROR("Image hash mishmash (counted %#04x, got %#04x)", image->hash, hash);
		image_free(image);
		return NULL;
	}

	image_align(image);

	return image;

}

void image_free(struct image * image) {

	if ( ! image )
		return;

	while ( image->fds ) {
		struct image_fd * next = image->fds->next;
		if ( ! image->fds->is_shared_fd )
			close(image->fds->fd);
		free(image->fds->orig_filename);
		free(image->fds);
		image->fds = next;
	}

	while ( image->devices ) {
		struct device_list * next = image->devices->next;
		free(image->devices->hwrevs);
		free(image->devices);
		image->devices = next;
	}

	while ( image->parts ) {
		struct image_part * next = image->parts->next;
		free(image->parts->name);
		free(image->parts);
		image->parts = next;
	}

	free(image->version);
	free(image->layout);

	free(image);

}

void image_seek(struct image * image, size_t whence) {

	off_t offset;
	size_t start = 0;
	struct image_fd * image_fd = image->fds;

	image->cur = whence;

	if ( whence > image->size ) {
		ERROR("Seek in image failed: Position end of the image");
		return;
	}

	while ( image_fd ) {
		if ( ( whence >= start && whence < start + image_fd->size ) || ! image_fd->next )
			break;
		start += image_fd->size;
		image_fd = image_fd->next;
	}

	if ( whence - start >= image_fd->size - image_fd->align ) {
		offset = lseek(image_fd->fd, image_fd->size - image_fd->align, SEEK_SET);
	} else {
		offset = lseek(image_fd->fd, image_fd->offset + whence - start, SEEK_SET);
	}

	if ( offset == (off_t)-1 )
		ERROR_INFO("Seek in file %s failed", (image_fd->orig_filename ? image_fd->orig_filename : "(unknown)"));
	else if ( image_fd->is_shared_fd )
		image_fd->shared_cur = offset;

}

size_t image_read(struct image * image, void * buf, size_t count) {

	ssize_t ret;
	off_t offset;
	size_t new_count = 0;
	size_t ret_count = 0;
	size_t start = 0;
	struct image_fd * image_fd = image->fds;

	while ( image_fd ) {
		if ( ( image->cur >= start && image->cur < start + image_fd->size ) || ! image_fd->next )
			break;
		start += image_fd->size;
		image_fd = image_fd->next;
	}

	if ( image_fd->is_shared_fd ) {
		if ( lseek(image_fd->fd, image_fd->shared_cur, SEEK_SET) != image_fd->shared_cur ) {
			ERROR_INFO("Cannot restore offset of file %s", (image_fd->orig_filename ? image_fd->orig_filename : "(unknown)"));
			return 0;
		}
	}

	while ( count > 0 ) {

		if ( image->cur < start + image_fd->size - image_fd->align ) {

			if ( image->cur + count > start + image_fd->size - image_fd->align )
				new_count = start + image_fd->size - image_fd->align - image->cur;
			else
				new_count = count;

			ret = read(image_fd->fd, buf, new_count);
			if ( ret <= 0 )
				break;

			count -= ret;
			buf = (unsigned char *)buf + ret;
			ret_count += ret;
			image->cur += ret;

			if ( image_fd->is_shared_fd )
				image_fd->shared_cur += ret;

			if ( (size_t)ret != new_count )
				continue;

		}

		if ( image_fd->align > 0 && count > 0 ) {

			if ( count > image_fd->align )
				new_count = image_fd->align;
			else
				new_count = count;

			memset(buf, 0xFF, new_count);
			ret = new_count;

			count -= ret;
			buf = (unsigned char *)buf + ret;
			ret_count += ret;
			image->cur += ret;

		}

		if ( image->cur == start + image_fd->size ) {

			start += image_fd->size;
			image_fd = image_fd->next;

			if ( ! image_fd )
				break;

			offset = lseek(image_fd->fd, image_fd->offset, SEEK_SET);
			if ( offset == (off_t)-1 ) {
				ERROR_INFO("Seek in file %s failed", (image_fd->orig_filename ? image_fd->orig_filename : "(unknown)"));
				break;
			} else if ( image_fd->is_shared_fd ) {
				image_fd->shared_cur = offset;
			}

		}

	}

	return ret_count;

}

void image_list_add(struct image_list ** list, struct image * image) {

	struct image_list * last = calloc(1, sizeof(struct image_list));
	if ( ! last )
		ALLOC_ERROR_RETURN();

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

	image_list_unlink(list);
	image_free(list->image);
	free(list);

}

void image_list_unlink(struct image_list * list) {

	if ( ! list )
		return;

	if ( list->prev )
		list->prev->next = list->next;

	if ( list->next )
		list->next->prev = list->prev;

	list->prev = NULL;
	list->next = NULL;

}

static uint16_t do_hash(uint16_t * b, size_t len) {

	uint16_t result = 0;

	for ( len >>= 1; len--; b = b+1 )
		result^=b[0];

	return result;

}

uint16_t image_hash_from_data(struct image * image) {

	unsigned char buf[0x20000];
	uint16_t hash = 0;
	size_t ret;

	image_seek(image, 0);
	while ( ( ret = image_read(image, buf, sizeof(buf)) ) )
		hash ^= do_hash((uint16_t *)buf, ret);

	return hash;
}

static const char * image_types[] = {
	[IMAGE_XLOADER] = "xloader",
	[IMAGE_2ND] = "2nd",
	[IMAGE_SECONDARY] = "secondary",
	[IMAGE_KERNEL] = "kernel",
	[IMAGE_INITFS] = "initfs",
	[IMAGE_ROOTFS] = "rootfs",
	[IMAGE_MMC] = "mmc",
	[IMAGE_CMT_2ND] = "cmt-2nd",
	[IMAGE_CMT_ALGO] = "cmt-algo",
	[IMAGE_CMT_MCUSW] = "cmt-mcusw",
	[IMAGE_1ST] = "1st",
	[IMAGE_CERT_SW] = "cert-sw",
	[IMAGE_APE_ALGO] = "ape-algo",
};

enum image_type image_type_from_data(struct image * image) {

	unsigned char buf[512];
	size_t size;

	memset(buf, 0, sizeof(buf));
	image_seek(image, 0);
	size = image_read(image, buf, sizeof(buf));

	if ( size >= 58 && memcmp(buf+52, "2NDAPE", 6) == 0 )
		return IMAGE_2ND;
	else if ( size >= 23 && memcmp(buf+20, "2ND", 3) == 0 )
		return IMAGE_2ND;
	else if ( size >= 8 && memcmp(buf+4, "NOLOScnd", 8) == 0 )
		return IMAGE_SECONDARY;
	else if ( size >= 28 && memcmp(buf+20, "X-LOADER", 8) == 0 )
		return IMAGE_XLOADER;
	else if ( size >= 20 && memcmp(buf+12, "NOLOXldr", 8) == 0 )
		return IMAGE_XLOADER;
	else if ( size >= 12 && memcmp(buf+4, "NOLOXldr", 8) == 0 )
		return IMAGE_2ND;
	else if ( size >= 40 && memcmp(buf+36, "\x18\x28\x6f\x01", 4) == 0 ) /* ARM Linux kernel magic number */
		return IMAGE_KERNEL;
	else if ( size >= 4 && memcmp(buf+1, "\x00\x00\xea", 3) == 0 ) /* ARM U-Boot - instruction branch */
		return IMAGE_KERNEL;
	else if ( size >= 4 && memcmp(buf, "UBI#", 4) == 0 ) /* UBI EC header */
		return IMAGE_ROOTFS;
	else if ( size >= 512 && memcmp(buf+510, "\x55\xaa", 2) == 0 ) /* FAT boot sector signature */
		return IMAGE_MMC;
	else if ( size >= 8 && memcmp(buf, "\xb0\x00\x01\x03\x9d\x00\x00\x00", 8) == 0 )
		return IMAGE_CMT_2ND;
	else if ( size >= 8 && memcmp(buf, "\xb1\x00\x00\x00\x82\x00\x00\x00", 8) == 0 )
		return IMAGE_CMT_ALGO;
	else if ( size >= 8 && memcmp(buf, "\xb2\x00\x00\x01\x44\x00\x00\x00", 8) == 0 )
		return IMAGE_CMT_MCUSW;
	else if ( size >= 4 && memcmp(buf, "\x45\x3d\xcd\x28", 4) == 0 ) /* CRAMFS MAGIC */
		return IMAGE_INITFS;
	else if ( size >= 2 && memcmp(buf, "\x85\x19", 2) == 0 ) { /* JFFS2 MAGIC */
		if ( image->size < 0x1000000 )
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

	if ( type >= sizeof(image_types)/sizeof(image_types[0]) )
		return NULL;

	return image_types[type];

}

int image_hwrev_is_valid(struct image * image, int16_t hwrev) {

	struct device_list * device_ptr = image->devices;

	while ( device_ptr ) {
		if ( hwrev_is_valid(device_ptr->hwrevs, hwrev) )
			return 1;
		device_ptr = device_ptr->next;
	}

	return 0;

}

void image_print_info(struct image * image) {

	int i = 0;
	const char * str;
	struct image_fd * image_fd = image->fds;
	struct image_part * part = image->parts;
	struct device_list * device = image->devices;

	if ( image->fds && image->fds->orig_filename ) {
		if ( image->fds->next )
			printf("Multifile image:\n");
		else
			printf("File: %s\n", image->fds->orig_filename);
	}

	str = image_type_to_string(image->type);
	printf("    Image type: %s\n", str ? str : "unknown");
	printf("    Image size: %d bytes\n", image->size);

	if ( image->version )
		printf("    Image version: %s\n", image->version);

	if ( image->layout )
		printf("    Image layout: included\n");

	while ( part ) {

		if ( image->fds && image->fds->orig_filename && image->fds->next )
			printf("File: %s\n", (image_fd && image_fd->orig_filename) ? image_fd->orig_filename : "(unknown)");

		printf("    Image part %d name: %s\n", i, part->name ? part->name : "(empty)");
		printf("    Image part %d size: %d bytes\n", i, part->size);

		part = part->next;
		image_fd = image_fd ? image_fd->next : NULL;
		++i;

	}

	while ( device ) {

		char * str2 = NULL;
		struct device_list * next = device->next;

		if ( device->device == DEVICE_UNKNOWN )
			str = "unknown";
		else
			str = device_to_string(device->device);

		if ( str )
			printf("    Valid for: device %s", str);

		if ( str && device->hwrevs )
			str2 = hwrevs_alloc_to_string(device->hwrevs);

		if ( str2 ) {
			printf(", HW revisions: %s\n", str2);
			free(str2);
		} else if ( str ) {
			printf("\n");
		}

		device = next;
	}

}
