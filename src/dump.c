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
#include <usb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int rf_extract(char *dev, off_t from, off_t to, char *file)
{
	off_t i, blk = 0xfffff;
	char buf[0xfffff];
	FILE *fs = fopen(dev, "rb");
	FILE *fd = fopen(file, "wb+");

	if (from>to) {
		printf("Bad range %lld - %lld for %s\n", from, to, file);
		goto __rf_extract_exit;
	}
	if (fs == NULL) { perror(dev);  goto __rf_extract_exit; }
	if (fd == NULL) { perror(file); goto __rf_extract_exit; }

	printf("Extracting %s...\n", file);
	fseek(fs, from, SEEK_SET);

	for(i=from;i<to;i+=blk) {
		int ret;
		if (i+blk>to) blk = to-i;
		ret = fread(buf, blk, 1, fs);
		progressbar(i, to);
		if (!ret || feof(fs)) break;
		fwrite(buf, blk, 1, fd);
	}

	printf("\r%s: %lld bytes dumped from %s\n",
		file, to-from, dev);

__rf_extract_exit:
	if (fs) fclose(fs);
	if (fd) fclose(fd);

	return 1;
}

int rf_strip(char *file)
{
	FILE *fd = fopen(file, "rw");
	unsigned char buf[4096];
	int ret;
	off_t eof = 0;

	if (!fd) {
		fprintf(stderr, "Weird! can't open %s???\n", file);
		exit(0);
	}
	printf("Stripping padding from %s... ", file);
	fflush(stdout);
	fseek(fd, 0, SEEK_END);

	do {
		fseek(fd, -4096, SEEK_CUR);
		ret = fread(buf, 1, 4096, fd);
		fseek(fd, -4096, SEEK_CUR);
		for(ret--;ret>0;ret--)
			if (buf[ret]!=0xff) {
				fseek(fd, ret+1, SEEK_CUR);
				eof = ftell(fd);
				goto __done;
			}
	} while(1);

__done:
	fclose(fd);
	if (eof) truncate(file, eof);
	printf("done at %d\n", (int)eof);

	return 1;
}

int is_n800()
{
	int n800 = 0;
	unsigned char buf[4];
	FILE *fd = fopen("/dev/mtd0", "rb");

	if (!fd) {
		fprintf(stderr, "Cannot open /dev/mtd0.\n");
		exit(1);
	}
	fread(buf, 4, 1,fd);
	if (!memcmp("\xff\xff\xff\xff", buf, 4))
		n800 = 1;
	
	fclose(fd);

	return n800;
}


int reverse_extract_pieces(char *dir)
{
	char reply;
	chdir(dir);

	rf_extract("/dev/mtd0", is_n800()?0x200:0, 0x003600, "xloader.bin");
	rf_extract("/dev/mtd0", 0x004000, 0x01ffff,  "secondary.bin");
	rf_extract("/dev/mtd2", 0x000800, 0x200000,  "zImage");
	rf_extract("/dev/mtd3", 0x000000, 0x1D00000, "initfs.jffs2");
	printf("\nWARNING: the rootfs extraction on n800 is known to be buggy! feedback is welcome.\n\n");
	printf("Extract rootfs? (y/N): "); fflush(stdout);
	read(0, &reply, 1);
	if (reply=='y'||reply=='Y')
		rf_extract("/dev/mtd4", 0x000000, 0x6000000, "rootfs.jffs2");
	else	printf("*** Ignoring rootfs\n");
	rf_strip("xloader.bin");
	rf_strip("secondary.bin");
	rf_strip("zImage");
//	rf_strip("initfs.jffs2"); 	// do not strip initfs, is 2MB long
					//and can be useful for data recovery
	printf("Identifying extracted files...\n");
	printf("%s: xloader\n", fpid_file("xloader.bin"));
	printf("%s: secondary.bin\n", fpid_file("secondary.bin"));
	printf("%s: zImage\n", fpid_file("zImage"));
	printf("%s: initfs.jffs2\n", fpid_file("initfs.jffs2"));

	return 1;
}
