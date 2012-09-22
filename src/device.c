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

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "global.h"

#include "device.h"

static const char * devices[] = {
	[DEVICE_SU_18] = "SU-18",
	[DEVICE_RX_34] = "RX-34",
	[DEVICE_RX_44] = "RX-44",
	[DEVICE_RX_48] = "RX-48",
	[DEVICE_RX_51] = "RX-51",
};

enum device device_from_string(const char * device) {

	size_t i;

	if ( ! device || ! device[0] )
		return DEVICE_ANY;

	for ( i = 0; i < sizeof(devices)/sizeof(devices[0]); ++i )
		if ( devices[i] && strcmp(devices[i], device) == 0 )
			return i;

	return DEVICE_UNKNOWN;

}

const char * device_to_string(enum device device) {

	if ( device > sizeof(devices)/sizeof(devices[0]) )
		return NULL;

	return devices[device];

}

static const char * long_devices[] = {
	[DEVICE_SU_18] = "Nokia 770",
	[DEVICE_RX_34] = "Nokia N800",
	[DEVICE_RX_44] = "Nokia N810",
	[DEVICE_RX_48] = "Nokia N810 Wimax",
	[DEVICE_RX_51] = "Nokia N900",
};

const char * device_to_long_string(enum device device) {

	if ( device > sizeof(long_devices)/sizeof(long_devices[0]) )
		return NULL;

	return long_devices[device];

}

int hwrevs_is_valid(const int16_t * hwrevs, int16_t hwrev) {

	int i;
	for ( i = 0; hwrevs[i] != -1; ++i )
		if ( hwrev == hwrevs[i] )
			return 1;
	return 0;

}

int16_t * hwrevs_alloc_from_string(const char * str) {

	const char * ptr = str;
	const char * oldptr = ptr;
	int count = 2;
	int16_t * ret;
	int16_t tmp;
	int i;
	char buf[5];

	while ( (ptr = strchr(ptr, ',')) ) {
		++count;
		++ptr;
	}

	ret = calloc(count, sizeof(int16_t));
	if ( ! ret )
		return NULL;

	i = 0;

	while ( (ptr = strchr(ptr, ',')) ) {
		strncpy(buf, oldptr, ptr-oldptr);
		buf[4] = 0;
		tmp = atoi(buf);
		if ( tmp >= 0 )
			ret[i++] = tmp;
		++ptr;
		oldptr = ptr;
	}

	tmp = atoi(oldptr);
	if ( tmp >= 0 )
		ret[i++] = tmp;

	ret[i++] = -1;
	return ret;

}

char * hwrevs_alloc_to_string(const int16_t * hwrevs) {

	char * ret;
	char * ptr;
	int i;
	int len = 0;

	for ( i = 0; hwrevs[i] != -1; ++i )
		len += log10(hwrevs[i])+2;

	ret = calloc(1, len);
	if ( ! ret )
		return NULL;

	ptr = ret;
	for ( i = 0; hwrevs[i] != -1; ++i )
		ptr += sprintf(ptr, "%d,", hwrevs[i]);

	*(ptr-1) = 0;
	return ret;

}

char ** device_hwrevs_alloc_to_256bufs(const struct device_hwrevs * device_hwrevs) {

	int16_t * hwrevs = device_hwrevs->hwrevs;
	const char * device;
	int count = 0;
	char ** ret;
	int i, j, k;

	device = device_to_string(device_hwrevs->device);
	if ( ! device )
		return NULL;

	for ( i = 0; hwrevs[i] != -1; ++i )
		if ( hwrevs[i] >= 0 && hwrevs[i] <= 9999 )
			++count;

	count = count/30 + 2;

	ret = malloc(count*sizeof(char *) + count*256);
	if ( ! ret )
		return NULL;

	i = -1;
	j = 0;

	while ( hwrevs[i+1] != -1 ) {

		ret[j] = (char *)ret + count*sizeof(char *) + j*256;

		memset(ret[j], 0, 256);
		strncpy(ret[j], device, 16);

		k = 0;

		for ( k = 0; k < 30; ++k ) {

			++i;

			if ( hwrevs[i] < 0 || hwrevs[i] > 9999 )
				continue;

			memset(ret[j] + k*8, 0, 8);
			snprintf(ret[j] + k*8, 8, "%d", hwrevs[i]);

		}

		++j;

	}

	ret[j] = NULL;

	return ret;

}

struct device_hwrevs device_hwrevs_alloc_from_256buf(const char * buf, size_t size) {

	int i;
	int len;
	int count;
	char str[17];
	const char * ptr1;
	const char * ptr;
	struct device_hwrevs ret;

	len = strnlen(buf, size);
	if ( len > 16 )
		len = 16;

	memset(str, 0, sizeof(str));
	memcpy(str, buf, len);
	ptr1 = buf + len;

	ret.device = device_from_string(str);

	ptr = ptr1;
	count = 1;
	while ( ptr < buf + size ) {

		while ( ptr < buf + size && *ptr < 32 )
			++ptr;

		if ( ptr >= buf + size )
			break;

		len = strnlen(ptr, buf + size - ptr);
		if ( len > 8 )
			len = 8;

		ptr += len + 1;
		++count;

	}

	ret.hwrevs = calloc(count, sizeof(int16_t));
	if ( ! ret.hwrevs )
		return ret;

	ptr = ptr1;
	i = 0;
	while ( ptr < buf + size ) {

		while ( ptr < buf + size && *ptr < 32 )
			++ptr;

		if ( ptr >= buf + size )
			break;

		len = strnlen(ptr, buf + size - ptr);
		if ( len > 8 )
			len = 8;

		memset(str, 0, sizeof(str));
		memcpy(str, ptr, len);
		ptr += len + 1;
		ret.hwrevs[i++] = atoi(str);

	}

	ret.hwrevs[i] = -1;
	return ret;

}
