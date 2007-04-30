/*
 * Copyright (C) 2007
 *       pancake <pancake@youterm.com>
 *       esteve <esteve@eslack.org>
 *
 * logotool is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * logotool is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with logotool; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int compress_image(char *srcf, char *dstf, int w, int h)
{
	char buf[256];
	char tmp[256];
	int fd, fdout;
	int i,ret;

	fd = open(srcf, O_RDONLY);
	if (fd == -1) {
		printf("Cannot open file '%s' \n", srcf);
		return 1;
	}

	fdout  = open( dstf, O_CREAT | O_WRONLY , 0660 );
	if (fdout == -1) {
		printf("Cannot open file '%s' for writing\n", dstf);
		return 1;
	}

	// TODO: force little endian!! (not safe for big endian)
	write(fdout, &w, 4);
	write(fdout, &h, 4);

	for(;;) {
		unsigned char c;
		ret = read(fd, buf, 3*80);
		c = (int)((2*ret/3)>>1);
		write(fdout,&c, 1);
		if (ret <= 0) break;
		for(i=0;i<ret;i+=3) {
			tmp[0]  = buf[i+0]&0xf8;
			tmp[0] |= buf[i+1]&0x07;
			tmp[1]  = buf[i+1]&0xe0;
			tmp[1] |= buf[i+1]&0x1f;
			write(fdout, tmp, 2);
		}
	}
	close(fd);
	close(fdout);
	printf("eval PATH=$PWD:$PATH logotool -u %s\n", dstf);
	return 1;
}
