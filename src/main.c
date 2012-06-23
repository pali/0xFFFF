/*
 *  0xFFFF - Open Free Fiasco Firmware Flasher
 *  Copyright (C) 2007, 2008  pancake <pancake@youterm.com>
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
#include "query.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

/* global pr0n */
#if HAVE_USB
#include <usb.h>
struct usb_device *device  = NULL;
struct usb_dev_handle *dev = NULL;
#endif
char  *fiasco_image        = NULL;
char  *boot_cmdline        = NULL;
char  *reverseto           = NULL;
char  *subverstr           = NULL;
int    rd_mode             = -1;
int    rd_flags            = -1;
int    usb_mode            = -1;
int    root_device         = -1;
int    verbose             = 0;
int    identify            = 0;
int    moboreboot          = 0;
int    unpack              = 0;
int    qmode               = 0;
int    info                = 0;
int    nomode              = 0;

char *modes[]={
	"host",
	"peripheral",
	NULL
};

char *root_devices[] = {
	"flash",
	"mmc",
	"usb",
	NULL
};

void show_title()
{
	printf("0xFFFF v%s  // The Free Fiasco Firmware Flasher\n", VERSION);
}

void show_usage()
{
	int i;
	show_title();

#if 0
	// TODO: remove console: serial.c

#if HAVE_USB
	printf ("Over USB:\n"
		" -d [vid:pid]    injects a usb device into the supported list\n"
		" -l              list supported usb device ids\n"
		"\n"
		" -b [cmdline]    boot default or loaded kernel with cmdline\n"
		" -R              reboot device\n"
		" -l              load all specified images to RAM\n"
		" -f              flash all specified images\n"
		" -c              cold flash all specified images\n"
		"\n"
		);
#endif

	printf ("On device:\n"
		" -R              reboot device\n"
		" -f              flash all specified images\n"
		" -C [/dev/mtd]   check for bad blocks on mtd\n"
		" -e [dir]        dump all specified images to directory\n"
		"\n"
		);

	printf ("Device configuration:\n"
		" -i              show all device information\n"
		" -D [0|1|2]      sets root device to: flash (0), mmc (1) or usb (2)\n"
		" -U [0|1]        disable/enable USB host mode\n"
		" -r [0|1]        disable/enable R&D mode\n"
		" -f <flags>      set all R&D flags\n"
		" -H [rev]        set HW revision\n"
		" -S [ver]        set SW version\n"
		"\n"
		);

	printf ("Fiasco options:\n"
		" -u [dir]        unpack fiasco image to directory\n"
		" -g [ver]        generate fiasco image for SW version\n"
		"\n"
		);

	printf ("Image specification:\n"
		" -p [arg]        specify normal image\n"
		"                 arg is [[[dev:[hw:]]ver:]type:]file[%%layout]\n"
		"                   hw is device HW revision\n"
		"                   dev is device name string\n"
		"                   ver is image version string\n"
		"                   type is image type\n"
		"                   file is image file name\n"
		"                   layout is file name for layout file\n"
		" -F [file]       specify Fiasco image\n"
		" -t [type]       specify image type\n"
		" -s [ver]        specify image with version\n"
		" -d [dev]        specify image for device\n"
		" -o [hw]         specify image for HW revision\n"
		"\n"
		);

	printf( "Other options:\n"
		" -I [file]       identify a firmware image file\n"
		" -H [file]       calculate hash for file\n"
		" -c              console prompt mode\n"
		" -n              do not flash or write to disk (simulation)\n"
		" -v              be verbose and noisy\n"
		" -V              show 0xFFFF version information\n"
		" -h              show this help message\n"
		"\n"
		);

#if HAVE_SQUEUE
	printf( " -Q              enter shared queues server mode (for gui or remote)\n\n");
#endif

	printf( "R&D flags:\n"
		"  0x02 - disable OMAP watchdog\n"
		"  0x04 - disable RETU/EXT watchdog \n"
		"  0x08 - disable lifeguard reset\n"
		"  0x10 - enable serial console\n"
		"  0x20 - disable USB timeout\n"
		"  0x?? - enable STI console\n"
		"  0x?? - disable charging\n"
		"  0x?? - force power key boot reason\n"
		"\n"
		);

	printf( "Image types:\n");
	for(i=0;pieces[i];i++) printf("%s ", pieces[i]); printf("\n");

#endif

#if HAVE_USB
	printf ("Over USB:\n"
		" -b [arg]        boots the kernel with arguments\n"
		" -c              console prompt mode\n"
		" -d [vid:pid]    injects a usb device into the supported list\n"
		" -D [0|1|2]      sets the root device to flash (0), mmc (1) or usb (2)\n"
		" -f <flags>      set the given RD flags (see '-f help')\n"
		" -i              show device information (let standby mode)\n"
		" -l              list supported usb device ids\n"
		" -p format       piece-of-firmware\n"
		"                 piece format: [[[[dev:[hw:]]ver:]type:]file[%%layout]\n"
		"                   hw is device HW revision\n"
		"                   dev is device name string\n"
		"                   ver is image version string\n"
		"                   type is image type\n"
		"                   file is image file name\n"
		"                   layout is file name for layout file\n"
		" -r [0|1]        disable/enable R&D mode\n"
		" -R              reboot the omap board\n"
		" -U [0|1]        disable/enable the usb host mode\n");
#endif
#if HAVE_SQUEUE
	printf(" -Q              enter shared queues server mode (for gui or remote)\n");
#endif
	printf ("Local stuff:\n"
		" -s [serial]     serial port console (minicom like terminal)\n"
		" -h              show this help message\n"
		" -S [subversion] unpacks/flash pieces matching this sub-version information\n"
		" -n              do not flash or write to disk (simulation)\n"
		" -C [/dev/mtd]   check bad blocks on mtd\n"
		" -e [path]       dump/extract pieces to path\n"
		" -F [fiasco]     flash a fiasco firmware image\n"
		" -H [file]       calculate hash for file\n"
		" -I [piece]      identify a firmware piece\n"
		" -P [new-fiasco] creates a new fiasco package, pieces as arguments\n"
		"                   optional argument 'version:<version>'\n"
		" -u [fiasco]     unpack target fiasco image\n"
		" -v              be verbose and noisy\n"
		" -V              show 0xFFFF version information\n"
		" -x              extract configuration entries from /dev/mtd1\n"
		"Pieces are: ");
	for(i=0;pieces[i];i++) printf("%s ", pieces[i]); printf("\n");
	// serial port support is not yet done (cold flash is for flashing the 8kB nand)
	// TODO: commandline shell prompt for nolo comm

	exit(0);
}

int unpack_callback(struct header_t *header)
{
	FILE *fd;

	fiasco_data_read(header);
	if (subverstr) {
		if (strchr(header->name, ',') != NULL) {
			if (!strstr(header->name, subverstr)) {
				printf("Skipping '%s' does not matches -S subversion\n",
						header->name);
				return 1;
			}
		}
	}
	if (nomode)
		return 1;

	fd = fopen(header->name, "wb");
	if (fd == NULL) {
		printf("Cannot open file.\n");
		return 1;
	}
	if (fwrite(header->data, header->size, 1, fd) != 1)
		printf("Writing failed\n");
	fclose(fd);

	return 0;
}

int flash_callback(struct header_t *header)
{
	int ret;
	char *type;

	ret = unpack_callback(header);
	if (ret) {
		printf("ignored\n");
		return 1;
	}

	type = header->type;
	if (!type[0])
		type = (char *)fpid_file(header->name);
	printf("\nFlashing %s (%s)\n", header->name, type);
	flash_image(header->name, type, header->device, header->hwrevs, header->version);

	return 0;
}

void unpack_fiasco_image(char *file)
{
	printf("Dumping firmware pieces to disk.\n");
	fiasco_callback = &unpack_callback;
	openfiasco(file, NULL ,1);
}

int fiasco_flash(const char *file)
{
	char *p;
	char version[64];

	if (connect_via_usb()) {
		fprintf(stderr, "Cannot connect to device. It is possibly not in boot stage.\n");
		return 0;
	}

	// if (info)
	cmd_info("");

	check_nolo_order();
	get_nolo_version();
	get_sw_version();
	get_hw_revision(version, 44);

	if (subverstr == NULL) {
		p = strstr(version, "hw_rev:");
		if (p) {
			subverstr = strdup(p+7);
			// TODO: delimit string by comma
			printf("SubVersionString autodetected: '%s'\n", subverstr);
		}
	}
	printf("\n");

	fiasco_callback = &flash_callback;
	openfiasco(file, "xloader", 0);
	openfiasco(file, "secondary", 0);
	openfiasco(file, "kernel", 0);
	openfiasco(file, "initfs", 0);
	openfiasco(file, "rootfs", 0);

	return 0;
}

#if HAVE_USB
int connect_via_usb()
{
	static char pbc[]={'/','-','\\', '|'};
	struct usb_device_descriptor udd;
	struct devices it_device;
	int    c = 0;

	// usb_set_debug(5);
	usb_init();

	/* Tries to get access to the Internet Tablet and retries
	 * if any of the neccessary steps fail.
	 *
	 * Note: While a proper device may be found on the bus it may
	 * not be in the right state to be accessed (e.g. the Nokia is
	 * not in the boot stage any more).
	 */
	while(!dev) {
		usleep(0xc350); // 0.5s

		if(!usb_device_found(&udd, &it_device)) {
			printf("\rWaiting for device... %c", pbc[++c%4]);
			fflush(stdout);
			continue;
		}

		/* open device */
		if(!(dev = usb_open(device))) {
			perror("usb_open");
			return 1;
		}

#if 1
{
char name[32];
name[0] = '\0';
usb_get_driver_np(dev, 2, name, sizeof(name));
printf ("Driver: %s\n", name);
usb_detach_kernel_driver_np(dev, 2);
}
#endif
		if (usb_claim_interface(dev, 2) < 0) {
			//device->config->interface->altsetting->bInterfaceNumber) < 0) { // 2 or 0
			perror("usb_claim_interface");

			// Something is broken if closing fails.
			if(usb_close(dev)) {
				perror("usb_close");
				return 1;
			}
			dev = NULL;
			// Try again later.?
			exit(1);
		}

		if (usb_set_altinterface(dev, 1) <0) { //device->config->interface->altsetting->bAlternateSetting) < 0) {
			perror("usb_set_altinterface");

			// Something is broken if closing fails.
			if(usb_close(dev)) {
				perror("usb_close");
				return 1;
			}

			dev = NULL;
			// Try again later.
			continue;
		}
		break;
	}

	printf("found %s (%04x:%04x)\n", it_device.name,
		it_device.vendor_id, it_device.product_id);

	/* go go go! */
	while(get_status());

	sleep(1); // take breath

	return 0;
}
#endif

int main(int argc, char **argv)
{
	int c;
	const char *type;

	while((c = getopt(argc, argv, "QC:cp:PvVhRu:ib:U:r:e:ld:I:D:f:F:s:xH:S:n")) != -1) {
		switch(c) {
		case 'H':
			printf("xorpair: %04x\n", do_hash_file(optarg, NULL));
			return 0;
		case 'x':
			return dump_config();
#if HAVE_USB
		case 'c':
			return console_prompt();
		case 'b':
			boot_cmdline = optarg;
			break;
		case 'S':
			subverstr = optarg;
			break;
		case 'n':
			nomode = 1;
			break;
		case 'U':
			usb_mode = atoi(optarg);
			break;
		case 'F':
			fiasco_image = optarg;
			break;
		case 'd':
			sscanf(optarg, "%04hx:%04hx", 
					&supported_devices[SUPPORTED_DEVICES-2].vendor_id,
					&supported_devices[SUPPORTED_DEVICES-2].product_id);
			supported_devices[SUPPORTED_DEVICES-2].name = strdup("user");
			break;
		case 'D':
			root_device = atoi(optarg);
			break;
		case 'f':
			if (!strcmp(optarg,"help")) {
				printf("* Flags are composed of:\n");
				printf("  0x02 - disable OMAP watchdog (possibly)\n");
				printf("  0x04 - disable RETU watchdog (possibly)\n");
				printf("  0x08 - disable lifeguard reset\n");
				printf("  0x10 - enable serial console\n");
				printf("  0x20 - disable USB timeout\n");
				exit(1);
			}
			rd_flags = (unsigned short) strtoul(optarg, NULL, 16);
			break;
		case 'r':
			rd_mode = atoi(optarg);
			break;
		case 'l':
			list_valid_devices();
			return 0;
		case 'p':
			add_piece(optarg);
			break;
		case 'i':
			info = 1;
			break;
		case 'R':
			moboreboot = 1;
			break;
#endif
		case 'e':
			reverseto = optarg;
			break;
		case 's':
			return console(optarg);
		case 'u':
			fiasco_image = optarg;
			unpack = 1;
			break;
		case 'Q':
			qmode = 1;
			break;
		case 'P':
			return fiasco_pack(optind, argv);
		case 'I':
			type = fpid_file(optarg);
			printf("%s: %s\n", type, optarg);
			if (type && strcmp(type, "fiasco") == 0)
				openfiasco(optarg, NULL, 1);
			identify = 1;
			break;
		case 'C':
			return check_badblocks(optarg);
		case 'v':
			verbose = 1;
			break;
		case 'h':
			show_usage();
			break;
		case 'V':
			printf("%s\n", VERSION);
			return 0;
		}
	}

	if (qmode)
		return queue_mode();

	if (!unpack && fiasco_image)
		return fiasco_flash(fiasco_image);

	if (identify)
		return 0;

	// flags ok?
	if (	(fiasco_image == NULL)
	&&	(boot_cmdline == NULL)
	&&	(reverseto    == NULL)
	&&	(pcs_n        == 0)
	&&	(rd_flags     == -1)
	&&	(rd_mode      == -1)
	&&	(info         == 0)
	&&	(moboreboot   == 0)
	&&	(usb_mode     == -1)
	&& 	(root_device  == -1))
	{

		printf("# The Free Fiasco Firmware Flasher v"VERSION"\n"
				"0xFFFF [-chinlQRvVx] [-C mtd-dev] [-d vid:pid] [-D 0|1|2] [-e path] [-f flags]\n"
				"       [-F fiasco] [-H hash-file] [-I piece] [-p [piece%%]file]] [-r 0|1] [-S subver]\n"
				"       [-s serial-dev] [-u fiasco-image] [-U 0|1] | [-P new-fiasco] [piece1] [2] ..\n");
		return 1;
	}

	if (unpack) {
		if (reverseto) {
			if (chdir(reverseto) < 0) {
				printf("Error: Cannot change directory to %s\n", reverseto);
				return 1;
			}
		}
		unpack_fiasco_image(fiasco_image);
		return 0;
	}

	if (reverseto) {
		reverse_extract_pieces(reverseto);
		return 0;
	}

#if HAVE_USB
	if (connect_via_usb()) {
		fprintf(stderr, "Cannot connect to device. It is possibly not in boot stage.\n");
		return 0;
	}

	// if (info)
	cmd_info("");

	if (pcs_n) {
		char version[64];
		check_nolo_order();
		get_sw_version();
		get_hw_revision(version, 44);
		if (subverstr == NULL) {
			char *p = strstr(version, "hw_rev:");
			if (p) {
				subverstr = strdup(p+7);
				// TODO: delimit string by comma
				printf("SubVersionString autodetected: '%s'\n", subverstr);
			}
		}
		get_nolo_version();

		for(c=0;c<pcs_n;c++) {
			printf("Flashing %s (%s)\n", pcs[c].type, pcs[c].name);
			flash_image(pcs[c].name, pcs[c].type, pcs[c].device, pcs[c].hwrevs, pcs[c].version);
		}
	}

	if (rd_mode != -1)
		set_rd_mode(rd_mode);

	if (rd_flags != -1)
		set_rd_flags(rd_flags);

	if (root_device != -1)
		set_root_device(root_device);

	if (usb_mode != -1)
		set_usb_mode(usb_mode);

	if (boot_cmdline)
		boot_board(boot_cmdline);

	if (moboreboot)
		reboot_board();
#endif

	return 0;
}
