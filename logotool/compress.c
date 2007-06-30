/*
 *  logotool - tool to modify logo images with simple compression
 *  Copyright (C) 2007
 *       pancake <pancake@youterm.com>
 *       esteve <esteve@eslack.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extern int rgba;

int isbe()
{
	int a=1; char *b=(char *)&a;
	return (int)b[0];
}

void ememcpy(char *dst, char *src)
{
	char buffer[4];
	memcpy(buffer, src, 4);
	dst[0] = buffer[3]; dst[1] = buffer[2];
	dst[2] = buffer[1]; dst[3] = buffer[0];
}

int compress_image(char *srcf, char *dstf, int w, int h)
{
	char buf[256];
	char tmp[256];
	int fdout;
	FILE *fd;
	int i,ret;

	fd = fopen(srcf, "rb");
	if (fd == NULL) {
		printf("Cannot open file '%s' \n", srcf);
		return 1;
	}

	fdout  = open( dstf, O_TRUNC | O_CREAT | O_WRONLY , 0660 );
	if (fdout == -1) {
		printf("Cannot open file '%s' for writing\n", dstf);
		return 1;
	}

	if (isbe()) {
		char oop[4];
		ememcpy((char *)&oop, (char *)&w);
		write(fdout, &oop, 4);
		ememcpy((char *)&oop, (char *)&h);
		write(fdout, &oop, 4);
	} else {
		write(fdout, &w, 4);
		write(fdout, &h, 4);
	}

	fseek(fd, 0, SEEK_END);
	if (!rgba && ftell(fd)%3) {
		fprintf(stderr, "Oops..looks like RGBA..forcing -a\n");
		rgba = 1;
	}
	fseek(fd, 0, SEEK_SET);

#define MAX 3*80
	if (rgba) {
		for(;;) {
			unsigned char c;
			ret = fread(&buf, 1, MAX, fd);
			if (ret <= 0) break;
			c = ret/4;
			write(fdout,&c, 1);
			for(i=0;i<ret;i+=4) {
				tmp[0]  = buf[i+0]&0xf8;
				tmp[0] |= buf[i+1]&0x07;
				tmp[1]  = buf[i+1]&0xe0;
				tmp[1] |= buf[i+2]&0x1f;
				write(fdout, tmp, 2);
			}
		}
	} else {
		for(;;) {
			unsigned char c;
			ret = fread(buf, 1, MAX, fd);
			if (ret <= 0) break;
			c = ret/3;
			write(fdout,&c, 1);
			for(i=0;i<ret;i+=3) {
				tmp[0]  = buf[i+0]&0xf8;
				tmp[0] |= buf[i+1]&0x07;
				tmp[1]  = buf[i+1]&0xe0;
				tmp[1] |= buf[i+2]&0x1f;
				write(fdout, tmp, 2);
			}
		}
	}
	fclose(fd);
	close(fdout);
	printf("eval PATH=$PWD:$PATH logotool -u %s\n", dstf);
	return 1;
}
#if 0
// TODO: compression stuff
		/* is a repeated block? */
		c = buf[0];
		for(i=1;i<MAX&&buf[i]==c&&buf[i+1]==c;i+=2);
printf("memcpy %d -> %d (ret %d)\n",i,ret-i, ret);
for(i=0;i<ret-i;i++)
printf(" %02x ", (unsigned char)buf[i]);
printf("\n");
		if (i>5) {
//		if (i==MAX) {
			if (0x80>ret) printf("PRON\n");
			c = 0x80 | ret; //(int)((2*ret/3)>>1);
			write(fdout,&c, 1);
			tmp[0]  = buf[i+0]&0xf8;
			tmp[0] |= buf[i+1]&0x07;
			tmp[1]  = buf[i+1]&0xe0;
			tmp[1] |= buf[i+1]&0x1f;
			write(fdout, tmp, 2);
			// restore state
			memcpy(buf,buf+i,ret-i);
			ret -= i;
		}
		
#endif
