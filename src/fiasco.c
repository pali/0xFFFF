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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include "main.h"

void (*fiasco_callback)(struct header_t *header) = NULL;

int openfiasco(char *name)
{
	struct header_t header;
	unsigned char buf[128];
	unsigned char data[128];
	unsigned int namelen;
	int i;
	int fd = open(name, O_RDONLY);

	if (fd == -1) {
		fprintf(stderr, "Cannot open %s\n", name);
		return 1;
	}

	/* read header */
	read(fd, buf, 5);
	if (buf[0] != 0xb4) {
		printf("Invalid header\n");
		return close(fd);
	}
	memcpy(&namelen,buf+1,4);
	namelen = ntohl(namelen);
	if (namelen>128) {
		printf("Stupid length at header. Is this a joke?\n");
		return close(fd);
	}
	memset(buf,'\0', 128);
	read(fd, buf, namelen);
	// 6 first bytes are unknown
	// buf 00 00 00 VERSION e8 0e
//	printf("6Bytes: %02x %02x %02x %02x %02x %02x\n", 
//		buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
	printf("Fiasco version: %2d\n", buf[3]);
	strcpy(header.fwname, buf+6);
	for(i=6;i<namelen;i+=strlen(buf+i)+1) {
		printf("Name: %s\n", buf+i);
	}

	/* walk the tree */
	while(1) {
		if (read(fd, buf, 9)<9)
			break;

		if (buf[0] != 0x54) { // 'T'
			printf("unexpected header at %d, found %02x %02x %02x\n", 
				(int)lseek(fd, 0, SEEK_CUR),
				buf[0], buf[1], buf[2]);
			break;
		}
		header.hash = buf[7]<<8|buf[8];
		//printf("BYTE: %02x %02x %02x %02x %02x %02x\n", 
			//	buf[1], buf[2], buf[3], buf[4], buf[5], buf[6]);

		/* piece name */
		memset(data, '\0', 13);
		if (read(fd, data, 12)<12)
			break;
		if (data[0] == 0xff) {
			printf(" [eof]\n");
			break;
		} else printf(" %s\n", data);
		strcpy(header.name, data);

		if (read(fd, buf, 9)<9)
			break;
		memcpy(&header.size, buf,4);
		header.size = ntohl(header.size);
		printf("   size:    %d bytes\n", header.size);
		printf("   hash:    %04x\n", header.hash);
		//printf("BYTE: %02x %02x %02x %02x %02x\n", 
		//	buf[4], buf[5], buf[6], buf[7], buf[8]);
		/* XXX this is not ok */
		//printf("BUF8: %02x\n", buf[8]);
		while (buf[8] && buf[8]>'0' && buf[8] < '9') {
			if (read(fd, data, 1)<1)
				break;
			i = data[0];
			if (read(fd, data, i)<i)
				break;
			if (data[0])
			printf("   version: %s\n", data);
			strcpy(header.version, data);
			if (read(fd, buf+8, 1)<1)
				break;
		}
		/* callback */
		if (fiasco_callback != NULL) {
			header.data = (char *)malloc(header.size);
			if (header.data == NULL) {
				printf("Cannot alloc %d bytes\n", header.size);
				break;
			}
			read(fd, header.data, header.size);
			fiasco_callback(&header);
			free(header.data);
			continue;
		}
		lseek(fd, header.size, SEEK_CUR);
	}
	return 0;
}

/* fiasco writer */

int fiasco_new(const char *filename, const char *name)
{
	int fd;
	int len = htonl(strlen(name));
	fd = open(filename, O_RDWR|O_CREAT);
	if (fd == -1)
		return -1;
	write(fd, "\xb4", 1);
	write(fd, &len, 4);
	/* version header */
	write(fd, "\x00\x00\x00\x02\xe8\x0e", 6);
	write(fd, name, strlen(name)+1);
	return fd;
}

int fiasco_add_eof(int fd)
{
	unsigned char buf[32];
	if (fd == -1)
		return -1;
	memset(buf,'\xff', 32);
	write(fd, buf, 32);
	return 0;
}

int fiasco_add(int fd, const char *name, const char *file, const char *version)
{
	int gd;
	unsigned int sz;
	unsigned char len;
	unsigned short hash;
	unsigned char *ptr = &hash;
	char bname[13];

	if (fd == -1)
		return -1;
	gd = open(file, O_RDONLY);
	if (gd == -1)
		return -1;
	sz = htonl((unsigned int) lseek(gd, 0, SEEK_END));
	// 4 bytes big endian
	write(fd, "T\x02\x2e\x19\x01\x01\x00", 7); // header?
	/* checksum */
	hash = do_hash_file(file);
	ptr[0]^=ptr[1]; ptr[1]=ptr[0]^ptr[1]; ptr[0]^=ptr[1];
	write(fd, hash, 2);

	memset(bname, '\0', 13);
	strncpy(bname, name, 12);

	write(fd, &sz, 4);
	if (version) {
		/* append metadata */
		//write(fd, version, 
		write(fd, "\x00\x00\x00\x00\x31", 5);
		len = strlen(version);
		write(fd, &len, 1);
		write(fd, version, len);
		write(fd, "\x00\x9b", 2);
	} else {
		write(fd, "\x00\x00\x00\x00\x9b", 5);
	}
	return 0;
}


/* local code */
#if 0
void my_callback(struct header_t *header)
{
	printf("Dumping %s\n", header->name);
	printf("DATA: %02x\n", header->data[0]);
}

int main(int argc, char **argv)
{
	if (argc!=2) {
		printf("Usage: unfiasco [file]\n");
		return 1;
	}

/*
	fd = fiasco_new("myfiasco", "pancake-edition");
	fiasco_add(fd, "kernel", "zImage", "2.6.22");
	close(fd);
*/

//	fiasco_callback = &my_callback;

	return openfiasco(argv[1]);
}
#endif
