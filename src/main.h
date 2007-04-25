
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
