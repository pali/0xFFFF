#ifndef QUERY_H_
#define QUERY_H_

int boot_board(char *cmdline);

int reboot_board();

int get_status();

int get_usb_mode();
int set_usb_mode(unsigned int);

int get_rd_mode();
int set_rd_mode(unsigned short mode);

int get_rd_flags();
int set_rd_flags(unsigned short flags);

int get_hw_revision();

int get_root_device();
int set_root_device(unsigned short);

int get_nolo_version();

int get_sw_version();

#endif /*QUERY_H_*/
