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

#define VERSION "0.1"

int uncompress_image(char *srcf, char *dstf);
int compress_image(char *srcf, char *dstf, int w, int h);
int rgb2yuv(char *from, char *to, int width, int height);
int rgba = 0;

static int show_usage()
{
	printf("Usage: logotool [-flags]\n");
	printf("  -u [img]   uncompress image to img.rgb\n");
	printf("  -c [img]   compres a 24 bit rgb raw image to a 16(565) one\n");
	printf("  -m [img]   show image using mplayer (dumps to <img>.yuv)\n");
	printf("  -v [img]   view uncompressed image using the 'display'\n");
	printf("             command from ImageMagick (in monochrome)\n");
	printf("  -w         specify width (required for -v and -c)\n");
	printf("  -a         force RGBA instead of RGB when compressing (use with -c)\n");
	printf("  -h         specify height (required for -v and -c)\n");
	printf("  -V         version number\n");
	printf("\n");
	printf("Example:\n");
	printf(" $ `logotool -u logo-file`   # uncompress and display a logo\n");
	return 1;
}

int main ( int argc , char** argv )
{	
	int c;
	int w=0,h=0;
	char *mimg = NULL;
	char *cimg = NULL;
	char *uimg = NULL;
	char *view = NULL;

	while((c = getopt(argc, argv, "w:h:Vv:u:c:m:a")) != -1) {
		switch(c) {
		case 'V': printf("%s\n", VERSION); return 0;
		case 'u': uimg = optarg; break;
		case 'c': cimg = optarg; break;
		case 'm': mimg = optarg; break;
		case 'a': rgba = 1; break;
		case 'w': w = atoi(optarg); break;
		case 'h': h = atoi(optarg); break;
		case 'v': view = optarg; break;
		}
	}

	if (mimg) {
		char *buf;
		if (w==0||h==0) {
			printf("You must specify width and height with '-w' and '-h'.\n");
			return 1;
		}

		buf = (char *)malloc(strlen(mimg)+128);
		sprintf(buf, "%s.rgb", mimg);

		if ( rgb2yuv(mimg, buf, w,h ) ) {
			sprintf(buf, "echo pause | mplayer %s.rgb -slave -demuxer rawvideo -rawvideo fps=1:w=%d:h=%d", mimg, w,h);
			system(buf);
		} else {
			printf("Oops\n");
		}
		free(buf);
	} else
	if (view) {
		char *buf;
		
		if (w==0||h==0) {
			printf("You must specify width and height with '-w' and '-h'.\n");
			return 1;
		}
		buf = (char *)malloc(strlen(view)+128);
		sprintf(buf, "display -size %dx%d -depth 16 %s", w,h,view);
		printf("%s\n", buf);
		system(buf);
		free(buf);
	} else
	if (uimg) {
		char *dst = (char*)malloc(strlen(uimg)+5);
		strcpy(dst, uimg);
		strcat(dst, ".gray");
		uncompress_image(uimg, dst);
		free(dst);
	} else
	if (cimg) {
		char *dst;
		if (w==0||h==0) {
			printf("You must specify width and height with '-w' and '-h'.\n");
			return 1;
		}
		dst = (char*)malloc(strlen(cimg)+6);
		strcpy(dst, cimg);
		strcat(dst, ".logo");
		compress_image(cimg, dst, w, h);
		free(dst);
	} else
		show_usage();

	return 0;
}
