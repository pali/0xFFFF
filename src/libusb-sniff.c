/*
    libusb-sniff.c - Library for usb sniffing nokia's flasher-3.5
    Copyright (C) 2011-2012  Pali Rohár <pali.rohar@gmail.com>

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

/* compile: gcc libusb-sniff.c -o libusb-sniff.so -W -Wall -O2 -fPIC -ldl -shared -m32 */
/* usage: sudo USBSNIFF_WAIT=1 LD_PRELOAD=./libusb-sniff.so flasher-3.5 ... */
/* usage: sudo USBSNIFF_SKIP_READ=1 USBSNIFF_SKIP_WRITE=1 LD_PRELOAD=./libusb-sniff.so flasher-3.5 ... */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <usb.h>

static char to_ascii(char c) {

	if ( c >= 32 && c <= 126 )
		return c;
	return '.';

}

static void dump_bytes(const char * buf, size_t size) {

	size_t i, ascii_cnt = 0;
	char ascii[17] = { 0, };

	for ( i = 0; i < size; i++ ) {
		if ( i % 16 == 0 ) {
			if ( i != 0 ) {
				printf("  |%s|\n", ascii);
				ascii[0] = 0;
				ascii_cnt = 0;
			}
			printf("%04X:", (unsigned int)i);
		}
		printf(" %02X", buf[i] & 0xFF);
		ascii[ascii_cnt] = to_ascii(buf[i]);
		ascii[ascii_cnt + 1] = 0;
		ascii_cnt++;
	}

	if ( ascii[0] ) {
		if ( size % 16 )
			for ( i = 0; i < 16 - (size % 16); i++ )
				printf("   ");
		printf("  |%s|\n", ascii);
	}

}

int usb_bulk_write(usb_dev_handle * dev, int ep, const char * bytes, int size, int timeout) {

	static int (*real_usb_bulk_write)(usb_dev_handle * dev, int ep, const char * bytes, int size, int timeout) = NULL;

	if ( ! real_usb_bulk_write )
		real_usb_bulk_write = dlsym(RTLD_NEXT, "usb_bulk_write");

	if ( ! getenv("USBSNIFF_SKIP_WRITE") ) {

		printf("\n==== usb_bulk_write (ep=%d size=%d timeout=%d) ====\n", ep, size, timeout);
		dump_bytes(bytes, size);
		printf("====\n");

		if ( getenv("USBSNIFF_WAIT") ) {
			printf("Press ENTER"); fflush(stdout); getchar();
		}

	}

	return real_usb_bulk_write(dev, ep, bytes, size, timeout);

}

int usb_bulk_read(usb_dev_handle * dev, int ep, char * bytes, int size, int timeout) {

	static int (*real_usb_bulk_read)(usb_dev_handle * dev, int ep, char * bytes, int size, int timeout) = NULL;
	int ret;

	if ( ! real_usb_bulk_read )
		real_usb_bulk_read = dlsym(RTLD_NEXT, "usb_bulk_read");

	ret = real_usb_bulk_read(dev, ep, bytes, size, timeout);

	if ( ! getenv("USBSNIFF_SKIP_READ") ) {

		printf("\n==== usb_bulk_read (ep=%d size=%d timeout=%d) ret = %d ====\n", ep, size, timeout, ret);
		if ( ret > 0 ) {
			dump_bytes(bytes, ret);
			printf("====\n");
		}

		if ( getenv("USBSNIFF_WAIT") ) {
			printf("Press ENTER"); fflush(stdout); getchar();
		}

	}

	return ret;

}

int usb_control_msg(usb_dev_handle *dev, int requesttype, int request, int value, int index, char *bytes, int size, int timeout) {

	static int (*real_usb_control_msg)(usb_dev_handle *dev, int requesttype, int request, int value, int index, char *bytes, int size, int timeout) = NULL;
	int ret;

	if ( ! real_usb_control_msg )
		real_usb_control_msg = dlsym(RTLD_NEXT, "usb_control_msg");

	if ( requesttype == 64 && ! getenv("USBSNIFF_SKIP_CONTROL") ) {

		printf("\n==== usb_control_msg(requesttype=%d, request=%d, value=%d, index=%d, size=%d, timeout=%d) ====\n", requesttype, request, value, index, size, timeout);
		dump_bytes(bytes, size);
		printf("====\n");

		if ( getenv("USBSNIFF_WAIT") ) {
			printf("Press ENTER"); fflush(stdout); getchar();
		}

	}

	ret = real_usb_control_msg(dev, requesttype, request, value, index, bytes, size, timeout);

	if ( requesttype != 64 && ! getenv("USBSNIFF_SKIP_CONTROL") ) {

		printf("\n==== usb_control_msg(requesttype=%d, request=%d, value=%d, index=%d, size=%d, timeout=%d) ret = %d ====\n", requesttype, request, value, index, size, timeout, ret);
		if ( ret > 0 ) {
			dump_bytes(bytes, ret);
			printf("====\n");
		}

		if ( getenv("USBSNIFF_WAIT") ) {
			printf("Press ENTER"); fflush(stdout); getchar();
		}

	}

	return ret;

}

int usb_set_configuration(usb_dev_handle *dev, int configuration) {

	static int (*real_usb_set_configuration)(usb_dev_handle *dev, int configuration) = NULL;

	if ( ! real_usb_set_configuration )
		real_usb_set_configuration = dlsym(RTLD_NEXT, "usb_set_configuration");

	printf("\n==== usb_set_configuration (configuration=%d) ====\n", configuration);

	return real_usb_set_configuration(dev, configuration);

}

int usb_claim_interface(usb_dev_handle *dev, int interface) {

	static int (*real_usb_claim_interface)(usb_dev_handle *dev, int interface) = NULL;

	if ( ! real_usb_claim_interface )
		real_usb_claim_interface = dlsym(RTLD_NEXT, "usb_claim_interface");

	printf("\n==== usb_claim_interface (interface=%d) ====\n", interface);

	return real_usb_claim_interface(dev, interface);

}

int usb_set_altinterface(usb_dev_handle *dev, int alternate) {

	static int (*real_usb_set_altinterface)(usb_dev_handle *dev, int alternate) = NULL;

	if ( ! real_usb_set_altinterface )
		real_usb_set_altinterface = dlsym(RTLD_NEXT, "usb_set_altinterface");

	printf("\n==== usb_set_altinterface (alternate=%d) ====\n", alternate);

	return real_usb_set_altinterface(dev, alternate);

}
