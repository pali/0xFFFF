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

	if ( device >= sizeof(devices)/sizeof(devices[0]) )
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

	if ( device >= sizeof(long_devices)/sizeof(long_devices[0]) )
		return NULL;

	return long_devices[device];

}

int hwrev_is_valid(const int16_t * hwrevs, int16_t hwrev) {

	int i;

	/* hwrevs not specified, valid for any hwrev */
	if ( ! hwrevs )
		return 1;

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

	if ( ! str )
		return NULL;

	while ( (ptr = strchr(ptr, ',')) ) {
		++count;
		++ptr;
	}

	ret = calloc(count, sizeof(int16_t));
	if ( ! ret )
		return NULL;

	i = 0;

	ptr = str;
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
		len += log10(hwrevs[i]+1)+2;

	ret = calloc(1, len+1);
	if ( ! ret )
		return NULL;

	ptr = ret;
	for ( i = 0; hwrevs[i] != -1; ++i )
		ptr += sprintf(ptr, "%d,", hwrevs[i]);

	*(ptr-1) = 0;
	return ret;

}

#define MAX_HWREVS 29

char ** device_list_alloc_to_bufs(const struct device_list * device_list) {

	int count = 0;
	int size = 0;
	const char * device;
	const struct device_list * device_first;
	char ** ret;
	char * last_ptr;
	int i, j, k;

	device_first = device_list;

	while ( device_first ) {

		int local = 0;

		if ( ! device_to_string(device_first->device) ) {
			device_first = device_first->next;
			continue;
		}

		for ( i = 0; device_first->hwrevs[i] != -1; ++i )
			if ( device_first->hwrevs[i] >= 0 && device_first->hwrevs[i] <= 9999 )
				++local;

		size += (1+16+(MAX_HWREVS+1)*8)*(local/MAX_HWREVS+1);
		count += local/MAX_HWREVS;
		if ( local%MAX_HWREVS != 0 || local == 0 )
			++count;

		device_first = device_first->next;

	}

	ret = calloc(1, (count+1)*sizeof(char *) + size);
	if ( ! ret )
		return NULL;

	j = 0;
	last_ptr = (char *)ret + (count+1)*sizeof(char *);
	device_first = device_list;

	while ( device_first ) {

		i = -1;

		device = device_to_string(device_first->device);
		if ( ! device ) {
			device_first = device_first->next;
			continue;
		}

		while ( device_first->hwrevs[i+1] != -1 ) {

			uint8_t len = 0;
			ret[j] = ++last_ptr;

			strncpy(ret[j]+1, device, 16);
			last_ptr += 16;
			len += 16;

			k = 0;

			for ( k = 0; k < MAX_HWREVS; ++k ) {

				if ( device_first->hwrevs[i+1] == -1 )
					break;

				++i;

				if ( device_first->hwrevs[i] < 0 || device_first->hwrevs[i] > 9999 )
					continue;

				snprintf(ret[j]+1+16+k*8, 8, "%d", device_first->hwrevs[i]);
				last_ptr += 8;
				len += 8;

			}

			((uint8_t*)ret[j])[0] = len;

			++j;

		}

		device_first = device_first->next;

	}

	ret[j] = NULL;

	return ret;

}

struct device_list * device_list_alloc_from_buf(const char * buf, size_t size) {

	int i;
	int len;
	int count;
	char str[17];
	const char * ptr1;
	const char * ptr;
	struct device_list * ret;

	ret = calloc(1, sizeof(struct device_list));
	if ( ! ret )
		return NULL;

	len = strnlen(buf, size);
	if ( len > 16 )
		len = 16;

	memset(str, 0, sizeof(str));
	memcpy(str, buf, len);
	ptr1 = buf + len;

	ret->device = device_from_string(str);

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

	ret->hwrevs = calloc(count, sizeof(int16_t));
	if ( ! ret->hwrevs )
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
		ret->hwrevs[i++] = atoi(str);

	}

	ret->hwrevs[i] = -1;
	return ret;

}
