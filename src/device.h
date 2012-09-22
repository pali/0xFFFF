/*
    0xFFFF - Open Free Fiasco Firmware Flasher
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

#ifndef DEVICE_H
#define DEVICE_H

enum device {
	DEVICE_UNKNOWN = 0,
	DEVICE_ANY,   /* Unspecified / Any device */
	DEVICE_SU_18, /* Nokia 770 */
	DEVICE_RX_34, /* Nokia N800 */
	DEVICE_RX_44, /* Nokia N810 */
	DEVICE_RX_48, /* Nokia N810 WiMax */
	DEVICE_RX_51, /* Nokia N900 */
	DEVICE_COUNT,
};

/*
  hwrevs - array of int16_t
         - terminated by -1
         - valid numbers: 0-9999
*/
struct device_hwrevs {
	enum device device;
	int16_t * hwrevs;
};

enum device device_from_string(const char * device);
const char * device_to_string(enum device device);
const char * device_to_long_string(enum device device);

int hwrevs_is_valid(const int16_t * hwrevs, int16_t hwrev);

int16_t * hwrevs_alloc_from_string(const char * str);
char * hwrevs_alloc_to_string(const int16_t * hwrevs);

char ** device_hwrevs_alloc_to_256bufs(const struct device_hwrevs * device_hwrevs);
struct device_hwrevs device_hwrevs_alloc_from_256buf(const char * buf, size_t size);

#endif
