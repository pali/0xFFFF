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

#include "device.h"
#include "image.h"
#include "fiasco2.h"

#define FIASCO_READ_ERROR(fiasco, error) do { fprintf(stderr, "Error: %s\n", error); fiasco_free(fiasco); return NULL; } while (0)
#define FIASCO_WRITE_ERROR(file, fd, error) do { fprintf(stderr, "Error: %s: %s\n", file, error); if ( fd >= 0 ) close(fd); return -1; } while (0)
#define READ_OR_FAIL(fiasco, buf, size) do { if ( read(fiasco->fd, buf, size) != size ) { fprintf(stderr, "Error: Cannot read %d bytes\n", size); fiasco_free(fiasco); return NULL; } } while (0)
#define READ_OR_RETURN(fiasco, buf, size) do { if ( read(fiasco->fd, buf, size) != size ) return fiasco; } while (0)
#define WRITE_OR_FAIL(fd, buf, size) do { if ( write(fd, buf, size) != size ) { fprintf(stderr, "Error: Cannot write %d bytes\n", size); close(fd); return -1; } } while (0)

struct fiasco * fiasco_alloc_empty(void) {

	struct fiasco * fiasco = calloc(1, sizeof(struct fiasco));
	if ( ! fiasco ) {
		perror("Cannot allocate memory");
		return NULL;
	}

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

	int v = 1;

	struct fiasco * fiasco = fiasco_alloc_empty();
	if ( ! fiasco )
		return NULL;

	fiasco->fd = open(file, O_RDONLY);
	if ( fiasco->fd < 0 ) {
		perror("Cannot open file");
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

	if ( v ) printf("Number of header blocks: %d\n", count);

	while ( count > 0 ) {
		READ_OR_FAIL(fiasco, &byte, 1);
		READ_OR_FAIL(fiasco, &length8, 1);
		READ_OR_FAIL(fiasco, buf, length8);
		if ( byte == 0xe8 ) {
			memset(fiasco->name, 0, sizeof(fiasco->name));
			strncpy(fiasco->name, (char *)buf, length8);
			if ( v ) printf("Fiasco name: %s\n", fiasco->name);
		} else if ( byte == 0x31 ) {
			memset(fiasco->swver, 0, sizeof(fiasco->swver));
			strncpy(fiasco->swver, (char *)buf, length8);
			if ( v ) printf("SW version: %s\n", fiasco->swver);
		} else {
			if ( v ) printf("Unknown header %#x\n", byte);
		}
		--count;
	}

	/* walk the tree */
	while ( 1 ) {

		/* If end of file, return fiasco image */
		READ_OR_RETURN(fiasco, buf, 7);

		/* Header of next image */
		if ( ! buf[0] == 0x54 && buf[2] == 0x2E && buf[3] == 0x19 && buf[4] == 0x01 && buf[5] == 0x01 && buf[6] == 0x00 ) {
			printf("Invalid next image header\n");
			return fiasco;
		}

		count8 = buf[1];
		if ( count8 > 0 )
			--count8;

		READ_OR_RETURN(fiasco, &hash, 2);
		hash = ntohs(hash);

		memset(type, 0, sizeof(type));
		READ_OR_RETURN(fiasco, type, 12);

//		memcpy(byte, type, 1);
		byte = type[0];
		if ( byte == 0xFF )
			return fiasco;

		if ( v ) printf(" %s\n", type);

		READ_OR_RETURN(fiasco, &length, 4);
		length = ntohl(length);

		/* unknown */
		READ_OR_RETURN(fiasco, buf, 4);

		if ( v )  {
			printf("   size:    %d bytes\n", length);
			printf("   hash:    %#04x\n", hash);
			printf("   subsections: %d\n", count8);
		}

		memset(device, 0, sizeof(device));
		memset(hwrevs, 0, sizeof(hwrevs));
		memset(version, 0, sizeof(version));
		memset(layout, 0, sizeof(layout));

		while ( count8 > 0 ) {

			READ_OR_RETURN(fiasco, &byte, 1);
			READ_OR_RETURN(fiasco, &length8, 1);
			READ_OR_RETURN(fiasco, buf, length8);

			if ( v ) {
				printf("   subinfo\n");
				printf("     length: %d\n", length8);
				printf("     type: ");
			}

			if ( byte == '1' ) {
				memset(version, 0, sizeof(version));
				strncpy(version, (char *)buf, length8);
				if ( v ) {
					printf("version string\n");
					printf("       version: %s\n", version);
				}
			} else if ( byte == '2' ) {
				int tmp = length8;
				if ( tmp > 16 ) tmp = 16;
				memset(device, 0, sizeof(device));
				strncpy(device, (char *)buf, tmp);
				if ( v ) printf("hw revision\n");
				printf("       device: %s\n", device);
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
						/* TODO: check if hwrevs has enought size */
						strcat(hwrevs, ",");
						strcat(hwrevs, hwrev);
					}
					if ( v ) printf("       hw revision: %s\n", hwrev);
					pbuf += strlen(hwrev) + 1;
				}
			} else if ( byte == '3' ) {
				memset(layout, 0, sizeof(layout));
				strncpy(layout, (char *)buf, length8);
				if ( v ) printf("layout\n");
			} else {
				if ( v ) printf("unknown ('%c':%#x)\n", byte, byte);
			}

			--count8;
		}

		/* unknown */
		READ_OR_RETURN(fiasco, buf, 1);

		offset = lseek(fiasco->fd, 0, SEEK_CUR);

		if ( v ) {
			printf("   version: %s\n", version);
			printf("   device: %s\n", device);
			printf("   hwrevs: %s\n", hwrevs);
			printf("   data at: %#08x\n", (unsigned int)offset);
		}

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
	uint32_t size;
	uint32_t length;
	uint16_t hash;
	uint8_t length8;
	const char * str;
	const char * type;
	const char * device;
	struct image_list * image_list;
	struct image * image;
	unsigned char buf[4096];

	if ( ! fiasco )
		return -1;

	if ( ! fiasco->first )
		FIASCO_WRITE_ERROR(file, fd, "Nothing to write");

	if ( fiasco->name && strlen(fiasco->name)+1 > UINT8_MAX )
		FIASCO_WRITE_ERROR(file, fd, "Fiasco name string is too long");

	if ( fiasco->swver && strlen(fiasco->swver)+1 > UINT8_MAX )
		FIASCO_WRITE_ERROR(file, fd, "SW version string is too long");

	fd = open(file, O_RDWR|O_CREAT|O_TRUNC, 0644);
	if ( fd < 0 ) {
		perror("Cannot create file");
		return -1;
	}

	printf("Writing file header\n");

	WRITE_OR_FAIL(fd, "\xb4", 1); /* signature */

	printf("Writing FW header\n");

	if ( fiasco->name[0] )
		str = fiasco->name;
	else
		str = "OSSO UART+USB";

	length = 4 + strlen(str) + 3;
	if ( fiasco->swver[0] )
		length += strlen(fiasco->swver) + 3;
	length = htonl(length);
	WRITE_OR_FAIL(fd, &length, 4); /* FW header length */

	if ( fiasco->swver[0] )
		length = htonl(2);
	else
		length = htonl(1);
	WRITE_OR_FAIL(fd, &length, 4); /* FW header blocks count */

	/* Fiasco name */
	length8 = strlen(str)+1;
	WRITE_OR_FAIL(fd, "\xe8", 1);
	WRITE_OR_FAIL(fd, &length8, 1);
	WRITE_OR_FAIL(fd, str, length8);

	/* SW version */
	if ( fiasco->swver[0] ) {
		printf("Writing SW version: %s\n", fiasco->swver);
		length8 = strlen(fiasco->swver)+1;
		WRITE_OR_FAIL(fd, "\x31", 1);
		WRITE_OR_FAIL(fd, &length8, 1);
		WRITE_OR_FAIL(fd, fiasco->swver, length8);
	};

	image_list = fiasco->first;

	while ( image_list ) {

		image = image_list->image;

		if ( ! image )
			FIASCO_WRITE_ERROR(file, fd, "Empty image");

		type = image_type_to_string(image->type);
		device = device_to_string(image->device);

		if ( ! type )
			FIASCO_WRITE_ERROR(file, fd, "Unknown image type");

		if ( image->version && strlen(image->version) > UINT8_MAX )
			FIASCO_WRITE_ERROR(file, fd, "Image version string is too long");

		if ( image->layout && strlen(image->layout) > UINT8_MAX )
			FIASCO_WRITE_ERROR(file, fd, "Image layout is too long");

		printf("Writing image:\n");
		printf("  type: %s\n", type);
		printf("  hash: %d\n", image->hash);
		printf("  size: %d\n", image->size);

		if ( device )
			printf("  device: %s\n", device);

		if ( image->hwrevs )
			printf("  hwrevs: %s\n", image->hwrevs);

		if ( image->version )
			printf("  version: %s\n", image->version);

		if ( image->layout )
			printf("  layout: included\n");

		printf("Writing image header\n");

		/* signature */
		WRITE_OR_FAIL(fd, "T", 1);

		/* number of subsections */
		length8 = 1;
		if ( image->version )
			++length8;
		if ( device )
			++length8;
		if ( image->layout )
			++length8;
		WRITE_OR_FAIL(fd, &length8, 1);

		/* unknown */
		WRITE_OR_FAIL(fd, "\x2e\x19\x01\x01\x00", 5);

		/* checksum */
		hash = htons(image->hash);
		WRITE_OR_FAIL(fd, &hash, 2);

		/* image type name */
		memset(buf, 0, 12);
		strncpy((char *)buf, type, 12);
		WRITE_OR_FAIL(fd, buf, 12);

		/* image size */
		size = htonl(image->size);
		WRITE_OR_FAIL(fd, &size, 4);

		/* unknown */
		WRITE_OR_FAIL(fd, "\x00\x00\x00\x00", 4);

		/* append version subsection */
		if ( image->version ) {
			WRITE_OR_FAIL(fd, "1", 1); /* 1 - version */
			length8 = strlen(image->version)+1;
			WRITE_OR_FAIL(fd, &length8, 1);
			WRITE_OR_FAIL(fd, image->version, length8);
		}

		/* append device & hwrevs subsection */
		if ( device ) {
			const char *ptr = image->hwrevs;
			const char *oldptr = ptr;
			int i;
			WRITE_OR_FAIL(fd, "2", 1); /* 2 - device & hwrevs */
			length8 = 16;
			if ( image->hwrevs ) {
				i = 1;
				while ( (ptr = strchr(ptr, ',')) ) { i++; ptr++; }
				if ( (int)length8 + i*8 > 255 ) {
					FIASCO_WRITE_ERROR(file, fd, "Device string and HW revisions are too long");
				}
				length8 += i*8;
			}
			WRITE_OR_FAIL(fd, &length8, 1);
			length8 = strlen(device);
			if ( length8 > 15 ) length8 = 15;
			WRITE_OR_FAIL(fd, device, length8);
			for ( i = 0; i < 16 - length8; ++i )
				WRITE_OR_FAIL(fd, "\x00", 1);
			if ( image->hwrevs ) {
				ptr = image->hwrevs;
				oldptr = ptr;
				while ( (ptr = strchr(ptr, ',')) ) {
					length8 = ptr-oldptr;
					if ( length8 > 8 ) length8 = 8;
					WRITE_OR_FAIL(fd, oldptr, length8);
					for ( i=0; i < 8 - length8; ++i )
						WRITE_OR_FAIL(fd, "\x00", 1);
					++ptr;
					oldptr = ptr;
				}
				length8 = strlen(oldptr);
				if ( length8 > 8 ) length8 = 8;
				WRITE_OR_FAIL(fd, oldptr, length8);
				for ( i = 0; i < 8 - length8; ++i )
				WRITE_OR_FAIL(fd, "\x00", 1);
			}
		}

		/* append layout subsection */
		if ( image->layout ) {
			length8 = strlen(image->layout);
			WRITE_OR_FAIL(fd, "3", 1); /* 3 - layout */
			WRITE_OR_FAIL(fd, &length8, 1);
			WRITE_OR_FAIL(fd, image->layout, length8);
		}

		/* dummy byte - end of all subsections */
		WRITE_OR_FAIL(fd, "\x00", 1);

		image_seek(image, 0);
		while ( 1 ) {
			size = image_read(image, buf, sizeof(buf));
			if ( size < 1 )
				break;
			WRITE_OR_FAIL(fd, buf, size);
		}

		image_list = image_list->next;

	}

	printf("Done\n");
	close(fd);
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
			perror("Cannot getcwd");
			return -1;
		}

		if ( chdir(dir) < 0 ) {
			perror("Cannot chdir");
			return -1;
		}

	}

	image_list = fiasco->first;

	while ( image_list ) {

		image = image_list->image;

		name = image_name_alloc_from_values(image);

		printf("Unpacking image...\n");
		image_print_info(image);

		if ( image->layout ) {

			layout_name = calloc(1, strlen(name) + strlen(".layout") + 1);
			if ( ! layout_name ) {
				perror("Alloc error");
				return -1;
			}

			sprintf(layout_name, "%s.layout", name);

			printf("    Layout file: %s\n", layout_name);

		}

		printf("    Output file: %s\n", name);

		fd = open(name, O_RDWR|O_CREAT|O_TRUNC, 0644);
		if ( fd < 0 ) {
			perror("Cannot create file");
			return -1;
		}

		free(name);

		image_seek(image, 0);
		while ( 1 ) {
			size = image_read(image, buf, sizeof(buf));
			if ( size < 1 )
				break;
			WRITE_OR_FAIL(fd, buf, size);
		}

		close(fd);

		if ( image->layout ) {

			fd = open(layout_name, O_RDWR|O_CREAT|O_TRUNC, 0644);
			if ( fd < 0 ) {
				perror("Cannot create file");
				return -1;
			}

			free(layout_name);

			WRITE_OR_FAIL(fd, image->layout, (int)strlen(image->layout));

			close(fd);

		}

		image_list = image_list->next;

		if ( image_list )
			printf("\n");

	}

	if ( dir ) {

		if ( chdir(cwd) < 0 ) {
			perror("Cannot chdir");
			return -1;
		}

	}

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
