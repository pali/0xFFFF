/*
 *  0xFFFF - Open Free Fiasco Firmware Flasher
 *  Copyright (C) 2007  pancake <pancake@youterm.com>
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

#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int pcs_n = 0;
struct piece_t pcs[10];

int add_piece(char *piece)
{
	int i,ok = 0;
	char *file;

	if (pcs_n==9) {
		fprintf(stderr, "Oops...cannot add more pieces. no sense operation!\n");
		return 0;
	}

	file = strchr(piece, '%');
	if (file) {
		file[0]='\0';
		file = file + 1;
		for(i=0;pieces[i];i++)
			if (!strcmp(pieces[i], piece))
				ok = 1;
		if (!ok) {
			printf("Invalid piece name.\n");
			printf("Pieces: ");
			for(i=0;pieces[i];i++)
				printf("%s ", pieces[i]);
			printf("\n");
			exit(1);
		}

		pcs[pcs_n].name = strdup(file);
		pcs[pcs_n].type = strdup(piece);
		pcs[pcs_n].vers = NULL; // TODO version string not yet supported
	} else {
		/*/ autodetect piece type */
		pcs[pcs_n].type = (char *)fpid_file(piece);
		if (pcs[pcs_n].type == NULL) {
			printf("Use -p [piece]:[file]\n");
			printf("Pieces: ");
			for(i=0;pieces[i];i++)
				printf("%s ", pieces[i]);
			printf("\n");
			exit(1);
		} else {
			pcs[pcs_n].name = strdup(piece);
			pcs[pcs_n].type = strdup(pcs[pcs_n].type);
			pcs[pcs_n].vers = NULL;
		}
	}

	pcs_n++;

	return 1;
}
