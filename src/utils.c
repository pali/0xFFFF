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
#include <string.h>
#include <stdlib.h>

unsigned long get_file_size(char *file)
{
	FILE *fd = fopen(file, "r");
	unsigned long len = 0;
	if (fd == NULL) {
		fprintf(stderr, "Cannot open file '%s'\n", file);
		exit(1);
	}
	fseek(fd, 0, SEEK_END);
	len = ftell(fd);
	fclose(fd);
	return len;
}

void progressbar(unsigned long long part, unsigned long long total)
{
        char *columns = getenv("COLUMNS");
	int pc;
        int tmp, cols = 80;

	pc = (int)(part*100/total);
        (pc<0)?pc=0:(pc>100)?pc=100:0;
        printf("\e[K  %3d%% [", pc);
        if (columns)
                cols = atoi(columns);
        cols-=15;
        for(tmp=cols*pc/100;tmp;tmp--) printf("#");
        for(tmp=cols-(cols*pc/100);tmp;tmp--) printf("-");
        printf("]\r");
        fflush(stdout);
}
