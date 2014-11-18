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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

#include "global.h"

#include "device.h"
#include "image.h"
#include "fiasco.h"

#define FIASCO_READ_ERROR(fiasco, ...) do { ERROR_INFO(__VA_ARGS__); fiasco_free(fiasco); return NULL; } while (0)
#define FIASCO_WRITE_ERROR(file, fd, ...) do { ERROR_INFO_STR(file, __VA_ARGS__); if ( fd >= 0 ) close(fd); return -1; } while (0)
#define READ_OR_FAIL(fiasco, buf, size) do { if ( read(fiasco->fd, buf, size) != size ) { FIASCO_READ_ERROR(fiasco, "Cannot read %d bytes", size); } } while (0)
#define READ_OR_RETURN(fiasco, buf, size) do { if ( read(fiasco->fd, buf, size) != size ) return fiasco; } while (0)
#define WRITE_OR_FAIL(file, fd, buf, size) do { if ( ! simulate ) { if ( write(fd, buf, size) != (ssize_t)size ) { FIASCO_WRITE_ERROR(file, fd, "Cannot write %d bytes", size); } } } while (0)

struct fiasco * fiasco_alloc_empty(void) {

	struct fiasco * fiasco = calloc(1, sizeof(struct fiasco));
	if ( ! fiasco )
		ALLOC_ERROR_RETURN(NULL);

	fiasco->fd = -1;
	return fiasco;

}

struct fiasco * fiasco_alloc_from_file(const char * file) {

	uint8_t byte;
	uint32_t length;
	uint32_t count;
	uint8_t length8;
	uint8_t count8;

	char type[13];
	char device[17];
	char hwrevs[1024];
	char version[257];
	char layout[257];
	uint16_t hash;
	off_t offset;
	struct image * image;

	char hwrev[9];
	unsigned char buf[512];
	unsigned char *pbuf;

	struct fiasco * fiasco = fiasco_alloc_empty();
	if ( ! fiasco )
		return NULL;

	fiasco->fd = open(file, O_RDONLY);
	if ( fiasco->fd < 0 ) {
		ERROR_INFO("Cannot open file");
		fiasco_free(fiasco);
		return NULL;
	}

	fiasco->orig_filename = strdup(file);

	READ_OR_FAIL(fiasco, &byte, 1);
	if ( byte != 0xb4 )
		FIASCO_READ_ERROR(fiasco, "Invalid fiasco signature");

	READ_OR_FAIL(fiasco, &length, 4);
	length = ntohl(length);

	READ_OR_FAIL(fiasco, &count, 4);
	count = ntohl(count);

	VERBOSE("Number of header blocks: %d\n", count);

	while ( count > 0 ) {
		READ_OR_FAIL(fiasco, &byte, 1);
		READ_OR_FAIL(fiasco, &length8, 1);
		READ_OR_FAIL(fiasco, buf, length8);
		if ( byte == 0xe8 ) {
			memset(fiasco->name, 0, sizeof(fiasco->name));
			strncpy(fiasco->name, (char *)buf, length8);
			VERBOSE("Fiasco name: %s\n", fiasco->name);
		} else if ( byte == 0x31 ) {
			memset(fiasco->swver, 0, sizeof(fiasco->swver));
			strncpy(fiasco->swver, (char *)buf, length8);
			VERBOSE("SW version: %s\n", fiasco->swver);
		} else {
			VERBOSE("Unknown header %#x\n", byte);
		}
		--count;
	}

	/* walk the tree */
	while ( 1 ) {

		/* If end of file, return fiasco image */
		READ_OR_RETURN(fiasco, buf, 7);

		/* Header of next image */
		if ( ! buf[0] == 0x54 && buf[2] == 0x2E && buf[3] == 0x19 && buf[4] == 0x01 && buf[5] == 0x01 && buf[6] == 0x00 ) {
			ERROR("Invalid next image header");
			return fiasco;
		}

		count8 = buf[1];
		if ( count8 > 0 )
			--count8;

		READ_OR_RETURN(fiasco, &hash, 2);
		hash = ntohs(hash);

		memset(type, 0, sizeof(type));
		READ_OR_RETURN(fiasco, type, 12);

		byte = type[0];
		if ( byte == 0xFF )
			return fiasco;

		VERBOSE(" %s\n", type);

		READ_OR_RETURN(fiasco, &length, 4);
		length = ntohl(length);

		/* unknown */
		READ_OR_RETURN(fiasco, buf, 4);

		VERBOSE("   size:    %d bytes\n", length);
		VERBOSE("   hash:    %#04x\n", hash);
		VERBOSE("   subsections: %d\n", count8);

		memset(device, 0, sizeof(device));
		memset(hwrevs, 0, sizeof(hwrevs));
		memset(version, 0, sizeof(version));
		memset(layout, 0, sizeof(layout));

		while ( count8 > 0 ) {

			READ_OR_RETURN(fiasco, &byte, 1);
			READ_OR_RETURN(fiasco, &length8, 1);
			READ_OR_RETURN(fiasco, buf, length8);

			VERBOSE("   subinfo\n");
			VERBOSE("     length: %d\n", length8);
			VERBOSE("     type: ");

			if ( byte == '1' ) {
				memset(version, 0, sizeof(version));
				strncpy(version, (char *)buf, length8);
				VERBOSE("version string\n");
				VERBOSE("       version: %s\n", version);
			} else if ( byte == '2' ) {
				int tmp = length8;
				if ( tmp > 16 ) tmp = 16;
				memset(device, 0, sizeof(device));
				strncpy(device, (char *)buf, tmp);
				VERBOSE("hw revision\n");
				VERBOSE("       device: %s\n", device);
				pbuf = buf + strlen(device) + 1;
				while ( pbuf < buf + length8 ) {
					while ( pbuf < buf + length8 && *pbuf < 32 )
						++pbuf;
					if ( pbuf >= buf + length8 ) break;
					tmp = buf + length8 - pbuf;
					if ( tmp > 8 ) tmp = 8;
					memset(hwrev, 0, sizeof(hwrev));
					strncpy(hwrev, (char *)pbuf, tmp);
					if ( ! hwrevs[0] )
						strcpy(hwrevs, hwrev);
					else {
						size_t len1 = strlen(hwrevs);
						size_t len2 = strlen(hwrev);
						if ( len1 + len2 + 2 < sizeof(hwrevs) ) {
							hwrevs[len1] = ',';
							memcpy(hwrevs+len1+1, hwrev, len2+1);
						}
					}
					VERBOSE("       hw revision: %s\n", hwrev);
					pbuf += strlen(hwrev) + 1;
				}
			} else if ( byte == '3' ) {
				memset(layout, 0, sizeof(layout));
				strncpy(layout, (char *)buf, length8);
				VERBOSE("layout\n");
			} else {
				VERBOSE("unknown ('%c':%#x)\n", byte, byte);
			}

			--count8;
		}

		/* unknown */
		READ_OR_RETURN(fiasco, buf, 1);

		offset = lseek(fiasco->fd, 0, SEEK_CUR);

		VERBOSE("   version: %s\n", version);
		VERBOSE("   device: %s\n", device);
		VERBOSE("   hwrevs: %s\n", hwrevs);
		VERBOSE("   data at: %#08x\n", (unsigned int)offset);

		image = image_alloc_from_shared_fd(fiasco->fd, length, offset, hash, type, device, hwrevs, version, layout);

		if ( ! image )
			FIASCO_READ_ERROR(fiasco, "Cannot allocate image");

		fiasco_add_image(fiasco, image);

		lseek(fiasco->fd, offset+length, SEEK_SET);

	}

	return fiasco;

}

void fiasco_free(struct fiasco * fiasco) {

	struct image_list * list = fiasco->first;

	while ( list ) {
		struct image_list * next = list->next;
		image_list_del(list);
		list = next;
	}

	if ( fiasco->fd >= 0 )
		close(fiasco->fd);

	free(fiasco->orig_filename);

	free(fiasco);

}

void fiasco_add_image(struct fiasco * fiasco, struct image * image) {

	image_list_add(&fiasco->first, image);

}

int fiasco_write_to_file(struct fiasco * fiasco, const char * file) {

	int fd = -1;
	int i;
	int device_count;
	uint32_t size;
	uint32_t length;
	uint16_t hash;
	uint8_t length8;
	char ** device_hwrevs_bufs;
	const char * str;
	const char * type;
	struct image_list * image_list;
	struct image * image;
	unsigned char buf[4096];

	if ( ! fiasco )
		return -1;

	printf("Generating Fiasco image %s...\n", file);

	if ( ! fiasco->first )
		FIASCO_WRITE_ERROR(file, fd, "Nothing to write");

	if ( fiasco->name && strlen(fiasco->name)+1 > UINT8_MAX )
		FIASCO_WRITE_ERROR(file, fd, "Fiasco name string is too long");

	if ( fiasco->swver && strlen(fiasco->swver)+1 > UINT8_MAX )
		FIASCO_WRITE_ERROR(file, fd, "SW version string is too long");

	if ( ! simulate ) {
		fd = open(file, O_RDWR|O_CREAT|O_TRUNC, 0644);
		if ( fd < 0 ) {
			ERROR_INFO("Cannot create file");
			return -1;
		}
	}

	printf("Writing Fiasco header...\n");

	WRITE_OR_FAIL(file, fd, "\xb4", 1); /* signature */

	if ( fiasco->name[0] )
		str = fiasco->name;
	else
		str = "OSSO UART+USB";

	length = 4 + strlen(str) + 3;
	if ( fiasco->swver[0] )
		length += strlen(fiasco->swver) + 3;
	length = htonl(length);
	WRITE_OR_FAIL(file, fd, &length, 4); /* FW header length */

	if ( fiasco->swver[0] )
		length = htonl(2);
	else
		length = htonl(1);
	WRITE_OR_FAIL(file, fd, &length, 4); /* FW header blocks count */

	/* Fiasco name */
	length8 = strlen(str)+1;
	WRITE_OR_FAIL(file, fd, "\xe8", 1);
	WRITE_OR_FAIL(file, fd, &length8, 1);
	WRITE_OR_FAIL(file, fd, str, length8);

	/* SW version */
	if ( fiasco->swver[0] ) {
		printf("Writing SW version: %s\n", fiasco->swver);
		length8 = strlen(fiasco->swver)+1;
		WRITE_OR_FAIL(file, fd, "\x31", 1);
		WRITE_OR_FAIL(file, fd, &length8, 1);
		WRITE_OR_FAIL(file, fd, fiasco->swver, length8);
	};

	printf("\n");

	image_list = fiasco->first;

	while ( image_list ) {

		image = image_list->image;

		if ( ! image )
			FIASCO_WRITE_ERROR(file, fd, "Empty image");

		printf("Writing image...\n");
		image_print_info(image);

		type = image_type_to_string(image->type);

		device_hwrevs_bufs = device_list_alloc_to_bufs(image->devices);

		device_count = 0;
		if ( device_hwrevs_bufs && device_hwrevs_bufs[0] )
			for ( ; device_hwrevs_bufs[device_count]; ++device_count );

		if ( ! type )
			FIASCO_WRITE_ERROR(file, fd, "Unknown image type");

		if ( image->version && strlen(image->version) > UINT8_MAX )
			FIASCO_WRITE_ERROR(file, fd, "Image version string is too long");

		if ( image->layout && strlen(image->layout) > UINT8_MAX )
			FIASCO_WRITE_ERROR(file, fd, "Image layout is too long");

		printf("Writing image header...\n");

		/* signature */
		WRITE_OR_FAIL(file, fd, "T", 1);

		/* number of subsections */
		length8 = device_count+1;
		if ( image->version )
			++length8;
		if ( image->layout )
			++length8;
		WRITE_OR_FAIL(file, fd, &length8, 1);

		/* unknown */
		WRITE_OR_FAIL(file, fd, "\x2e\x19\x01\x01\x00", 5);

		/* checksum */
		hash = htons(image->hash);
		WRITE_OR_FAIL(file, fd, &hash, 2);

		/* image type name */
		memset(buf, 0, 12);
		strncpy((char *)buf, type, 12);
		WRITE_OR_FAIL(file, fd, buf, 12);

		/* image size */
		size = htonl(image->size);
		WRITE_OR_FAIL(file, fd, &size, 4);

		/* unknown */
		WRITE_OR_FAIL(file, fd, "\x00\x00\x00\x00", 4);

		/* append version subsection */
		if ( image->version ) {
			WRITE_OR_FAIL(file, fd, "1", 1); /* 1 - version */
			length8 = strlen(image->version)+1;
			WRITE_OR_FAIL(file, fd, &length8, 1);
			WRITE_OR_FAIL(file, fd, image->version, length8);
		}

		/* append device & hwrevs subsection */
		for ( i = 0; i < device_count; ++i ) {
			WRITE_OR_FAIL(file, fd, "2", 1); /* 2 - device & hwrevs */
			WRITE_OR_FAIL(file, fd, &device_hwrevs_bufs[i][0], 1);
			WRITE_OR_FAIL(file, fd, device_hwrevs_bufs[i]+1, ((uint8_t *)(device_hwrevs_bufs[i]))[0]);
			/* FIXME: memory leak: device_hwrevs_bufs */
		}
		free(device_hwrevs_bufs);

		/* append layout subsection */
		if ( image->layout ) {
			length8 = strlen(image->layout);
			WRITE_OR_FAIL(file, fd, "3", 1); /* 3 - layout */
			WRITE_OR_FAIL(file, fd, &length8, 1);
			WRITE_OR_FAIL(file, fd, image->layout, length8);
		}

		/* dummy byte - end of all subsections */
		WRITE_OR_FAIL(file, fd, "\x00", 1);

		printf("Writing image data...\n");

		image_seek(image, 0);
		while ( 1 ) {
			size = image_read(image, buf, sizeof(buf));
			if ( size == 0 )
				break;
			WRITE_OR_FAIL(file, fd, buf, size);
		}

		image_list = image_list->next;

		if ( image_list )
			printf("\n");

	}

	close(fd);
	printf("\nDone\n\n");
	return 0;

}

int fiasco_unpack(struct fiasco * fiasco, const char * dir) {

	int fd;
	char * name;
	char * layout_name;
	struct image * image;
	struct image_list * image_list;
	uint32_t size;
	char cwd[256];
	unsigned char buf[4096];

	if ( dir ) {

		memset(cwd, 0, sizeof(cwd));

		if ( ! getcwd(cwd, sizeof(cwd)) ) {
			ERROR_INFO("Cannot store current directory");
			return -1;
		}

		if ( chdir(dir) < 0 ) {
			ERROR_INFO("Cannot change current directory to %s", dir);
			return -1;
		}

	}

	fiasco_print_info(fiasco);

	image_list = fiasco->first;

	while ( image_list ) {

		fd = -1;
		name = NULL;
		layout_name = NULL;

		image = image_list->image;

		name = image_name_alloc_from_values(image);
		if ( ! name )
			return -1;

		printf("\n");
		printf("Unpacking image...\n");
		image_print_info(image);

		if ( image->layout ) {

			layout_name = calloc(1, strlen(name) + sizeof(".layout")-1 + 1);
			if ( ! layout_name ) {
				free(name);
				ALLOC_ERROR_RETURN(-1);
			}

			sprintf(layout_name, "%s.layout", name);

			printf("    Layout file: %s\n", layout_name);

		}

		printf("    Output file: %s\n", name);

		if ( ! simulate ) {
			fd = open(name, O_RDWR|O_CREAT|O_TRUNC, 0644);
			if ( fd < 0 ) {
				ERROR_INFO("Cannot create output file %s", name);
				free(name);
				free(layout_name);
				return -1;
			}
		}

		image_seek(image, 0);
		while ( 1 ) {
			size = image_read(image, buf, sizeof(buf));
			if ( size == 0 )
				break;
			if ( ! simulate ) {
				if ( write(fd, buf, size) != (ssize_t)size ) {
					ERROR_INFO_STR(name, "Cannot write %d bytes", size);
					close(fd);
					free(name);
					free(layout_name);
					return -1;
				}
			}
		}

		free(name);

		if ( ! simulate )
			close(fd);

		if ( image->layout ) {

			if ( ! simulate ) {
				fd = open(layout_name, O_RDWR|O_CREAT|O_TRUNC, 0644);
				if ( fd < 0 ) {
					ERROR_INFO("Cannot create layout file %s", layout_name);
					free(layout_name);
					return -1;
				}

				size = strlen(image->layout);

				if ( write(fd, image->layout, size) != (ssize_t)size ) {
					ERROR_INFO_STR(layout_name, "Cannot write %d bytes", size);
					close(fd);
					free(layout_name);
					return -1;
				}
			}

			free(layout_name);

			if ( ! simulate )
				close(fd);

		}

		image_list = image_list->next;

	}

	if ( dir ) {
		if ( chdir(cwd) < 0 ) {
			ERROR_INFO("Cannot change current directory back to %s", cwd);
			return -1;
		}
	}

	printf("\nDone\n\n");
	return 0;

}

void fiasco_print_info(struct fiasco * fiasco) {

	if ( fiasco->orig_filename )
		printf("File: %s\n", fiasco->orig_filename);

	if ( fiasco->name[0] )
		printf("    Fiasco Name: %s\n", fiasco->name);

	if ( fiasco->swver[0] )
		printf("    Fiasco Software release version: %s\n", fiasco->swver);

}
