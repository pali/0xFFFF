/*
 *  0xFFFF - Open Free Fiasco Firmware Flasher
 *  Copyright (C) 2007  pancake <pancake@youterm.com>
 *  Copyright (C) 2012  Pali Rohár <pali.rohar@gmail.com>
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

#include "printf-utils.h"

int printf_prev = 0;

void printf_progressbar(unsigned long long part, unsigned long long total) {

	char *columns = getenv("COLUMNS");
	int pc;
	int tmp, cols = 80;

	/* percentage calculation */
	pc = (int)(part*100/total);
	(pc<0)?pc=0:(pc>100)?pc=100:0;

#if HAVE_SQUEUE
	if (qmode) {
		char msg[128];
		sprintf(msg, "%d%%", pc);
		squeue_push2(p, "bar", msg, 0);
	} else {
#endif
		PRINTF_BACK();
		PRINTF_ADD("\x1b[K  %3d%% [", pc);
		if (columns)
			cols = atoi(columns);
		if (cols > 115)
			cols = 115;
		cols-=15;
		for(tmp=cols*pc/100;tmp;tmp--) PRINTF_ADD("#");
		for(tmp=cols-(cols*pc/100);tmp;tmp--) PRINTF_ADD("-");
		PRINTF_ADD("]");
		fflush(stdout);
#if HAVE_SQUEUE
	}
#endif

}
