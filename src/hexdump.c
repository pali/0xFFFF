/*
 * Copyright (C) 2006-2007
 *       pancake <pancake@youterm.com>
 *
 * radare is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * radare is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with radare; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <string.h>
#include "hexdump.h"

char getprintablechar(char a)
{
	if (a>=' '&&a<='~')
		return a;
	return '.';
}

int is_printable (int c)
{
	if (c<' '||c>'~') return 0;
	return 1;
}

/*
 * Helper function
 */
void dump_bytes(unsigned char *buf, int len)
{
	int i,j,seek = 0;
	int inc = 16;

	for(i=0; i<len; i+=inc) {
		fprintf(stderr, "0x%08x | ", seek+i);
		for(j=i;j<i+inc;j++) {
			if (j>=len) {
				fprintf(stderr, "   ");
				continue;
			}
			fprintf(stderr, "%02x ", buf[j]);
		}
		fprintf(stderr, "  ");
		for(j=i; j<i+inc; j++) {
			if (j >= len)
				fprintf(stderr, " ");
			else
			if ( is_printable(buf[j]) )
				fprintf(stderr, "%c", buf[j]);
			else	fprintf(stderr, ".");
		}
		fprintf(stderr, "\n");
	}
	fflush(stderr);
}
