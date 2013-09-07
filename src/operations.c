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

#include "global.h"
#include "device.h"
#include "usb-device.h"
#include "cold-flash.h"
#include "nolo.h"
#include "mkii.h"
#include "disk.h"
#include "local.h"

#include "operations.h"

struct device_info * dev_detect(void) {

	int ret = 0;
	struct device_info * dev = NULL;
	struct usb_device_info * usb = NULL;

	dev = calloc(1, sizeof(struct device_info));
	if ( ! dev )
		goto clean;

	/* LOCAL */
	if ( local_init() == 0 ) {
		dev->method = METHOD_LOCAL;
		dev->detected_device = local_get_device();
		dev->detected_hwrev = local_get_hwrev();
		return dev;
	}

	/* USB */
	usb = usb_open_and_wait_for_device();
	if ( usb ) {
		dev->method = METHOD_USB;
		dev->usb = usb;

		if ( dev->usb->flash_device->protocol == FLASH_NOLO )
			ret = nolo_init(dev->usb);
		else if ( dev->usb->flash_device->protocol == FLASH_COLD )
			ret = init_cold_flash(dev->usb);
		else if ( dev->usb->flash_device->protocol == FLASH_MKII )
			ret = mkii_init(dev->usb);
		else if ( dev->usb->flash_device->protocol == FLASH_DISK )
			ret = disk_init(dev->usb);
		else {
			ERROR("Unknown USB mode");
			goto clean;
		}

		if ( ret < 0 )
			goto clean;

		dev->detected_device = dev_get_device(dev);
		dev->detected_hwrev = dev_get_hwrev(dev);

		if ( dev->detected_device && dev->usb->device && dev->detected_device != dev->usb->device ) {
			ERROR("Bad device, expected %s, got %s", device_to_string(dev->usb->device), device_to_string(dev->detected_device));
			goto clean;
		}

		return dev;
	}

clean:
	if (usb)
		usb_close_device(usb);
	free(dev);
	return NULL;

}

void dev_free(struct device_info * dev) {

	if ( dev->usb )
		usb_close_device(dev->usb);
	free(dev);

}

enum device dev_get_device(struct device_info * dev) {

	if ( dev->method == METHOD_LOCAL )
		return local_get_device();

	if ( dev->method == METHOD_USB ) {

		enum usb_flash_protocol protocol = dev->usb->flash_device->protocol;

		if ( protocol == FLASH_COLD )
			return DEVICE_UNKNOWN;
		else if ( protocol == FLASH_NOLO )
			return nolo_get_device(dev->usb);
		else if ( protocol == FLASH_MKII )
			return mkii_get_device(dev->usb);
/*		else if ( protocol == FLASH_DISK )
			return disk_get_device(dev->usb);*/

	}

	return DEVICE_UNKNOWN;

}

int dev_load_image(struct device_info * dev, struct image * image) {

	if ( dev->method == METHOD_LOCAL ) {
		ERROR("Loading image on local device is not supported");
		return -1;
	}

	if ( dev->method == METHOD_USB ) {

		enum usb_flash_protocol protocol = dev->usb->flash_device->protocol;

		if ( protocol == FLASH_NOLO )
			return nolo_load_image(dev->usb, image);

		usb_switch_to_nolo(dev->usb);
		return -EAGAIN;

	}

	return -1;

}

int dev_cold_flash_images(struct device_info * dev, struct image * x2nd, struct image * secondary) {

	if ( dev->method == METHOD_LOCAL ) {
		ERROR("Cold Flashing on local device is not supported");
		return -1;
	}

	if ( dev->method == METHOD_USB ) {

		enum usb_flash_protocol protocol = dev->usb->flash_device->protocol;

		if ( protocol == FLASH_COLD )
			return cold_flash(dev->usb, x2nd, secondary);

		usb_switch_to_cold(dev->usb);
		return -EAGAIN;

	}

	return -1;

}

int dev_flash_image(struct device_info * dev, struct image * image) {

	if ( dev->method == METHOD_LOCAL )
		return local_flash_image(image);

	if ( dev->method == METHOD_USB ) {

		enum usb_flash_protocol protocol = dev->usb->flash_device->protocol;

		if ( protocol == FLASH_NOLO )
			return nolo_flash_image(dev->usb, image);
		else if ( protocol == FLASH_MKII )
			return mkii_flash_image(dev->usb, image);
		else {
			usb_switch_to_nolo(dev->usb);
			return -EAGAIN;
		}

	}

	return -1;

}

int dev_dump_image(struct device_info * dev, enum image_type image, const char * file) {

	if ( dev->method == METHOD_LOCAL )
		return local_dump_image(image, file);

	if ( dev->method == METHOD_USB ) {
		ERROR("Dump image via USB is not supported");
		return -1;
	}

	return -1;

}

int dev_check_badblocks(struct device_info * dev, const char * device) {

	if ( dev->method == METHOD_LOCAL )
		return local_check_badblocks(device);

	if ( dev->method == METHOD_USB ) {
		ERROR("Check for badblocks via USB is not supported");
		return -1;
	}

	return -1;

}

int dev_boot_device(struct device_info * dev, const char * cmdline) {

	if ( dev->method == METHOD_LOCAL ) {
		ERROR("Booting device on local device does not make sense");
		return -1;
	}

	if ( dev->method == METHOD_USB ) {

		enum usb_flash_protocol protocol = dev->usb->flash_device->protocol;

		if ( protocol == FLASH_NOLO )
			return nolo_boot_device(dev->usb, cmdline);
		else {
			usb_switch_to_nolo(dev->usb);
			return 0;
		}

	}

	return -1;

}

int dev_reboot_device(struct device_info * dev) {

	if ( dev->method == METHOD_LOCAL )
		return local_reboot_device();

	if ( dev->method == METHOD_USB ) {

		enum usb_flash_protocol protocol = dev->usb->flash_device->protocol;

		if ( protocol == FLASH_COLD )
			return leave_cold_flash(dev->usb);
		else if ( protocol == FLASH_NOLO )
			return nolo_reboot_device(dev->usb);
		else if ( protocol == FLASH_MKII )
			return mkii_reboot_device(dev->usb);
		else {
			ERROR("Rebooting device in RAW disk mode is not supported");
			return -1;
		}

	}

	return -1;

}

int dev_get_root_device(struct device_info * dev) {

	if ( dev->method == METHOD_LOCAL )
		return local_get_root_device();

	if ( dev->method == METHOD_USB ) {

		enum usb_flash_protocol protocol = dev->usb->flash_device->protocol;

		if ( protocol == FLASH_NOLO )
			return nolo_get_root_device(dev->usb);

	}

	return -1;

}

int dev_set_root_device(struct device_info * dev, int device) {

	if ( dev->method == METHOD_LOCAL )
		return local_set_root_device(device);

	if ( dev->method == METHOD_USB ) {

		if ( dev->usb->flash_device->protocol == FLASH_NOLO )
			return nolo_set_root_device(dev->usb, device);

		if ( dev->usb->flash_device->protocol == FLASH_COLD ) {
			usb_switch_to_nolo(dev->usb);
			return -EAGAIN;
		}

	}

	return -1;

}

int dev_get_usb_host_mode(struct device_info * dev) {

	if ( dev->method == METHOD_LOCAL )
		return local_get_usb_host_mode();

	if ( dev->method == METHOD_USB ) {

		if ( dev->usb->flash_device->protocol == FLASH_NOLO )
			return nolo_get_usb_host_mode(dev->usb);

	}

	return -1;

}

int dev_set_usb_host_mode(struct device_info * dev, int enable) {

	if ( dev->method == METHOD_LOCAL )
		return local_set_usb_host_mode(enable);

	if ( dev->method == METHOD_USB ) {

		if ( dev->usb->flash_device->protocol == FLASH_NOLO )
			return nolo_set_usb_host_mode(dev->usb, enable);

		if ( dev->usb->flash_device->protocol == FLASH_COLD ) {
			usb_switch_to_nolo(dev->usb);
			return -EAGAIN;
		}

	}

	return -1;

}

int dev_get_rd_mode(struct device_info * dev) {

	if ( dev->method == METHOD_LOCAL )
		return local_get_rd_mode();

	if ( dev->method == METHOD_USB ) {

		if ( dev->usb->flash_device->protocol == FLASH_NOLO )
			return nolo_get_rd_mode(dev->usb);

	}

	return -1;

}

int dev_set_rd_mode(struct device_info * dev, int enable) {

	if ( dev->method == METHOD_LOCAL )
		return local_set_rd_mode(enable);

	if ( dev->method == METHOD_USB ) {

		if ( dev->usb->flash_device->protocol == FLASH_NOLO )
			return nolo_set_rd_mode(dev->usb, enable);

		if ( dev->usb->flash_device->protocol == FLASH_COLD ) {
			usb_switch_to_nolo(dev->usb);
			return -EAGAIN;
		}

	}

	return -1;

}

int dev_get_rd_flags(struct device_info * dev, char * flags, size_t size) {

	if ( dev->method == METHOD_LOCAL )
		return local_get_rd_flags(flags, size);

	if ( dev->method == METHOD_USB ) {

		if ( dev->usb->flash_device->protocol == FLASH_NOLO )
			return nolo_get_rd_flags(dev->usb, flags, size);

	}

	return -1;

}

int dev_set_rd_flags(struct device_info * dev, const char * flags) {

	if ( dev->method == METHOD_LOCAL )
		return local_set_rd_flags(flags);

	if ( dev->method == METHOD_USB ) {

		if ( dev->usb->flash_device->protocol == FLASH_NOLO )
			return nolo_set_rd_flags(dev->usb, flags);

		if ( dev->usb->flash_device->protocol == FLASH_COLD ) {
			usb_switch_to_nolo(dev->usb);
			return -EAGAIN;
		}

	}

	return -1;

}

int16_t dev_get_hwrev(struct device_info * dev) {

	if ( dev->method == METHOD_LOCAL )
		return local_get_hwrev();

	if ( dev->method == METHOD_USB ) {

		if ( dev->usb->flash_device->protocol == FLASH_NOLO )
			return nolo_get_hwrev(dev->usb);

	}

	return -1;

}

int dev_set_hwrev(struct device_info * dev, int16_t hwrev) {

	if ( dev->method == METHOD_LOCAL )
		return local_set_hwrev(hwrev);

	if ( dev->method == METHOD_USB ) {

		if ( dev->usb->flash_device->protocol == FLASH_NOLO )
			return nolo_set_hwrev(dev->usb, hwrev);

		if ( dev->usb->flash_device->protocol == FLASH_COLD ) {
			usb_switch_to_nolo(dev->usb);
			return -EAGAIN;
		}

	}

	return -1;

}

int dev_get_kernel_ver(struct device_info * dev, char * ver, size_t size) {

	if ( dev->method == METHOD_LOCAL )
		return local_get_kernel_ver(ver, size);

	if ( dev->method == METHOD_USB ) {

		if ( dev->usb->flash_device->protocol == FLASH_NOLO )
			return nolo_get_kernel_ver(dev->usb, ver, size);

	}

	return -1;

}

int dev_set_kernel_ver(struct device_info * dev, const char * ver) {

	if ( dev->method == METHOD_LOCAL )
		return local_set_kernel_ver(ver);

	if ( dev->method == METHOD_USB ) {

		if ( dev->usb->flash_device->protocol == FLASH_NOLO )
			return nolo_set_kernel_ver(dev->usb, ver);

		if ( dev->usb->flash_device->protocol == FLASH_COLD ) {
			usb_switch_to_nolo(dev->usb);
			return -EAGAIN;
		}

	}

	return -1;

}

int dev_get_initfs_ver(struct device_info * dev, char * ver, size_t size) {

	if ( dev->method == METHOD_LOCAL )
		return local_get_initfs_ver(ver, size);

	if ( dev->method == METHOD_USB ) {

		if ( dev->usb->flash_device->protocol == FLASH_NOLO )
			return nolo_get_initfs_ver(dev->usb, ver, size);

	}

	return -1;

}

int dev_set_initfs_ver(struct device_info * dev, const char * ver) {

	if ( dev->method == METHOD_LOCAL )
		return local_set_initfs_ver(ver);

	if ( dev->method == METHOD_USB ) {

		if ( dev->usb->flash_device->protocol == FLASH_NOLO )
			return nolo_set_initfs_ver(dev->usb, ver);

		if ( dev->usb->flash_device->protocol == FLASH_COLD ) {
			usb_switch_to_nolo(dev->usb);
			return -EAGAIN;
		}

	}

	return -1;

}

int dev_get_nolo_ver(struct device_info * dev, char * ver, size_t size) {

	if ( dev->method == METHOD_LOCAL )
		return local_get_nolo_ver(ver, size);

	if ( dev->method == METHOD_USB ) {

		if ( dev->usb->flash_device->protocol == FLASH_NOLO )
			return nolo_get_nolo_ver(dev->usb, ver, size);

	}

	return -1;

}

int dev_set_nolo_ver(struct device_info * dev, const char * ver) {

	if ( dev->method == METHOD_LOCAL )
		return local_set_nolo_ver(ver);

	if ( dev->method == METHOD_USB ) {

		if ( dev->usb->flash_device->protocol == FLASH_NOLO )
			return nolo_set_nolo_ver(dev->usb, ver);

		if ( dev->usb->flash_device->protocol == FLASH_COLD ) {
			usb_switch_to_nolo(dev->usb);
			return -EAGAIN;
		}

	}

	return -1;

}

int dev_get_sw_ver(struct device_info * dev, char * ver, size_t size) {

	if ( dev->method == METHOD_LOCAL )
		return local_get_sw_ver(ver, size);

	if ( dev->method == METHOD_USB ) {

		if ( dev->usb->flash_device->protocol == FLASH_NOLO )
			return nolo_get_sw_ver(dev->usb, ver, size);

	}

	return -1;

}

int dev_set_sw_ver(struct device_info * dev, const char * ver) {

	if ( dev->method == METHOD_LOCAL )
		return local_set_sw_ver(ver);

	if ( dev->method == METHOD_USB ) {

		if ( dev->usb->flash_device->protocol == FLASH_NOLO )
			return nolo_set_sw_ver(dev->usb, ver);

		if ( dev->usb->flash_device->protocol == FLASH_COLD ) {
			usb_switch_to_nolo(dev->usb);
			return -EAGAIN;
		}

	}

	return -1;

}

int dev_get_content_ver(struct device_info * dev, char * ver, size_t size) {

	if ( dev->method == METHOD_LOCAL )
		return local_get_content_ver(ver, size);

	if ( dev->method == METHOD_USB ) {

		if ( dev->usb->flash_device->protocol == FLASH_NOLO )
			return nolo_get_content_ver(dev->usb, ver, size);

	}

	return -1;

}

int dev_set_content_ver(struct device_info * dev, const char * ver) {

	if ( dev->method == METHOD_LOCAL )
		return local_set_content_ver(ver);

	if ( dev->method == METHOD_USB ) {

		if ( dev->usb->flash_device->protocol == FLASH_NOLO )
			return nolo_set_content_ver(dev->usb, ver);

		if ( dev->usb->flash_device->protocol == FLASH_COLD ) {
			usb_switch_to_nolo(dev->usb);
			return -EAGAIN;
		}

	}

	return -1;

}
