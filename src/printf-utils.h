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

#ifndef PRINTF_UTILS_H
#define PRINTF_UTILS_H

extern int printf_prev;

#define PRINTF_BACK() do { if ( printf_prev ) { printf("\r%-*s\r", printf_prev, ""); printf_prev = 0; } } while (0)
#define PRINTF_ADD(...) do { printf_prev += printf(__VA_ARGS__); } while (0)
#define PRINTF_LINE(...) do { PRINTF_BACK(); PRINTF_ADD(__VA_ARGS__); fflush(stdout); } while (0)
#define PRINTF_END() do { if ( printf_prev ) { printf("\n"); printf_prev = 0; } } while (0)
#define PRINTF_ERROR(...) do { PRINTF_END(); ERROR_INFO(__VA_ARGS__); } while (0)

void printf_progressbar(unsigned long long part, unsigned long long total);
void printf_and_wait(const char * format, ...);

#endif
