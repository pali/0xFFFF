
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
			strncpy(fiasco->name, (char *)buf, length8);
			fiasco->name[length8] = 0;
			if ( v ) printf("Fiasco name: %s\n", fiasco->name);
		} else if ( byte == 0x31 ) {
			strncpy(fiasco->swver, (char *)buf, length8);
			fiasco->name[length8] = 0;
			if ( v ) printf("SW version: %s\n", fiasco->swver);
		} else {
			if ( v ) printf("Unknown header 0x%x\n", byte);
		}
		--count;
	}

	/* walk the tree */
	while ( 1 ) {

		length = 0;
		while ( 1 ) {
			/* If end of file, return fiasco image */
			READ_OR_RETURN(fiasco, buf, 7);
			/* Header of next image */
			if ( buf[0] == 0x54 && buf[2] == 0x2E && buf[3] == 0x19 && buf[4] == 0x01 && buf[5] == 0x01 && buf[6] == 0x00 )
				break;
			/* Return back and try again */
			lseek(fiasco->fd, -6, SEEK_CUR);
			++length;
		}
		if ( length && v ) printf("Skipping %d padding bytes\n", length);

		count8 = buf[1];
		if ( count8 > 0 )
			--count8;

		READ_OR_RETURN(fiasco, &hash, 2);

		memset(type, 0, sizeof(type));
		READ_OR_RETURN(fiasco, type, 12);

		if ( type[0] == (char)0xFF )
			return fiasco;

		if ( v ) printf(" %s\n", type);

		READ_OR_RETURN(fiasco, &length, 4);
		length = ntohl(length);

		if ( v )  {
			printf("   size:    %d bytes\n", length);
			printf("   hash:    %04x\n", hash);
			printf("   subsections: %d\n", count8);
		}

		memset(hwrevs, 0, sizeof(hwrevs));

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
					while ( *pbuf < 32 && pbuf < buf + length8 ) ++pbuf;
					if ( pbuf >= buf+length8 ) break;
					tmp = buf + length8 - pbuf;
					if ( tmp > 8 ) tmp = 8;
					memset(hwrev, 0, sizeof(hwrev));
					strncpy(hwrev, (char *)pbuf, tmp);
					if ( ! hwrevs[0] )
						strcpy(hwrevs, hwrev);
					else {
						strcat(hwrevs, ",");
						strcat(hwrevs, hwrev);
					}
					if ( v ) printf("       hw revision: %s\n", hwrev);
				}
			} else if ( byte == '3' ) {
				memset(layout, 0, sizeof(layout));
				strncpy(layout, (char *)buf, length8);
				if ( v ) printf("layout\n");
			} else {
				if ( v ) printf("unknown (%c:0x%x)\n", byte, byte);
			}

			--count8;
		}

		offset = lseek(fiasco->fd, 0, SEEK_CUR);

		if ( v ) {
			printf("   version: %s\n", version);
			printf("   device: %s\n", device);
			printf("   hwrevs: %s\n", hwrevs);
			printf("   data at:   0x%08x\n", (unsigned int)offset);
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

	free(fiasco->name);
	free(fiasco->swver);
	free(fiasco);

}

void fiasco_add_image(struct fiasco * fiasco, struct image * image) {

//	if ( fiasco->fd >= 0 ) {
//		fprintf(stderr, "Fiasco image is on disk\n");
//		return 1;
//	}

	if ( ! fiasco->first ) {
		fiasco->first = calloc(1, sizeof(struct image_list));
		fiasco->first->image = image;
	} else {
		image_list_add(&fiasco->first, image);
	}

}

int fiasco_write_to_file(struct fiasco * fiasco, const char * file) {

	int fd = -1;
	uint32_t size;
	uint32_t length;
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

	if ( fiasco->name )
		str = fiasco->name;
	else
		str = "OSSO UART+USB";

	length = 4 + strlen(str) + 3;
	if ( fiasco->swver )
		length += strlen(fiasco->swver) + 3;
	length = htonl(length);
	WRITE_OR_FAIL(fd, &length, 4); /* FW header length */

	if ( fiasco->swver )
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
	if ( fiasco->swver ) {
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
		WRITE_OR_FAIL(fd, &image->hash, 2);

		/* image type name */
		WRITE_OR_FAIL(fd, type, 12);

		/* image size */
		size = image->size;

		/* Align mmc image size */
		if ( image->type == IMAGE_MMC )
			size = ((size >> 8) + 1) << 8;
		/* Align kernel image size */
		else if ( image->type == IMAGE_KERNEL )
			size = ((size >> 7) + 1) << 7;

		/* TODO: update hash after align */

		size = htonl(size);
		WRITE_OR_FAIL(fd, &size, 4);

		/* unknown */
		WRITE_OR_FAIL(fd, "\x00\x00\x00\x00", 4);

		/* append version subsection */
		if ( image->version ) {
			WRITE_OR_FAIL(fd, "1", 1); /* 1 - version */
			length8 = strlen(image->version)+1;
			WRITE_OR_FAIL(fd, &length, 1);
			WRITE_OR_FAIL(fd, image->version, length);
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

		size = image->size;

		if ( image->type == IMAGE_MMC )
			size = ((image->size >> 8) + 1) << 8;
		else if ( image->type == IMAGE_KERNEL )
			size = ((image->size >> 7) + 1) << 7;

		/* Align image if needed (fill with 0xff) */
		while ( size > image->size ) {
			WRITE_OR_FAIL(fd, "\xff", 1);
			--size;
		}

		image_list = image_list->next;

	}

	printf("Done\n");
	close(fd);
	return 0;

}

int fiasco_unpack(struct fiasco * fiasco, const char * dir) {

	/* TODO: Unpack fiasco image to dir */

	size_t length;
	char * name;
	const char * type;
	const char * device;
	struct image * image;
	struct image_list * image_list;

	if ( dir && chdir(dir) < 0 ) {
		perror("Cannot chdir");
		return -1;
	}

	image_list = fiasco->first;

	while ( image_list ) {

		image = image_list->image;

		length = 0;
		type = image_type_to_string(image->type);
		device = device_to_string(image->device);

		name = calloc(1, length);

		/* TODO: Unpack image */

		image_list = image_list->next;

	}

	return 0;

}
