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

#include <stdio.h>
#include "hash.h"

usho do_hash(usho *b, int len)
{
	usho result = 0;
	for(len>>=1;len--;b=b+1)
		result^=b[0];
	return result;
}

usho do_hash_file(char *filename)
{
	unsigned char buf[BSIZE];
	FILE *fd = fopen(filename, "r");
	usho hash = 0;
	int ret;

	if (fd == NULL) {
		fprintf(stderr, "ERROR: File '%s' not found.\n", filename);
		return -1;
	}

	do {	ret   = fread(&buf, 1, BSIZE, fd);
		if (ret == -1)
			return 0;
		hash ^= do_hash((usho *)&buf, ret);
	} while(ret);

	fclose(fd);

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
