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
#include "query.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

/* global pr0n */
struct usb_device *device  = NULL;
struct usb_dev_handle *dev = NULL;
char  *fiasco_image        = NULL;
char  *boot_cmdline        = NULL;
char  *reverseto           = NULL;
int    rd_mode             = -1;
int    rd_flags            = -1;
int    usb_mode            = -1;
int    root_device         = -1;
int    verbose             = 0;
int    identify            = 0;
int    reboot              = 0;
int    unpack              = 0;
int    info                = 0;

/* global structs */
char *pieces[] = {
	"xloader",    // xloader.bin
	"2nd",        // 2nd
	"secondary",  // secondary.bin
	"kernel",     // zImage
	"initfs",     // jffs'd initfs
	"rootfs",     // 80mB of blob
	"omap-nand",  // 8kB of food for the nand
	NULL
};

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
	printf(" -b [arg]        boots the kernel with arguments\n");
	printf(" -c              console prompt mode\n");
	printf(" -C [/dev/mtd]   check bad blocks on mtd\n");
	printf(" -d [vid:pid]    injects a usb device into the supported list\n");
	printf(" -D [0|1|2]      sets the root device to flash (0), mmc (1) or usb (2)\n");
	printf(" -e [path]       dump and extract pieces to path\n");
	printf(" -f <flags>      set the given RD flags (see '-f help')\n");
	printf(" -F [fiasco]     flash a fiasco firmware image\n");
	printf(" -h              show this help message\n");
	printf(" -H [file]       calculate hash for file\n");
	printf(" -i              show device information (let standby mode)\n");
	printf(" -I [piece]      identify a firmware piece\n");
	printf(" -l, -L          list supported usb device ids\n");
	printf(" -p [[p%%]file]  piece-of-firmware %% file-where-this-piece-is\n");
	printf(" -P [new-fiasco] creates a new fiasco package, pieces as arguments\n");
	printf(" -r [0|1]        disable/enable R&D mode\n");
	printf(" -R              reboot the omap board\n");
	printf(" -s [serial]     serial port console (minicom like terminal)\n");
	printf(" -u [fiasco]     unpack target fiasco image\n");
	printf(" -U [0|1]        disable/enable the usb host mode\n");
	printf(" -v              be verbose and noisy\n");
	printf(" -V              show 0xFFFF version information\n");
	printf(" -x              extract configuration entries from /dev/mtd1\n");
	printf("Pieces are: ");
	for(i=0;pieces[i];i++) printf("%s ", pieces[i]); printf("\n");
	// serial port support is not yet done (cold flash is for flashing the 8kB nand)
	// TODO: commandline shell prompt for nolo comm

	exit(0);
}


void unpack_callback(struct header_t *header)
{
	FILE *fd = fopen(header->name, "wb");
	if (fd == NULL) {
		printf("Cannot open file.\n");
		return;
	}
	fiasco_data_read(header);
	fwrite(header->data, header->size, 1, fd);
	fclose(fd);
}

void unpack_fiasco_image(char *file)
{
	printf("Dumping firmware pieces to disk.\n");
	fiasco_callback = &unpack_callback;
	openfiasco( file );
}

int fiasco_flash(char *file)
{
	/* TODO */
	fiasco_callback = NULL;
	openfiasco( file );

	printf("\nTODO: Implement the fiasco flashing here.\n");
	return -1;
}

int connect_via_usb()
{
        struct usb_device_descriptor udd;
        struct devices it_device;
	int    c = 0;
	static char pbc[]={'/','-','\\', '|'};

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

        	if ( usb_claim_interface(dev, 2) < 0) { // 2 or 0
	        	D perror("usb_claim_interface");

                        // Something is broken if closing fails.
                        if(usb_close(dev)) {
                          perror("usb_close");
                          return 1;
                        }

                        dev = NULL;

                        // Try again later.
                        continue;
        	}

        	if (usb_set_altinterface(dev, 1) < 0) {
	        	D perror("usb_set_altinterface");

                        // Something is broken if closing fails.
                        if(usb_close(dev)) {
                          perror("usb_close");
                          return 1;
                        }

                        dev = NULL;
                        // Try again later.
                        continue;
        	}
        }

        printf("found %s (%04x:%04x)\n", it_device.name,
		it_device.vendor_id, it_device.product_id);

	/* go go go! */
	while(get_status());

	sleep(1); // take breath

	return 0;
}

int main(int argc, char **argv)
{
	int c;

	while((c = getopt(argc, argv, "C:cp:PvVhRu:ib:U:r:e:Lld:I:D:f:F:s:xH:")) != -1) {
		switch(c) {
		case 'H':
			printf("xorpair: %04x\n", do_hash_file(optarg));
			return 0;
		case 'x':
			return dump_config();
		case 'c':
			return console_prompt();
		case 'F':
			return fiasco_flash(optarg);
		case 'd':
			sscanf(optarg, "%04hx:%04hx", 
				&supported_devices[SUPPORTED_DEVICES-2].vendor_id,
				&supported_devices[SUPPORTED_DEVICES-2].product_id);
			supported_devices[SUPPORTED_DEVICES-2].name = strdup("user");
			break;
		case 'D':
			root_device = atoi(optarg);
			break;
		case 'e':
			reverseto = optarg;
			break;
		case 's':
			return console(optarg);
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
		case 'U':
			usb_mode = atoi(optarg);
			break;
		case 'r':
			rd_mode = atoi(optarg);
			break;
		case 'b':
			boot_cmdline = optarg;
			break;
		case 'u':
			fiasco_image = optarg;
			unpack = 1;
			break;
		case 'p':
			add_piece(optarg);
			break;
		case 'P':
			return fiasco_pack(optind, argv);
		case 'L':
		case 'l':
			list_valid_devices();
			return 0;
		case 'I':
			printf("%s: %s\n", fpid_file(optarg), optarg);
			identify = 1;
			break;
		case 'C':
			return check_badblocks(optarg);
		case 'i':
			info = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'h':
			show_usage();
			break;
		case 'V':
			printf("%s\n", VERSION);
			return 0;
		case 'R':
			reboot = 1;
			break;
		}
	}

	if (identify)
		return 0;

	// flags ok?
	if (    (fiasco_image == NULL)
	&&	(boot_cmdline == NULL)
	&&	(reverseto    == NULL)
	&&	(pcs_n        == 0)
	&&	(rd_flags     == -1)
	&&	(rd_mode      == -1)
	&&	(info         == 0)
	&&	(reboot       == 0)
	&&	(usb_mode     == -1)
	&& 	(root_device  == -1))
	{
		printf("0xFFFF [-chiLRvVx] [-C mtd-dev] [-d vid:pid] [-D 0|1|2] [-e path] [-f flags]\n");
		printf("       [-F fiasco] [-H hash-file] [-I piece] [-p [piece%%]file]] [-r 0|1]\n");
		printf("       [-s serial-dev] [-u fiasco-image] [-U 0|1] | [-P new-fiasco] [piece1] [2] ..\n");
		return 1;
	}

	if (unpack) {
		unpack_fiasco_image(fiasco_image);
		return 0;
	}

	if (reverseto) {
		reverse_extract_pieces(reverseto);
		return 0;
	}

        if (connect_via_usb()) {
                fprintf(stderr, "Cannot connect to device. It is possibly not in boot stage.\n");
                return 0;
        }

	// if (info)
	cmd_info("");
	
	if (pcs_n) {
		int c;

		check_nolo_order();
		get_sw_version();
		get_nolo_version();

		for(c=0;c<pcs_n;c++) {
			printf("Flashing %s (%s)\n", pcs[c].type, pcs[c].name);
			flash_image(pcs[c].name, pcs[c].type, pcs[c].vers);
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

	if (reboot)
		reboot_board();

	return 0;
}
