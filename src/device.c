

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

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

	if ( ! device )
		return DEVICE_ANY;

	for ( i = 0; i < sizeof(devices)/sizeof(devices[0]); ++i )
		if ( devices[i] && strcmp(devices[i], device) == 0 )
			return i;

	return DEVICE_UNKNOWN;

}

const char * device_to_string(enum device device) {

	if ( device > sizeof(devices) )
		return NULL;

	return devices[device];

}
