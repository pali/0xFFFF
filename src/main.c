/*
    0xFFFF - Open Free Fiasco Firmware Flasher
    Copyright (C) 2007, 2008  pancake <pancake@youterm.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>

#include "global.h"

#include "image.h"
#include "fiasco.h"
#include "device.h"
#include "operations.h"

extern char *optarg;
extern int optind, opterr, optopt;

static void show_title(void) {
	printf("0xFFFF v%s  // Open Free Fiasco Firmware Flasher\n", VERSION);
}

static void show_usage(void) {

	int i;
	printf (""

		"Operations:\n"
		" -b [cmdline]    boot default or loaded kernel (default: no cmdline)\n"
		" -b update       boot default or loaded kernel to Update mode\n"
		" -r              reboot device\n"
		" -l              load kernel and initfs images to RAM\n"
		" -f              flash all specified images\n"
		" -c              cold flash 2nd and secondary images\n"
		" -x [/dev/mtd]   check for bad blocks on mtd device (default: all)\n"
		" -E file         dump all device images to one fiasco image\n"
		" -e [dir]        dump all device images (or one -t) to directory (default: current)\n"
		"\n"

		"Device configuration:\n"
		" -I              identify, show all information about device\n"
		" -D 0|1|2        change root device: 0 - flash, 1 - mmc, 2 - usb\n"
		" -U 0|1          disable/enable USB host mode\n"
		" -R 0|1          disable/enable R&D mode\n"
		" -F flags        change R&D flags, flags are comma separated list, can be empty\n"
		" -H rev          change HW revision\n"
		" -N ver          change NOLO version string\n"
		" -K ver          change kernel version string\n"
		" -T ver          change initfs version string\n"
		" -S ver          change SW release version string\n"
		" -C ver          change content eMMC version string\n"
		"\n"

		"Input image specification:\n"
		" -M file         specify fiasco image\n"
		" -m arg          specify normal image\n"
		"                 arg is [[[dev:[hw:]]ver:]type:]file[%%lay]\n"
		"                   dev is device name string (default: empty)\n"
		"                   hw are comma separated list of HW revisions (default: empty)\n"
		"                   ver is image version string (default: empty)\n"
		"                   type is image type (default: autodetect)\n"
		"                   file is image file name\n"
		"                   lay is layout file name (default: none)\n"
		"\n"

		"Image filters:\n"
		" -t type         filter images by type\n"
		" -d dev          filter images by device\n"
		" -w hw           filter images by HW revision\n"
		"\n"

		"Fiasco image:\n"
		" -u [dir]        unpack fiasco image to directory (default: current)\n"
		" -g file[%%sw]    generate fiasco image with SW rel version (default: no version)\n"
		"\n"

		"Other options:\n"
		" -i              identify images\n"
		" -s              simulate, do not flash or write on disk\n"
		" -n              disable hash, checksum and image type checking\n"
		" -v              be verbose and noisy\n"
		" -h              show this help message\n"
		"\n"

		"R&D flags:\n"
		"  no-omap-wd          disable auto reboot by OMAP watchdog\n"
		"  no-ext-wd           disable auto reboot by external watchdog\n"
		"  no-lifeguard-reset  disable auto reboot by software lifeguard\n"
		"  serial-console      enable serial console\n"
		"  no-usb-timeout      disable usb timeout for flashing\n"
		"  sti-console         enable sti console\n"
		"  no-charging         disable battery charging\n"
		"  force-power-key     force omap boot reason to power key\n"
		"\n"

	);

	printf( "Supported devices:\n");
	for ( i = 0; i < DEVICE_COUNT; ++i )
		if ( device_to_string(i) && device_to_long_string(i) )
			printf("  %-14s %s\n", device_to_string(i), device_to_long_string(i));
	printf( "\n");

	printf( "Supported image types:\n");
	for ( i = 0; i < IMAGE_COUNT; ++i )
		if ( image_type_to_string(i) )
			printf("  %s\n", image_type_to_string(i));
	printf( "\n");

	printf( "Supported connection protocols:\n");
	printf( "  Local on device\n");
	for ( i = 0; i < FLASH_COUNT; ++i )
		if ( usb_flash_protocol_to_string(i) )
			printf("  %s via USB\n", usb_flash_protocol_to_string(i));
	printf( "\n");

}

int simulate;
int noverify;
int verbose;

/* arg = [[[dev:[hw:]]ver:]type:]file[%%lay] */
static void parse_image_arg(char * arg, struct image_list ** image_first) {

	struct stat st;
	struct image * image;
	char * file;
	char * type;
	char * device;
	char * hwrevs;
	char * version;
	char * layout;
	char * layout_file;
	int fd;

	/* First check if arg is file, then try to parse arg format */
	fd = open(arg, O_RDONLY);
	if ( fd >= 0 ) {
		if ( fstat(fd, &st) == 0 && !S_ISDIR(st.st_mode) ) {
			image = image_alloc_from_fd(fd, arg, NULL, NULL, NULL, NULL, NULL);
			if ( ! image ) {
				ERROR("Cannot load image file %s", arg);
				exit(1);
			}
			image_list_add(image_first, image);
			return;
		}
		close(fd);
	} else if ( errno != ENOENT ) {
		ERROR("Cannot load image file %s", arg);
		exit(1);
	}

	layout_file = strchr(arg, '%');
	if ( layout_file )
		*(layout_file++) = 0;

	type = NULL;
	device = NULL;
	hwrevs = NULL;
	version = NULL;
	layout = NULL;

	file = strrchr(arg, ':');
	if ( file ) {
		*(file++) = 0;
		type = strrchr(arg, ':');
		if ( type ) {
			*(type++) = 0;
			version = strrchr(arg, ':');
			if ( version ) {
				*(version++) = 0;
				hwrevs = strchr(arg, ':');
				if ( hwrevs )
					*(hwrevs++) = 0;
				device = arg;
			} else {
				version = arg;
			}
		} else {
			type = arg;
		}
	} else {
		file = arg;
	}

	if ( layout_file ) {
		off_t len;
		int fd = open(layout_file, O_RDONLY);
		if ( fd < 0 ) {
			ERROR_INFO("Cannot open layout file %s", layout_file);
			exit(1);
		}
		len = lseek(fd, 0, SEEK_END);
		if ( len == (off_t)-1 ) {
			ERROR_INFO("Cannot get size of file %s", layout_file);
			exit(1);
		}
		if ( lseek(fd, 0, SEEK_SET) == (off_t)-1 ) {
			ERROR_INFO("Cannot seek to begin of file %s", layout_file);
			exit(1);
		}
		layout = malloc(len+1);
		if ( ! layout ) {
			ALLOC_ERROR();
			exit(1);
		}
		if ( read(fd, layout, len) != len ) {
			ERROR_INFO("Cannot read %ju bytes from layout file %s", (intmax_t)len, layout_file);
			exit(1);
		}
		layout[len] = 0;
		close(fd);
	}

	image = image_alloc_from_file(file, type, device, hwrevs, version, layout);

	if ( layout )
		free(layout);

	if ( ! image ) {
		ERROR("Cannot load image file %s", file);
		exit(1);
	}

	image_list_add(image_first, image);

}

void filter_images_by_type(enum image_type type, struct image_list ** image_first) {

	struct image_list * image_ptr = *image_first;
	while ( image_ptr ) {
		struct image_list * next = image_ptr->next;
		if ( image_ptr->image->type != type ) {
			if ( image_ptr == *image_first )
				*image_first = next;
			image_list_del(image_ptr);
		}
		image_ptr = next;
	}

}

void filter_images_by_device(enum device device, struct image_list ** image_first) {

	struct image_list * image_ptr = *image_first;
	while ( image_ptr ) {
		struct image_list * next = image_ptr->next;
		struct device_list * device_ptr = image_ptr->image->devices;
		int match = 0;
		while ( device_ptr ) {
			if ( device_ptr->device == device || device_ptr->device == DEVICE_ANY ) {
				match = 1;
				break;
			}
			device_ptr = device_ptr->next;
		}
		if ( ! match ) {
			if ( image_ptr == *image_first )
				*image_first = next;
			image_list_del(image_ptr);
		}
		image_ptr = next;
	}

}

void filter_images_by_hwrev(int16_t hwrev, struct image_list ** image_first) {

	struct image_list * image_ptr = *image_first;
	while ( image_ptr ) {
		struct image_list * next = image_ptr->next;
		if ( ! image_hwrev_is_valid(image_ptr->image, hwrev) ) {
			if ( image_ptr == *image_first )
				*image_first = next;
			image_list_del(image_ptr);
		}
		image_ptr = next;
	}

}

static const char * image_tmp[] = {
	[IMAGE_XLOADER] = "xloader_tmp",
	[IMAGE_SECONDARY] = "secondary_tmp",
	[IMAGE_KERNEL] = "kernel_tmp",
	[IMAGE_INITFS] = "initfs_tmp",
	[IMAGE_ROOTFS] = "rootfs_tmp",
	[IMAGE_MMC] = "mmc_tmp",
};

static const char * image_tmp_name(enum image_type type) {

	if ( type >= sizeof(image_tmp)/sizeof(image_tmp[0]) )
		return NULL;

	if ( ! image_tmp[type] || ! image_tmp[type][0] )
		return NULL;

	return image_tmp[type];

}

int main(int argc, char **argv) {

	const char * optstring = ":"
	"b:rlfcx:E:e:"
	"ID:U:R:F:H:K:T:N:S:C:"
	"M:m:"
	"t:d:w:"
	"u:g:"
	"i"
	"p"
	"Q"
	"snvh"
	"";
	int c;

	int ret = 0;
	int do_something = 0;
	int do_device = 0;

	int dev_boot = 0;
	char * dev_boot_arg = NULL;
	int dev_load = 0;
	int dev_cold_flash = 0;

	int dev_check = 0;
	char * dev_check_arg = NULL;
	int dev_dump_fiasco = 0;
	char * dev_dump_fiasco_arg = NULL;
	int dev_dump = 0;
	char * dev_dump_arg = NULL;

	int dev_flash = 0;
	int dev_reboot = 0;
	int dev_ident = 0;

	int set_root = 0;
	char * set_root_arg = NULL;
	int set_usb = 0;
	char * set_usb_arg = NULL;
	int set_rd = 0;
	char * set_rd_arg = NULL;
	int set_rd_flags = 0;
	char * set_rd_flags_arg = NULL;
	int set_hw = 0;
	char * set_hw_arg = NULL;
	int set_kernel = 0;
	char * set_kernel_arg = NULL;
	int set_initfs = 0;
	char * set_initfs_arg = NULL;
	int set_nolo = 0;
	char * set_nolo_arg = NULL;
	int set_sw = 0;
	char * set_sw_arg = NULL;
	int set_emmc = 0;
	char * set_emmc_arg = NULL;

	int image_fiasco = 0;
	char * image_fiasco_arg = NULL;

	int filter_type = 0;
	char * filter_type_arg = NULL;
	int filter_device = 0;
	char * filter_device_arg = NULL;
	int filter_hwrev = 0;
	char * filter_hwrev_arg = NULL;

	int fiasco_un = 0;
	char * fiasco_un_arg = NULL;
	int fiasco_gen = 0;
	char * fiasco_gen_arg = NULL;

	int image_ident = 0;

	int help = 0;

	struct image_list * image_first = NULL;
	struct image_list * image_ptr = NULL;

	int have_2nd = 0;
	int have_secondary = 0;
	int have_kernel = 0;
	int have_initfs = 0;
	struct image * image_2nd = NULL;
	struct image * image_secondary = NULL;
	struct image_list * image_kernel = NULL;
	struct image_list * image_initfs = NULL;

	struct fiasco * fiasco_in = NULL;
	struct fiasco * fiasco_out = NULL;

	struct device_info * dev = NULL;

	enum device detected_device = DEVICE_UNKNOWN;
	int16_t detected_hwrev = -1;

	int i;
	char buf[512];
	char * ptr = NULL;
	char * tmp = NULL;

	char nolo_ver[512];
	char kernel_ver[512];
	char initfs_ver[512];
	char sw_ver[512];
	char content_ver[512];

	simulate = 0;
	noverify = 0;
	verbose = 0;

	show_title();

	opterr = 0;

	while ( ( c = getopt(argc, argv, optstring) ) != -1 ) {

		switch (c) {

			default:
				ERROR("Unknown option '%c'", c);
				ret = 1;
				goto clean;

			case '?':
				ERROR("Unknown option '%c'", optopt);
				ret = 1;
				goto clean;

			case ':':
				if ( optopt == 'b' ) {
					dev_boot = 1;
					break;
				}
				if ( optopt == 'e' ) {
					dev_dump = 1;
					break;
				}
				if ( optopt == 'u' ) {
					fiasco_un = 1;
					break;
				}
				if ( optopt == 'x' ) {
					dev_check = 1;
					break;
				}
				ERROR("Option '%c' requires an argument", optopt);
				ret = 1;
				goto clean;

			case 'b':
				dev_boot = 1;
				if ( optarg[0] != '-' )
					dev_boot_arg = optarg;
				else
					--optind;
				break;
			case 'l':
				dev_load = 1;
				break;
			case 'c':
				dev_cold_flash = 1;
				break;

			case 'x':
				dev_check = 1;
				if ( optarg[0] != '-' )
					dev_check_arg = optarg;
				else
					--optind;
				break;
			case 'E':
				dev_dump_fiasco = 1;
				dev_dump_fiasco_arg = optarg;
				break;
			case 'e':
				dev_dump = 1;
				if ( optarg[0] != '-' )
					dev_dump_arg = optarg;
				else
					--optind;
				break;

			case 'f':
				dev_flash = 1;
				break;
			case 'r':
				dev_reboot = 1;
				break;

			case 'I':
				dev_ident = 1;
				break;
			case 'D':
				set_root = 1;
				set_root_arg = optarg;
				break;
			case 'U':
				set_usb = 1;
				set_usb_arg = optarg;
				break;
			case 'R':
				set_rd = 1;
				set_rd_arg = optarg;
				break;
			case 'F':
				set_rd_flags = 1;
				set_rd_flags_arg = optarg;
				break;
			case 'H':
				set_hw = 1;
				set_hw_arg = optarg;
				break;
			case 'K':
				set_kernel = 1;
				set_kernel_arg = optarg;
				break;
			case 'T':
				set_initfs = 1;
				set_initfs_arg = optarg;
				break;
			case 'N':
				set_nolo = 1;
				set_nolo_arg = optarg;
				break;
			case 'S':
				set_sw = 1;
				set_sw_arg = optarg;
				break;
			case 'C':
				set_emmc = 1;
				set_emmc_arg = optarg;
				break;

			case 'M':
				image_fiasco = 1;
				image_fiasco_arg = optarg;
				break;
			case 'm':
				parse_image_arg(optarg, &image_first);
				break;

			case 't':
				filter_type = 1;
				filter_type_arg = optarg;
				break;
			case 'd':
				filter_device = 1;
				filter_device_arg = optarg;
				break;
			case 'w':
				filter_hwrev = 1;
				filter_hwrev_arg = optarg;
				break;

			case 'u':
				fiasco_un = 1;
				fiasco_un_arg = optarg;
				break;
			case 'g':
				fiasco_gen = 1;
				if ( optarg[0] != '-' )
					fiasco_gen_arg = optarg;
				else
					--optind;
				break;

			case 'i':
				image_ident = 1;
				break;

			case 's':
				simulate = 1;
				break;
			case 'n':
				noverify = 1;
				break;
			case 'v':
				verbose = 1;
				break;

			case 'h':
				help = 1;
				break;

		}

	}

	if ( optind < argc ) {
		ERROR("Extra argument '%s'", argv[optind]);
		ret = 1;
		goto clean;
	}

	if ( dev_boot || dev_reboot || dev_load || dev_flash || dev_cold_flash || dev_ident || dev_check || dev_dump_fiasco || dev_dump
		|| set_root || set_usb || set_rd || set_rd_flags || set_hw || set_kernel || set_initfs || set_nolo || set_sw || set_emmc )
		do_device = 1;

	if ( dev_boot || dev_load || dev_cold_flash )
		do_something = 1;
	if ( dev_check || dev_dump_fiasco || dev_dump )
		do_something = 1;
	if ( dev_flash || dev_reboot || dev_ident || set_root || set_usb || set_rd || set_rd_flags || set_hw || set_kernel || set_initfs || set_nolo || set_sw || set_emmc )
		do_something = 1;
	if ( fiasco_un || fiasco_gen || image_ident )
		do_something = 1;
	if ( help )
		do_something = 1;

	if ( ! do_something ) {
		ERROR("Nothing to do");
		ret = 1;
		goto clean;
	}

	printf("\n");

	/* help */
	if ( help ) {
		show_usage();
		ret = 0;
		goto clean;
	}

	/* load images from files */
	if ( image_first && image_fiasco ) {
		ERROR("Cannot specify normal and fiasco images together");
		ret = 1;
		goto clean;
	}

	/* load fiasco image */
	if ( image_fiasco ) {
		fiasco_in = fiasco_alloc_from_file(image_fiasco_arg);
		if ( ! fiasco_in ) {
			ERROR("Cannot load fiasco image file %s", image_fiasco_arg);
			ret = 1;
			goto clean;
		}
		image_first = fiasco_in->first;
	}

	/* filter images by type */
	if ( filter_type ) {
		enum image_type type = image_type_from_string(filter_type_arg);
		if ( ! type ) {
			ERROR("Specified unknown image type for filtering: %s", filter_type_arg);
			ret = 1;
			goto clean;
		}
		filter_images_by_type(type, &image_first);
		/* make sure that fiasco_in has valid images */
		if ( fiasco_in )
			fiasco_in->first = image_first;
	}

	/* filter images by device */
	if ( filter_device ) {
		enum device device = device_from_string(filter_device_arg);
		if ( ! device ) {
			ERROR("Specified unknown device for filtering: %s", filter_device_arg);
			ret = 1;
			goto clean;
		}
		filter_images_by_device(device, &image_first);
		/* make sure that fiasco_in has valid images */
		if ( fiasco_in )
			fiasco_in->first = image_first;
	}

	/* filter images by hwrev */
	if ( filter_hwrev ) {
		filter_images_by_hwrev(atoi(filter_hwrev_arg), &image_first);
		/* make sure that fiasco_in has valid images */
		if ( fiasco_in )
			fiasco_in->first = image_first;
	}

	/* reorder images for flashing (first x-loader, second secondary) */
	/* set 2nd and secondary images for cold-flashing */
	if ( dev_flash || dev_cold_flash ) {

		struct image_list * image_unorder_first;

		image_unorder_first = image_first;
		image_first = NULL;

		image_ptr = image_unorder_first;
		while ( image_ptr ) {
			struct image_list * next = image_ptr->next;
			if ( image_ptr->image->type == IMAGE_XLOADER ) {
				if ( image_ptr == image_unorder_first )
					image_unorder_first = next;
				image_list_add(&image_first, image_ptr->image);
				image_list_unlink(image_ptr);
				free(image_ptr);
			}
			image_ptr = next;
		}

		image_ptr = image_unorder_first;
		while ( image_ptr ) {
			struct image_list * next = image_ptr->next;
			if ( image_ptr->image->type == IMAGE_SECONDARY ) {
				if ( have_secondary == 0 ) {
					image_secondary = image_ptr->image;
					have_secondary = 1;
				} else if ( have_secondary == 1 ) {
					image_secondary = NULL;
					have_secondary = 2;
				}
				if ( image_ptr == image_unorder_first )
					image_unorder_first = next;
				image_list_add(&image_first, image_ptr->image);
				image_list_unlink(image_ptr);
				free(image_ptr);
			}
			image_ptr = next;
		}

		image_ptr = image_unorder_first;
		while ( image_ptr ) {
			struct image_list * next = image_ptr->next;
			if ( image_ptr->image->type == IMAGE_2ND ) {
				if ( have_2nd == 0 ) {
					image_2nd = image_ptr->image;
					have_2nd = 1;
				} else if ( have_2nd == 1 ) {
					image_2nd = NULL;
					have_2nd = 2;
				}
			}
			if ( image_ptr == image_unorder_first )
				image_unorder_first = next;
			image_list_add(&image_first, image_ptr->image);
			image_list_unlink(image_ptr);
			free(image_ptr);
			image_ptr = next;
		}

		/* make sure that fiasco_in has valid images */
		if ( fiasco_in )
			fiasco_in->first = image_first;

	}

	/* remove 2nd image when doing normal flash */
	if ( dev_flash ) {
		image_ptr = image_first;
		while ( image_ptr ) {
			struct image_list * next = image_ptr->next;
			if ( image_ptr->image->type == IMAGE_2ND ) {
				if ( image_ptr == image_first )
					image_first = next;
				image_list_del(image_ptr);
			}
			image_ptr = next;
		}

		/* make sure that fiasco_in has valid images */
		if ( fiasco_in )
			fiasco_in->first = image_first;
	}

	/* identify images */
	if ( image_ident ) {
		if ( fiasco_in ) {
			fiasco_print_info(fiasco_in);
			printf("\n");
		} else if ( ! image_first ) {
			ERROR("No image specified");
			ret = 1;
			goto clean;
		}
		for ( image_ptr = image_first; image_ptr; image_ptr = image_ptr->next ) {
			image_print_info(image_ptr->image);
			printf("\n");
		}
		ret = 0;
		goto clean;
	}

	/* unpack fiasco */
	if ( fiasco_un ) {
		if ( ! fiasco_in ) {
			ERROR("No fiasco image specified");
			ret = 1;
			goto clean;
		}
		fiasco_unpack(fiasco_in, fiasco_un_arg);
	}

	/* remove unknown images */
	image_ptr = image_first;
	while ( image_ptr ) {
		struct image_list * next = image_ptr->next;
		if ( image_ptr->image->type == IMAGE_UNKNOWN ) {
			WARNING("Removing unknown image (specified by %s %s)", image_ptr->image->orig_filename ? "file" : "fiasco", image_ptr->image->orig_filename ? image_ptr->image->orig_filename : "image");
			if ( image_ptr == image_first )
				image_first = next;
			if ( fiasco_in && image_ptr == fiasco_in->first )
				fiasco_in->first = fiasco_in->first->next;
			image_list_del(image_ptr);
		}
		image_ptr = next;
	}

	/* make sure that fiasco_in has valid images */
	if ( fiasco_in )
		fiasco_in->first = image_first;

	/* generate fiasco */
	if ( fiasco_gen ) {
		char * swver = strchr(fiasco_gen_arg, '%');
		if ( swver )
			*(swver++) = 0;
		if ( swver && strlen(swver) >= sizeof(fiasco_out->swver) ) {
			ERROR("SW rel version is too long");
			ret = 1;
			goto clean;
		}
		fiasco_out = fiasco_alloc_empty();
		if ( ! fiasco_out ) {
			ERROR("Cannot write images to fiasco file %s", fiasco_gen_arg);
			ret = 1;
			goto clean;
		} else {
			if ( swver )
				strcpy(fiasco_out->swver, swver);
			fiasco_out->first = image_first;
			fiasco_write_to_file(fiasco_out, fiasco_gen_arg);
			fiasco_out->first = NULL;
			fiasco_free(fiasco_out);
		}
	}

	if ( dev_cold_flash ) {
		if ( have_2nd == 0 ) {
			ERROR("2nd image for Cold Flashing was not specified");
			ret = 1;
			goto clean;
		} else if ( have_2nd == 2 ) {
			ERROR("More 2nd images for Cold Flashing was specified");
			ret = 1;
			goto clean;
		}

		if ( have_secondary == 0 ) {
			ERROR("Secondary image for Cold Flashing was not specified");
			ret = 1;
			goto clean;
		} else if ( have_secondary == 2 ) {
			ERROR("More Secondary images for Cold Flashing was specified");
			ret = 1;
			goto clean;
		}
	}

	if ( dev_load && dev_flash ) {
		ERROR("Options load and flash cannot be used together");
		ret = 1;
		goto clean;
	}

	if ( dev_load && ! image_first ) {
		ERROR("No image specified for loading");
		ret = 1;
		goto clean;
	}

	if ( dev_flash && ! image_first ) {
		ERROR("No image specified for flashing");
		ret = 1;
		goto clean;
	}

	/* operations */
	if ( do_device ) {

		int again = 1;

		while ( again ) {

			again = 0;

			if ( dev )
				dev_free(dev);

			dev = dev_detect();
			if ( ! dev ) {
				ERROR("No device detected");
				break;
			}

			/* cold flash */
			if ( dev_cold_flash ) {

				ret = dev_cold_flash_images(dev, image_2nd, image_secondary);
				dev_free(dev);
				dev = NULL;

				if ( ret == -EAGAIN ) {
					again = 1;
					continue;
				}

				if ( ret != 0 )
					goto clean;

				if ( dev_flash ) {
					dev_cold_flash = 0;
					again = 1;
					continue;
				}

				break;

			}

			if ( ! dev->detected_device )
				printf("Device: (not detected)\n");
			else
				printf("Device: %s\n", device_to_string(dev->detected_device));

			if ( dev->detected_hwrev <= 0 )
				printf("HW revision: (not detected)\n");
			else
				printf("HW revision: %d\n", dev->detected_hwrev);

			nolo_ver[0] = 0;
			dev_get_nolo_ver(dev, nolo_ver, sizeof(nolo_ver));
			printf("NOLO version: %s\n", nolo_ver[0] ? nolo_ver : "(not detected)");

			kernel_ver[0] = 0;
			dev_get_kernel_ver(dev, kernel_ver, sizeof(kernel_ver));
			printf("Kernel version: %s\n", kernel_ver[0] ? kernel_ver : "(not detected)");

			initfs_ver[0] = 0;
			dev_get_initfs_ver(dev, initfs_ver, sizeof(initfs_ver));
			printf("Initfs version: %s\n", initfs_ver[0] ? initfs_ver : "(not detected)");

			sw_ver[0] = 0;
			dev_get_sw_ver(dev, sw_ver, sizeof(sw_ver));
			printf("Software release version: %s\n", sw_ver[0] ? sw_ver : "(not detected)");

			content_ver[0] = 0;
			dev_get_content_ver(dev, content_ver, sizeof(content_ver));
			printf("Content eMMC version: %s\n", content_ver[0] ? content_ver : "(not detected)");

			ret = dev_get_root_device(dev);
			printf("Root device: ");
			if ( ret == 0 )
				printf("flash");
			else if ( ret == 1 )
				printf("mmc");
			else if ( ret == 2 )
				printf("usb");
			else
				printf("(not detected)");
			printf("\n");

			ret = dev_get_usb_host_mode(dev);
			printf("USB host mode: ");
			if ( ret == 0 )
				printf("disabled");
			else if ( ret == 1 )
				printf("enabled");
			else
				printf("(not detected)");
			printf("\n");

			ret = dev_get_rd_mode(dev);
			printf("R&D mode: ");
			if ( ret == 0 )
				printf("disabled");
			else if ( ret == 1 )
				printf("enabled");
			else
				printf("(not detected)");
			printf("\n");

			if ( ret == 1 ) {
				buf[0] = 0;
				ret = dev_get_rd_flags(dev, buf, sizeof(buf));
				printf("R&D flags: ");
				if ( ret < 0 )
					printf("(not detected)");
				else
					printf("%s", buf);
				printf("\n");
			}

			/* device identify */
			if ( dev_ident ) {
				if ( ! dev->detected_device ) {
					dev_reboot_device(dev);
					goto again;
				}
				dev_free(dev);
				dev = NULL;
				break;
			}

			printf("\n");

			/* filter images by device & hwrev */
			if ( detected_device )
				filter_images_by_device(dev->detected_device, &image_first);
			if ( detected_hwrev )
				filter_images_by_hwrev(dev->detected_hwrev, &image_first);
			if ( fiasco_in && ( detected_device || detected_hwrev ) )
				fiasco_in->first = image_first;

			/* set kernel and initfs images for loading */
			if ( dev_load ) {
				image_ptr = image_first;
				while ( image_ptr ) {
					struct image_list * next = image_ptr->next;
					if ( image_ptr->image->type == IMAGE_KERNEL ) {
						if ( have_kernel == 0 ) {
							image_kernel = image_ptr;
							have_kernel = 1;
						} else if ( have_kernel == 1 && image_kernel != image_ptr ) {
							image_kernel = NULL;
							have_kernel = 2;
						}
					}
					image_ptr = next;
				}

				image_ptr = image_first;
				while ( image_ptr ) {
					struct image_list * next = image_ptr->next;
					if ( image_ptr->image->type == IMAGE_INITFS ) {
						if ( have_initfs == 0 ) {
							image_initfs = image_ptr;
							have_initfs = 1;
						} else if ( have_initfs == 1 && image_initfs != image_ptr ) {
							image_initfs = NULL;
							have_initfs = 2;
						}
					}
					image_ptr = next;
				}

				if ( have_kernel == 2 ) {
					ERROR("More Kernel images for loading was specified");
					ret = 1;
					goto clean;
				}

				if ( have_initfs == 2 ) {
					ERROR("More Initfs images for loading was specified");
					ret = 1;
					goto clean;
				}

				if ( have_kernel == 0 && have_initfs == 0 ) {
					ERROR("Kernel image or Initfs image for loading was not specified");
					ret = 1;
					goto clean;
				}
			}

			/* load */
			if ( dev_load ) {
				if ( image_kernel ) {
					ret = dev_load_image(dev, image_kernel->image);
					if ( ret < 0 )
						goto again;

					if ( image_kernel == image_first )
						image_first = image_first->next;
					if ( fiasco_in && image_kernel == fiasco_in->first )
						fiasco_in->first = fiasco_in->first->next;

					image_list_del(image_kernel);
					image_kernel = NULL;
				}

				if ( image_initfs ) {
					ret = dev_load_image(dev, image_initfs->image);
					if ( ret < 0 )
						goto again;

					if ( image_initfs == image_first )
						image_first = image_first->next;
					if ( fiasco_in && image_initfs == fiasco_in->first )
						fiasco_in->first = fiasco_in->first->next;

					image_list_del(image_initfs);
					image_initfs = NULL;
				}
			}

			/* flash */
			if ( dev_flash ) {
				image_ptr = image_first;
				while ( image_ptr ) {
					struct image_list * next = image_ptr->next;
					ret = dev_flash_image(dev, image_ptr->image);
					if ( ret < 0 )
						goto again;

					if ( image_ptr == image_first )
						image_first = image_first->next;
					if ( fiasco_in && image_ptr == fiasco_in->first )
						fiasco_in->first = fiasco_in->first->next;

					image_list_del(image_ptr);
					image_ptr = next;
				}
			}

			/* configuration */
			ret = 0;
			if ( set_rd_flags ) {
				set_rd = 1;
				set_rd_arg = "1";
			}

			if ( sw_ver[0] && dev_flash && ! set_sw && fiasco_in && fiasco_in->swver[0] && strcmp(fiasco_in->swver, sw_ver) != 0 ) {
				set_sw = 1;
				set_sw_arg = fiasco_in->swver;
			}

			if ( set_root )
				ret = dev_set_root_device(dev, atoi(set_root_arg));
			if ( ret == -EAGAIN )
				goto again;

			if ( set_usb )
				ret = dev_set_usb_host_mode(dev, atoi(set_usb_arg));
			if ( ret == -EAGAIN )
				goto again;

			if ( set_rd )
				ret = dev_set_rd_mode(dev, atoi(set_rd_arg));
			if ( ret == -EAGAIN )
				goto again;

			if ( set_rd_flags )
				ret = dev_set_rd_flags(dev, set_rd_flags_arg);
			if ( ret == -EAGAIN )
				goto again;

			if ( set_hw )
				ret = dev_set_hwrev(dev, atoi(set_hw_arg));
			if ( ret == -EAGAIN )
				goto again;

			if ( set_nolo )
				ret = dev_set_nolo_ver(dev, set_nolo_arg);
			if ( ret == -EAGAIN )
				goto again;

			if ( set_kernel )
				ret = dev_set_kernel_ver(dev, set_kernel_arg);
			if ( ret == -EAGAIN )
				goto again;

			if ( set_initfs )
				ret = dev_set_initfs_ver(dev, set_initfs_arg);
			if ( ret == -EAGAIN )
				goto again;

			if ( set_sw )
				ret = dev_set_sw_ver(dev, set_sw_arg);
			if ( ret == -EAGAIN )
				goto again;

			if ( set_emmc )
				ret = dev_set_content_ver(dev, set_emmc_arg);
			if ( ret == -EAGAIN )
				goto again;

			/* check */
			if ( dev_check )
				ret = dev_check_badblocks(dev, dev_check_arg);
			if ( ret == -EAGAIN )
				goto again;

			/* dump */

			if ( dev_dump_fiasco ) {
				dev_dump = 1;
				tmp = strdup(dev_dump_fiasco_arg);
				dev_dump_arg = dirname(tmp);
			}

			if ( dev_dump ) {

				buf[0] = 0;

				if ( dev_dump_arg ) {
					if ( ! getcwd(buf, sizeof(buf)) )
						buf[0] = 0;
					if ( chdir(dev_dump_arg) < 0 )
						buf[0] = 0;
				}

				if ( filter_type ) {
					enum image_type type = image_type_from_string(filter_type_arg);
					if ( ! type || ! image_tmp_name(type) ) {
						ERROR("Specified unknown image type for filtering: %s", filter_type_arg);
						ret = 1;
						goto clean;
					}
					ret = dev_dump_image(dev, type, image_tmp_name(type));
					if ( ret != 0 )
						goto clean;
				} else {
					for ( i = 0; i < IMAGE_COUNT; ++i )
						if ( image_tmp_name(i) )
							dev_dump_image(dev, i, image_tmp_name(i));
				}

				if ( buf[0] )
					if ( chdir(buf) < 0 )
						ERROR_INFO("Cannot chdir back to %s", buf);

				printf("Done\n");

			}

			/* dump fiasco */
			if ( dev_dump_fiasco ) {

				struct image_list * image_dump_first = NULL;
				struct image * image_dump = NULL;

				for ( i = 0; i < IMAGE_COUNT; ++i ) {

					if ( ! image_tmp_name(i) )
						continue;

					buf[0] = 0;
					snprintf(buf, sizeof(buf), "%hd", dev->detected_hwrev);

					switch ( i ) {
						case IMAGE_2ND:
						case IMAGE_XLOADER:
						case IMAGE_SECONDARY:
							ptr = nolo_ver;
							break;

						case IMAGE_KERNEL:
							ptr = kernel_ver;
							break;

						case IMAGE_INITFS:
							ptr = initfs_ver;
							break;

						case IMAGE_ROOTFS:
							ptr = sw_ver;
							break;

						case IMAGE_MMC:
							ptr = content_ver;
							break;

						default:
							ptr = NULL;
							break;
					}

					image_dump = image_alloc_from_file(image_tmp_name(i), image_type_to_string(i), device_to_string(dev->detected_device), buf, ptr, NULL);

					if ( ! image_dump )
						continue;

					image_list_add(&image_dump_first, image_dump);

				}

				printf("\n");

				fiasco_out = fiasco_alloc_empty();
				if ( ! fiasco_out ) {
					ERROR("Cannot write images to fiasco file %s", dev_dump_fiasco_arg);
				} else {
					strncpy(fiasco_out->swver, sw_ver, sizeof(fiasco_out->swver));
					fiasco_out->swver[sizeof(fiasco_out->swver)-1] = 0;
					fiasco_out->first = image_dump_first;
					fiasco_write_to_file(fiasco_out, dev_dump_fiasco_arg);
					fiasco_free(fiasco_out); /* this will also free list image_dump_first */
				}

				for ( i = 0; i < IMAGE_COUNT; ++i )
					if ( image_tmp_name(i) )
						unlink(image_tmp_name(i));

				free(tmp);
				tmp = NULL;
				dev_dump_arg = NULL;

			}

			if ( ! dev_dump_fiasco ) {

				for ( i = 0; i < IMAGE_COUNT; ++i ) {

					int rename_ret;
					int rename_errno;

					if ( ! image_tmp_name(i) )
						continue;

					switch ( i ) {
						case IMAGE_2ND:
						case IMAGE_XLOADER:
						case IMAGE_SECONDARY:
							ptr = nolo_ver;
							break;

						case IMAGE_KERNEL:
							ptr = kernel_ver;
							break;

						case IMAGE_INITFS:
							ptr = initfs_ver;
							break;

						case IMAGE_ROOTFS:
							ptr = sw_ver;
							break;

						case IMAGE_MMC:
							ptr = content_ver;
							break;

						default:
							ptr = NULL;
							break;
					}

					buf[0] = 0;
					snprintf(buf, sizeof(buf), "%s-%s:%hd_%s", image_type_to_string(i), device_to_string(dev->detected_device), dev->detected_hwrev, ptr);

					rename_ret = rename(image_tmp_name(i), buf);
					rename_errno = errno;

					if ( rename_ret < 0 && rename_errno == ENOENT )
						continue;

					printf("Renaming %s image file to %s...\n", image_type_to_string(i), buf);

					if ( rename_ret < 0 ) {

						errno = rename_errno;
						ERROR_INFO("Renaming failed");

						buf[0] = 0;
						snprintf(buf, sizeof(buf), "%s-%s_%s", image_type_to_string(i), device_to_string(dev->detected_device), ptr);
						printf("Trying to rename %s image file to %s...\n", image_type_to_string(i), buf);

						if ( rename(image_tmp_name(i), buf) < 0 )
							ERROR_INFO("Renaming failed");

					}

				}

			}

			/* boot */
			if ( dev_boot ) {
				dev_boot_device(dev, dev_boot_arg);
				break;
			}

			/* reboot */
			if ( dev_reboot ) {
				dev_reboot_device(dev);
				break;
			}

			continue;

again:
			dev_free(dev);
			dev = NULL;
			again = 1;

		}

	}

	ret = 0;

	/* clean */
clean:

	if ( ! image_fiasco ) {
		image_ptr = image_first;
		while ( image_ptr ) {
			struct image_list * next = image_ptr->next;
			image_list_del(image_ptr);
			image_ptr = next;
		}
	}

	if ( fiasco_in )
		fiasco_free(fiasco_in);

	if ( dev )
		dev_free(dev);

	return ret;
}
