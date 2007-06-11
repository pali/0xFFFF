
#ifndef _INCLUDE_MAIN_H_
#define _INCLUDE_MAIN_H_

#define _FILE_OFFSET_BITS 64
#define _GNU_SOURCE


int reverse_extract_pieces(char *dir);
void flash_image(char *filename, char *piece, char *version);
int fiasco_read_image(char *file);
void check_nolo_order();
extern struct usb_dev_handle *dev;
unsigned long get_file_size(char *file);
void progressbar(unsigned long long part, unsigned long long total);
char *fpid_file(char *filename);
int add_piece(char *piece);

#include <usb.h>

extern struct usb_device *device;
extern struct usb_dev_handle *dev;
int is_valid_device(struct usb_device_descriptor *udd);
void list_valid_devices();
int usb_device_found(struct usb_device_descriptor *udd);
int console(const char *device);
int connect_via_usb();
int console_prompt();

//
void cmd_info(char *line);
int check_badblocks(char *mtddev);
int dump_config();

extern int verbose;
#define D if (verbose)

#define CMD_WRITE 64
#define CMD_QUERY 192

#define NOLO_GET_STATUS   1
#define NOLO_GET_BOARD_ID 2
#define NOLO_GET_VERSION  3

struct piece_t {
	char *name;
	char *type;
	char *vers;
};

struct devices {
  char *name;
  unsigned short vendor_id;
  unsigned short product_id;
  unsigned short flags;
};
#define SUPPORTED_DEVICES 5
extern struct devices supported_devices[SUPPORTED_DEVICES];

extern int pcs_n;
extern struct piece_t pcs[10];

enum {
	PIECE_XLOADER = 0,
	PIECE_SECONDARY,
	PIECE_KERNEL,
	PIECE_INITFS,
	PIECE_ROOTFS,
	PIECE_OMAPNAND,
	PIECE_LAST
};

extern char *pieces[];
extern char *modes[];
extern char *root_devices[];

#endif
