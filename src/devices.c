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

/*
 * The internet tablet PC (and probably most of
 * Nokia devices have different USB-IDs depending
 * on the boot state.
 *
 * The following table shows this info:
 *
 *           boot       os
 * n770   0421:0105  0421:0431
 * n800              0421:0431
 *
 */
struct devices supported_devices[SUPPORTED_DEVICES] = {
  { "FFFF", 0x000, 0x0000, 0x0000 },  // dummy
  { "unkn", 0x421, 0x3f00, 0x0000 },  // probably a development board
  { "n770", 0x421, 0x0105, 0x0001 },  // my n770 
  { "n800", 0x421, 0x04c3, 0x0001 },  // a n800 
  { 0 },
  { 0 }
};

/** Returns 0 when no device was found and a positive
  * non-zero value when one was found.
  *
  * The return value is the index into the supported_devices
  * array which denotes the device which was found.
  */
int is_valid_device(struct usb_device_descriptor *udd)
{
	int i;
	struct devices ptr = supported_devices[1];

	for(i=1 ; ptr.vendor_id; ptr = supported_devices[++i])
		if ((udd->idVendor  == ptr.vendor_id)
		&&  (udd->idProduct == ptr.product_id)) {
                        D printf("found %s\n", supported_devices[i].name);
			return i;
		}

	return 0;
}

/*
 * List all supported device-ids and it's names
 *
 */
void list_valid_devices()
{
	int i;
	struct devices ptr = supported_devices[1];

	for(i=1; ptr.vendor_id; ptr = supported_devices[++i])
		printf("%04x:%04x  %s\n", ptr.vendor_id, ptr.product_id, ptr.name);
}

/*
 * Returns true (1) when a valid usb device is found and stores the udd and devices
 *   structures for the specified device.
 * Otherwise returns false (0)
 */

int usb_device_found(struct usb_device_descriptor *udd, struct devices *it_device)
{
	struct usb_bus *bus;
        int dev_index = 0;

	if (usb_find_busses() < 0) {
		fprintf(stderr, "\nerror: no usb busses found.\n");
		exit(1);
	} else {
		switch(usb_find_devices()) {
		case -1:
			fprintf(stderr, "\nerror: no devices found.\n");
			exit(1);
		default:
			for (bus = usb_busses; bus; bus = bus->next) {
				struct usb_device *dev = bus->devices;
				D printf("bus: \n");
				for (; dev; dev = dev->next) { 
					*udd = dev->descriptor;
					D printf(" dev (%s) - ", dev->filename);
					D printf("vendor: %04x product: %04x\n", udd->idVendor, udd->idProduct);

					if ((dev_index = is_valid_device(udd))) {
						device = dev;
                                                *it_device = supported_devices[dev_index];
						return 1;
					}
				}
			}
		}
	}

	return 0;
}
