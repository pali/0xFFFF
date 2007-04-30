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

int uncompress_image(char *srcf, char *dstf)
{
	int fd;
	int fout;
	int width,height;
	int fsize;
	char* src;
	char* dst;
	char* src2;
	char* eof;
	char *deof;
	char* dst2 = NULL;
	unsigned char byte;

	fd    = open( srcf, O_RDONLY );

	if (fd == -1) {
		printf("Cannot open file '%s'\n", srcf);
		return 1;
	}

	fout  = open( dstf, O_CREAT | O_WRONLY , 0660 );

	if (fout == -1) {
		printf("Cannot open file '%s' for writing\n", dstf);
		return 1;
	}

	fprintf(stderr, "Input file: %s\n", srcf);
	fprintf(stderr, "Output file: %s\n", dstf);
	fsize = lseek ( fd, 0,SEEK_END);
	fsize = lseek ( fd, 0,SEEK_CUR);
	lseek ( fd,0, SEEK_SET);

	fsize -= 8;

	read ( fd, &width, 4 );
	read ( fd, &height, 4 );

	fprintf(stderr, "Width: %d\nHeight: %d\n",width, height);
	fprintf(stderr, "Input Size: %d\n",fsize);
	src = malloc ( fsize );
	dst = malloc ( width*height*2 );
	deof = dst+width*height*2;
	if ((int)src == -1 || (int)dst == -1 ) {
		printf("Cannot malloc\n");
		return 1;
	}

	memset(dst,'\0', width*height*2);

	read(fd, src, fsize);
	eof = src+fsize;

	for(dst2 = dst, src2 = src; src2<eof;) {
		//byte = src2[0] & 0xff;
		//src2 = src2 + 1;
		byte=(*src2++)&0xff;
		// [1xxxyyyy] [char] [char]
		// copies xxxyyyy times two chars
		if (byte > 0x7f) {
			int i,j = byte&0x7f;
			for(i=0; i++<j; dst2+=2) 
				memcpy(dst2, src2, 2);
			src2+=2;
		} else {	
			if ( byte != 0x00 ) {
				byte<<=1;
			//	fprintf(stderr, "COPYING: %d %d\n", byte, (eof-src2));
				if ((dst2+byte >= deof || (src2+byte >= eof))) {
					fprintf(stderr, "Break\n");
					break;
				} 
				memcpy(dst2, src2, byte);
				src2+=byte; dst2+=byte;
			}
		}
	}
	fprintf(stderr, "Output Size: %d\n",dst2-dst);

	if ((dst2-dst)!=(width*height*2))
		fprintf(stderr, "failed?\n");

	write ( fout, dst,  width*height*2);

	close(fout);
	close(fd);

	//printf("./logotool -w %d -h %d -v %s || \n", width, height, dstf);
	//printf("logotool -w %d -h %d -v %s\n", width, height, dstf);
	printf("eval PATH=$PWD:$PATH logotool -w %d -h %d -m %s\n", width, height, dstf);

	return 0;
}
