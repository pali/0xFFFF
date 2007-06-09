/*
 * Copyright (C) 2007
 *       pancake <pancake@youterm.com>
 *
 * 0xFFFF is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0xFFFF is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0xFFFF; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <getopt.h>

int open_serial(const char *file)
{
	struct termios tc;
	int fd = open (file, O_RDWR | O_SYNC);
	if (fd < 0) {
		perror (file);
		return -1;
	}
	// 115200 baud, 8n1, no flow control
	tcgetattr(fd, &tc);
	tc.c_iflag = IGNPAR;
	tc.c_oflag = 0;
	tc.c_cflag = CS8 | CREAD | CLOCAL;
	tc.c_lflag = 0;
	cfsetispeed(&tc, B115200);
	cfsetospeed(&tc, B115200);
	tcsetattr(fd, TCSANOW, &tc);
	return fd;
}

int console(const char *device)
{
	char str[513];
	int fd = open_serial(device);
	int pid;

	if (fd == -1) {
		fprintf(stderr, "Cannot open serial device %s\n", device);
		return 1;
	}

	pid = fork();
	if (pid) {
		while(1) {
			fgets(str, 512, stdin);
			if (write(fd, str, strlen(str)-1) == -1) {
				perror("write");
				exit (1);
			}
		}
	} else {
		char buf;
		while(1) {
			if (read(fd, &buf,1) == -1) {
				perror("read");
				exit (1);
			}
			write(1, &buf, 1);
		}
	}

	close(fd);

	return 0;
}
