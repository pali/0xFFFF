/*
 *  0xFFFF - Open Free Fiasco Firmware Flasher
 *  Copyright (C) 2007  pancake <pancake@youterm.com>
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

#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#warning The FIASCO format is not yet fully implemented.

off_t piece_header_to_size(unsigned char *header)
{
	off_t sz = 0;
	sz += header[0x15+3];
	sz += header[0x15+2] << 8;
	sz += header[0x15+1] << 16;
	sz += header[0x15+0] << 24;
	return sz;
}

int piece_header_is_valid(unsigned char *header)
{
	if (header[0]=='T' && header[1]=='\x02')
		return 1;
	return 0;
}

#define piece_header_to_name(x) x+0x9;

/*
 * Teh Fiasco firmware parser lives here!
 */
int fiasco_read_image(char *file)
{
	int fd;
	unsigned char header[5];
	unsigned char version[55];
	char *version2;

	fd = open(file, O_RDONLY);

	printf("WARNING: Fiasco firmware is not yet fully supported. Don't relay on it ATM.\n");

	if (fd == -1) {
		printf("Cannot open fiasco image.\n");
		return -1;
	}

	// b4 00 00 00 00 36
	if (read(fd, header, 5) != 5) {
		printf("Error reading fiasco header\n");
		goto __fiasco_read_image_end;
	}

	if (header[0] != 0xb4) {
		printf("Invalid fiasco signature.\n");
		goto __fiasco_read_image_end;
	}
	// skip 6 bytes
	read(fd, version, 6);

	// print version header
	if (read(fd, version, 55) != 55) {
		printf("Error reading fiasco version.\n");
		goto __fiasco_read_image_end;
	}

	printf("# Fiasco header information:\n");
	printf("# ==========================\n");
	printf("# firmware type:    %s\n", version);
	version2 = version+strlen((char *)version)+3;
	if (*version2=='.')
		printf("firmware version: (null) (old firmware? no version string found?)\n");
	else printf("firmware version: %s\n", version2);

	// pieces:
	// after the version2 string starts the header-body loop love
	lseek(fd, 0xf + strlen(version2) + strlen(version), SEEK_SET);
	do {
		char piece_header[30];
		char description[256];
		unsigned char desc_len;
		char *name;
		off_t size;
		off_t tmp = lseek(fd, 0, SEEK_CUR);

		size = read(fd, piece_header, 30);
		if (size != 30) {
			fprintf(stderr, "Unexpected end of file\n");
			break;
		}
		dump_bytes(piece_header,30);
		if (!piece_header_is_valid(piece_header)) {
			fprintf(stderr, "Oops. Invalid piece header.\n");
			break;
		}
		size = piece_header_to_size(piece_header);
		name = piece_header_to_name(piece_header);
		printf("# %s is %lld bytes\n", name, size);

		read(fd, description, 255);
		printf("#   version: %s\n", description);
		/* lseek foreach subimage */
		lseek(fd, tmp, SEEK_SET);
		lseek(fd, strlen(description) + 0x20, SEEK_CUR);
		printf("rsc '%s' %lld '%s.bin' 0 %lld\n", file, lseek(fd,0,SEEK_CUR), name, size);
		lseek(fd, size, SEEK_CUR);
	} while(1);

	//printf("Image '%s', size %d bytes.\n", image, size);

__fiasco_read_image_end:
	close(fd);
	return 0;
}
