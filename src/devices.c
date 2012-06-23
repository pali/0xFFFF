/*
 *  0xFFFF - Open Free Fiasco Firmware Flasher
 *  Copyright (C) 2007-2009  pancake <pancake@nopcode.org>
 *  Copyright (C) 2012       Pali Rohár <pali.rohar@gmail.com>
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

#if HAVE_USB
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
 *        cold-flash   flash   emmc-flash  mass-stor  pc-suite
 * n770              0421:0105             0421:0431
 * n800              0421:04c3             0421:0431
 * n810              0421:0105
 * n900   0421:0106  0421:0105  0421:????  0421:01c7  0421:01c8
 *
 */
struct devices supported_devices[SUPPORTED_DEVICES] = {
  { "FFFF",           0x0000, 0x0000, FLASH_NORMAL },  // dummy
  { "unkn",           0x0421, 0x3f00, FLASH_NORMAL },  // probably a development board
  { "n800",           0x0421, 0x04c3, FLASH_NORMAL },  // a n800 
  { "n770/n810/n900", 0x0421, 0x0105, FLASH_NORMAL },
  { "n900",           0x0421, 0x0106, FLASH_COLD   },
//  { "n900",           0x0421, 0x????, FLASH_EMMC   },
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
	int dev_index = 0;
	struct usb_bus *bus;
	if ((usb_find_busses()<0) || (usb_find_devices()==-1)) {
		fprintf(stderr, "\nerror: no usb bus or device found.\n");
		exit(1);
	} else {
		for (bus = usb_busses; bus; bus = bus->next) {
			struct usb_device *dev = bus->devices;
			D printf("bus: \n");
			for (; dev; dev = dev->next) { 
				*udd = dev->descriptor;
				D printf(" dev (%s) - ", dev->filename);
				D printf("vendor: %04x product: %04x\n", udd->idVendor, udd->idProduct);

				if ((dev_index = is_valid_device(udd))) {
					device = dev;
					printf("%s found!\n", dev->filename);
					*it_device = supported_devices[dev_index];
					return 1;
				}
			}
		}
	}
	return 0;
}
#endif
