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

/* global structs */
char *pieces[] = {
	"xloader",    // xloader.bin
	"2nd",        // 2nd
	"secondary",  // secondary.bin
	"kernel",     // zImage
	"initfs",     // jffs'd initfs
	"rootfs",     // 80mB of blob
	"omap-nand",  // 8kB of food for the nand
	"fiasco",     // FIASCO IMAGE
	NULL
};

long fpid_size(const char *filename)
{
	long sz;
	FILE *fd = fopen(filename, "r");
	fseek(fd, 0, SEEK_END);
	sz = ftell(fd);
	fclose(fd);
	return sz;
}

const char *fpid_file(const char *filename)
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

	if (!memcmp(b, "\xb4", 1))
		return pieces[PIECE_FIASCO];
	else
	if (!memcmp(b+0x34, "2NDAPE", 6))
		return pieces[PIECE_2ND];
	else
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
	// JFFS2 MAGIC
	if (!memcmp(b+0x00, "\x85\x19", 2)) { //\x01\xe0", 4)) {
		/*/ is jffs2 */
		if (size < 0x300000)
			return pieces[PIECE_INITFS];
		return pieces[PIECE_ROOTFS];
	}

	return piece;
}
