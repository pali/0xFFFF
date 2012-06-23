
#ifndef _INCLUDE_MAIN_H_
#define _INCLUDE_MAIN_H_

#include "hash.h"
#include "nolo.h"
#include "os.h"
extern char strbuf[1024];

#define _FILE_OFFSET_BITS 64
#define _GNU_SOURCE

// Forward declaration for use in function arguments.
struct devices;

int queue_mode();
int reverse_extract_pieces(char *dir);
void flash_image(const char *filename, const char *piece, const char *device, const char *hwrevs, const char *version);
int fiasco_read_image(char *file);
void check_nolo_order();
extern struct usb_dev_handle *dev;
unsigned long get_file_size(const char *file);
void progressbar(unsigned long long part, unsigned long long total);
const char *fpid_file(const char *filename);
int add_piece(char *piece);
void eprintf(const char *format, ...);

#include <limits.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#if HAVE_USB
#include <usb.h>

int fiasco_flash(const char *file);
extern struct usb_device *device;
extern struct usb_dev_handle *dev;
int is_valid_device(struct usb_device_descriptor *udd);
void list_valid_devices();
int usb_device_found(struct usb_device_descriptor *udd, struct devices *it_device);
int console(const char *device);
int connect_via_usb();
#endif

int console_prompt();

//
void cmd_info(char *line);
int check_badblocks(char *mtddev);
int dump_config();

extern int verbose;
#define D if (verbose)

#define CMD_WRITE 64
#define CMD_QUERY 192

struct piece_t {
	char *name;
	char *type;
	char *device;
	char *hwrevs;
	char *version;
};

enum flash_mode {
	FLASH_COLD,
	FLASH_NORMAL,
	FLASH_EMMC,
	FLASH_MKII,
};

struct devices {
  char *name;
  unsigned short vendor_id;
  unsigned short product_id;
  enum flash_mode mode;
};

#define SUPPORTED_DEVICES 8
extern struct devices supported_devices[SUPPORTED_DEVICES];

extern int pcs_n;
extern struct piece_t pcs[10];

enum {
	PIECE_XLOADER = 0,
	PIECE_2ND,
	PIECE_SECONDARY,
	PIECE_KERNEL,
	PIECE_INITFS,
	PIECE_ROOTFS,
	PIECE_OMAPNAND,
	PIECE_MMC,
	PIECE_CMT_2ND,
	PIECE_CMT_ALGO,
	PIECE_CMT_MCUSW,
	PIECE_FIASCO,
	PIECE_LAST
};

struct header_t {
	int fd;
	char fwname[128];
	char name[128];
	char type[128];
	char device[16];
	char hwrevs[128];
	char version[128];
	unsigned short hash;
	unsigned int size;
	unsigned char *data;
	char *layout;
};

extern char *pieces[];
extern char *modes[];
extern char *root_devices[];

// fiasco
int openfiasco(const char *name, const char *grep, int v);
int fiasco_new(const char *filename, const char *name);
void fiasco_data_read(struct header_t *header);
int fiasco_add_eof(int fd);
extern int (*fiasco_callback)(struct header_t *header);
int fiasco_add(int fd, const char *name, const char *file, const char *layout, const char *device, const char *hwrevs, const char *version);
int fiasco_pack(int optind, char *argv[]);
int nanddump(char *mtddev, unsigned long start_addr, unsigned long length, char *dumpfile, int isbl, int ioob);

//
int reboot_board();
int get_rd_flags();
int get_usb_mode();
int get_nolo_version();

#endif
