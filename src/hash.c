/*
 *  0xFFFF - Open Free Fiasco Firmware Flasher
 *  Copyright (C) 2007-2009  pancake <pancake@youterm.com>
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
#include "hash.h"

usho do_hash(usho *b, int len)
{
	usho result = 0;
	for(len>>=1;len--;b=b+1)
		result^=b[0];
	return result;
}

usho do_hash_file(const char *filename, const char *type)
{
	unsigned char buf[BSIZE];
	FILE *fd = fopen(filename, "r");
	usho hash = 0;
	int size;
	int ret;

	if (fd == NULL) {
		fprintf(stderr, "ERROR: File '%s' not found.\n", filename);
		return -1;
	}

	do {
		ret   = fread(&buf, 1, BSIZE, fd);
		if (ret == -1)
			return 0;
		hash ^= do_hash((usho *)&buf, ret);
	} while(ret);

	size = ftell(fd);
	fclose(fd);

	/* mmc image must be aligned */
	if (type && strcmp(type, "mmc") == 0) {
		int align = ((size >> 8) + 1) << 8;
		printf("align from %d to %d\n", size, align);
		buf[0] = 0xff;
		while (size < align) {
			hash ^= do_hash((usho *)&buf, 1);
			++size;
		}
	}

	return hash;
}

#if 0
main()
{
	usho us = do_hash_file("zImage");
	unsigned char *h= (unsigned char *)&us;
	printf("%x %x\n",h[0],h[1]);
}
#endif
