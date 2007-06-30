/*
 *  logotool - tool to modify logo images with simple compression
 *  Copyright (C) 2007
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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


void RGBtoYUV(unsigned short in565, unsigned char *yuv)
{
	/* 2 bytes, 565 
	 * RRRRR GGGGGG BBBBB
	 */
	
	int i;
	i = (in565>>8)&0xFF;
	in565 = (in565<<8)&0xFF00;
	in565 |= i;

	int rgb[3];
		
	rgb[0] = (in565>>11)&0x1F;
	rgb[1] = (in565>>5)&0x3F;
	rgb[2] = (in565)&0x1F;
	
	rgb[0] = (rgb[0]<<3) | 0x7;
	rgb[1] = (rgb[1]<<2) | 0x3;
	rgb[2] = (rgb[2]<<3) | 0x7;

	/*yuv[0] = 0.299*rgb[0] + 0.587*rgb[1] + 0.114*rgb[2];
	yuv[1] = (rgb[2]-yuv[0])*0.565 + 128;
	yuv[2] = (rgb[0]-yuv[0])*0.713 + 128;*/

	yuv[0] = (0.257 * rgb[0]) + (0.504 * rgb[1]) + (0.098 * rgb[2]) + 16;
	yuv[1] = 128 - (0.148 * rgb[0]) - (0.291 * rgb[1]) + (0.439 * rgb[2]);
	yuv[2] = 128 + (0.439 * rgb[0]) - (0.368 * rgb[1]) - (0.071 * rgb[2]);
}
// mplayer caca.yuv -demuxer rawvideo  -rawvideo fps=1:w=416:h=70  //:i420


int rgb2yuv(char *from, char *to, int width, int height)
{	
	int fd=open (from, O_RDONLY );
	int fout= open ( to, O_CREAT| O_WRONLY , 0660 );
	int fsize;
	unsigned char* src;
	unsigned char* dstY;
	unsigned char* dstU;
	unsigned char* dstV;
	unsigned char YUV[3];
	unsigned short inP;	
	int i,j;

	if (fd == -1) {
		printf("cannot open %s\n", from);
		return 0;
	}
	if (fout == -1) {
		printf("cannot open %s\n", to);
		return 0;
	}
	fsize = lseek ( fd, 0,SEEK_END);
	fsize = lseek ( fd, 0,SEEK_CUR);
	lseek ( fd,0, SEEK_SET);
		
	src=malloc ( fsize );
	dstY=malloc ( (fsize/2) );
	dstU=malloc ( (fsize/2) );
	dstV=malloc ( (fsize/2) );

	
	for ( i = 0 ; i < fsize; i+=2 ) {
		read ( fd, &inP, 2 );
		RGBtoYUV ( inP, YUV );
		dstY[i>>1]=YUV[0];
		dstU[i>>1]=YUV[1];
		dstV[i>>1]=YUV[2];
	}

	for ( i = 0 ; i < (fsize/2); i++ )
		write ( fout,&dstY[i], 1);

	for ( i = 0 ; i < height ; i+=2 )
		for ( j = 0; j < width ; j +=2 )
			write ( fout,&dstU[j+(i*width)], 1);

	for ( i = 0 ; i < height ; i+=2 )
		for ( j = 0; j < width ; j +=2 )
			write ( fout,&dstV[j+(i*width)], 1);
	return 1;
}	
