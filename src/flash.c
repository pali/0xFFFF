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

#if HAVE_USB
#include "main.h"
#include "hash.h"
#include "hexdump.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void check_nolo_order_failed()
{
	fprintf(stderr, "\nERROR: Provide xloader before the secondary. NOLO disagrees anything else.\n");
	fprintf(stderr, "Use: -p xloader.bin -p secondary.bin\n");
	exit(1);
}

void query_error_message()
{
	/* query error message */
	int len = 0;
	char nolomsg[2048];
	memset(nolomsg, '\0', 2048);
	usb_control_msg(dev, 192, 5, 0, 0, nolomsg, 2048, 2000);
	nolomsg[2047] = '\0';
	printf("\nNOLO says:\n");
	if (nolomsg[0] == '\0') {
		printf(" (.. silence ..)\n");
	} else {
		dump_bytes((unsigned char *)nolomsg, 128);
		do {
			printf(" - %s\n", nolomsg+len);
			len+=strlen(nolomsg+len)+1;
		} while(nolomsg[len]!='\0');
	}
}

void check_nolo_order()
{
	int i, xlo = 0;

	for(i=0;i<pcs_n;i++) {
		if (!strcmp(pcs[i].type, "xloader")) {
			if (!xlo && ++i<pcs_n) {
			printf("CHK%s\n", pcs[i].type);
				xlo++;
				if (!strcmp(pcs[i].type, "secondary")) {
					xlo = 0x11;
					break;
				} else check_nolo_order_failed();
			} check_nolo_order_failed();
		} else
		if (!strcmp(pcs[i].type, "secondary"))
			check_nolo_order_failed();
	}

	if (xlo && xlo != 0x11)
		check_nolo_order_failed();
}

void flash_image(const char *filename, const char *piece, const char *device, const char *hwrevs, const char *version)
{
	FILE *fd;
	int vlen = 0;
	int request;
	/*/ n800 flash queries have a variable size */
	unsigned char fquery[256]; /* flash query */
	unsigned long long size, off;
	unsigned char bsize[4], tmp;
	unsigned char nolofiller[128];
	ushort hash = do_hash_file(filename);

	if (piece == NULL) {
		//exit(1);
		piece = fpid_file(filename);
		if (piece == NULL) {
			printf("Unknown piece type\n");
			return;
		}
		printf("Piece type: %s\n", piece);
	}

	if (piece != NULL) {
		if (!strcmp(piece, "fiasco")) {
			fiasco_flash(filename);
			return;
		}
	}

	if (version)
		vlen = strlen(version)+1;

	fd = fopen(filename, "rb");
	if (fd == NULL) {
		printf("Cannot open file\n");
		exit(1);
	}
	/* cook flash query */
	memset(fquery, '\x00', 27);              // reset buffer
	memcpy(fquery, "\x2e\x19\x01\x01", 4);   // header
	//memcpy(fquery+5, "\xbf\x6b", 2); // some magic (modified crc16???)
	memcpy(fquery+5, &hash, 2);
	tmp = fquery[5]; fquery[5] = fquery[6]; fquery[6] = tmp;
	memcpy(fquery+7, piece, strlen(piece));  // XXX ??!??

	printf("| hash: 0x%hhx%hhx ", fquery[5], fquery[6]);
	size = get_file_size(filename);
	bsize[0] = (size & 0xff000000) >> 24;
	bsize[1] = (size & 0x00ff0000) >> 16;
	bsize[2] = (size & 0x0000ff00) >> 8;
	bsize[3] = (size & 0x000000ff);
	printf("size: %lld (%02x %02x %02x %02x)\n",
		size, bsize[0], bsize[1], bsize[2], bsize[3]);
	memcpy(fquery+0x13, &bsize, 4);
	if (vlen) memcpy(fquery+27, version, vlen);
	
	if (!strcmp(piece, "rootfs"))
		request = 85;
	else    request = 68;

	//dump_bytes(fquery, 27+vlen);
	if (usb_control_msg(dev, CMD_WRITE, request, 0, 0, (char *)fquery, 27+vlen, 2000) <0) {
		query_error_message();
		perror("flash_image.header");
		exit(1);
	}

	/*/ cook and bulk nollo filler */
	memset(&nolofiller, '\xff', 128);
	memcpy(nolofiller+0x00, "NOLO filler", 11);
	memcpy(nolofiller+0x40, "NOLO filler", 11);
	usb_bulk_write(dev, 2, (char *)nolofiller, 128, 5000);
	usb_bulk_write(dev, 2, (char *)nolofiller, 0,   5000);

	/*/ bulk write image here */
	printf("[=] Bulkwriting the %s piece...\n", piece);
	fflush(stdout);

	for(off = 0; off<size; off += BSIZE) {
		char buf[BSIZE];
		int bread, bsize = size-off;
		if (bsize>BSIZE) bsize = BSIZE;
		bread = fread(buf, bsize, 1, fd);
		if (bread != 1)
			printf("WARNING: Oops wrong read %d vs %d \n", bread, bsize);
		bread = usb_bulk_write(dev, 2, buf, bsize, 5000);
		if (bread == 64) {
			query_error_message();
			fclose(fd);
			return;
		}
		progressbar(off, size);
		if (bread<0) {
			printf("\n");
			perror(" -ee- ");
			fclose(fd);
			return;
		}
		fflush(stdout);
	}
	fclose(fd);
	/*/ EOF */
	usb_bulk_write(dev, 2, (char *)nolofiller, 0, 1000);
	progressbar(1, 1);
	printf("\n");

	// index = 4????
	if (!strcmp(piece, "rootfs")) {
		if (usb_control_msg(dev, CMD_WRITE, 82, 0, 0, (char *)fquery, 0, 30000)<0) {
			fprintf(stderr, "Oops. Invalid checksum?\n");
			exit(1);
		}
	} else {
		int t = 0;
		if (!strcmp(piece, "secondary"))
			t = 1;
		else
		if (!strcmp(piece, "kernel"))
			t = 3;
		else
		if (!strcmp(piece, "initfs"))
			t = 4;
		if (!strcmp(piece, "xloader"))
			printf("xloader flashed not commiting until secondary arrives...\n");
		else
		if (usb_control_msg(dev, CMD_WRITE, 80, 0, t, (char *)fquery, 0, 10000)<0) {
			fprintf(stderr, "Oops. Invalid checksum?\n");
			exit(1);
		}
	}

	// unknown query !! :""
	if (usb_control_msg(dev, CMD_WRITE, 67, 0, 0, (char *)fquery, 0, 2000)<0) {
		fprintf(stderr, "Oops, the flash was denied or so :/\n");
		exit(1);
	}
	printf("Flash done succesfully.\n");
}
#endif
