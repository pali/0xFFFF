/*
 * Copyright (C) 2007
 *       pancake <pancake@youterm.com>
 *
 * 0xFFFF is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0xFFFF is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0xFFFF; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "main.h"
#include <stdio.h>
#include <string.h>

char *fpid_file(char *filename)
{
	FILE *fd;
	char buf[512];
	unsigned char *b = (unsigned char *)&buf;
	char *piece = NULL;
	long size;

	// 2nd      : +0x34 = 2NDAPE
	// secondary: +0x04 = NOLOScnd
	// x-loader : +0x14 = X-LOADER
	// xloader8 : +0x0c = NOLOXldr
	// kernel   : +0x00 = 0000 a0e1 0000 a0e1
	// initfs   : <2M...be sure with 3M 0x300000

	fd = fopen(filename, "r");
	if (fd == NULL) {
		printf("Cannot open file '%s'\n", filename);
		return NULL;
	}
	fread(buf, 512, 1, fd);
	fseek(fd, 0, SEEK_END);
	size = ftell(fd);
	fclose(fd);

#if 0
	if (!memcmp(b+0x34, "2NDAPE", 6))
		return PIECE_2ND;
	else
#endif
	if (!memcmp(b+0x04, "NOLOScnd", 8))
		return pieces[PIECE_SECONDARY];
	else
	if (!memcmp(b+0x14, "X-LOADER", 8))
		return pieces[PIECE_XLOADER];
	else
	if (!memcmp(b+0x0c, "NOLOXldr", 8)) // TODO: this is xloader800, not valid on 770?
		return pieces[PIECE_XLOADER];
	else
	if (!memcmp(b+0x00, "\x00\x00\xa0\xe1\x00\x00\xa0\xe1", 8))
		return pieces[PIECE_KERNEL];
	else
	if (!memcmp(b+0x00, "\x85\x19\x01\xe0", 4)) {
		/*/ is jffs2 */
		if (size < 0x300000)
			return pieces[PIECE_INITFS];
		return pieces[PIECE_ROOTFS];
	}

	return piece;
}
