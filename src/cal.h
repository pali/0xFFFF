/*
    0xFFFF - Open Free Fiasco Firmware Flasher
    Copyright (c) 2011  Michael Buesch <mb@bu3sch.de>
    Copyright (C) 2012  Pali Rohár <pali.rohar@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef CAL_H
#define CAL_H

#define CAL_MAX_NAME_LEN	16
#define CAL_FLAG_USER		0x0001
#define CAL_FLAG_WRITE_ONCE	0x0002

struct cal;

int cal_init(struct cal ** cal_out);
int cal_init_file(const char * file, struct cal ** cal_out);
void cal_finish(struct cal * cal);
int cal_read_block(struct cal * cal, const char * name, void ** ptr, unsigned long * len, unsigned long flags);

#endif
