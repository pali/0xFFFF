/*
 *  0xFFFF - Open Free Fiasco Firmware Flasher
 *  Copyright (C) 2007-2011  pancake <pancake@youterm.com>
 *  Copyright (C) 2011-2012  Pali Rohár <pali.rohar@gmail.com>
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
#include "hash.h"

int (*fiasco_callback)(struct header_t *header) = NULL;

int openfiasco(const char *name, const char *piece_grep, int v)
{
	struct header_t header;
	unsigned char buf[256];
	unsigned char data[256];
	unsigned char *pdata, *pdataend;
	unsigned int namelen;
	off_t off, here;
	int i;

	memset(&header, 0, sizeof(header));
	header.fd = open(name, O_RDONLY);

	if (header.fd == -1) {
		fprintf(stderr, "Cannot open %s\n", name);
		return 1;
	}

	/* read header */
	if (read(header.fd, buf, 5) != 5) {
		printf("Invalid read of 5 bytes\n");
		return close(header.fd);
	}
	if (buf[0] != 0xb4) {
		printf("Invalid header\n");
		return close(header.fd);
	}
	memcpy(&namelen,buf+1,4);
	namelen = ntohl(namelen);
	if (namelen>128) {
		printf("Stupid length at header. Is this a joke?\n");
		return close(header.fd);
	}
	memset(buf,'\0', 128);
	if (read(header.fd, buf, namelen) != namelen) {
		printf("Invalid read of %d bytes\n", namelen);
		return close(header.fd);
	}
	if (v) printf("Fiasco version: %2d\n", buf[3]);
	strcpy(header.fwname, (char *)buf+6);
	if (v) for(i=6;i<namelen;i+=strlen((char *)(buf+i))+1)
			printf("Name: %s\n", buf+i);

	/* walk the tree */
	while(1) {
		here = lseek(header.fd, 0, SEEK_CUR);

		i = 0;
		while(1) {
			if (read(header.fd, buf, 7)<7) {
				printf("Next valid header not found\n");
				return close(header.fd);
			}
			if (buf[0] == 0x54 && buf[2] == 0x2E && buf[3] == 0x19 && buf[4] == 0x01 && buf[5] == 0x01 && buf[6] == 0x00)
				break;
			lseek(header.fd, -6, SEEK_CUR);
			++i;
		}
		if (i && v) printf("Skipping %d padding bytes\n", i);

		if (read(header.fd, buf+7, 2)<2)
			break;
		header.hash = buf[7]<<8|buf[8];

		/* piece name */
		memset(data, '\0', 13);
		if (read(header.fd, data, 12)<12)
			break;

		if (data[0] == 0xff) {
			printf(" [eof]\n");
			break;
		} else if (v) printf(" %s\n", data);
		strcpy(header.type, (char *)data);

		if (v)  {
			printf("   header: ");
			for (i=0; i<7;++i) printf(" 0x%.2X", (unsigned int)buf[i]);
			printf("\n");
		}

		if (read(header.fd, buf, 9)<9)
			return close(header.fd);
		memcpy(&header.size, buf,4);
		header.size = ntohl(header.size);
		if (v)  {
			printf("   offset:  0x%08x\n", (unsigned int)here);
			printf("   size:    %d bytes\n", header.size);
			printf("   hash:    %04x\n", header.hash);
		}
		//printf("BYTE: %02x %02x %02x %02x %02x\n", 
		//	buf[4], buf[5], buf[6], buf[7], buf[8]);
		/* XXX this is not ok */
		//printf("BUF8: %02x\n", buf[8]);
		memset(header.device, 0, sizeof(header.device));
		memset(header.hwrevs, 0, sizeof(header.hwrevs));
		memset(header.version, 0, sizeof(header.version));
		if (header.layout) {
			free(header.layout);
			header.layout = NULL;
		}
		while ((buf[8] >= '1' && buf[8] <= '4') || buf[8] == '/') {
			if (read(header.fd, data, 1)<1)
				return close(header.fd);
			i = data[0];
			if (read(header.fd, data, i)<i)
				return close(header.fd);
			if (data[0]) {
				if (v) {
					printf("   subinfo\n");
					printf("     type: ");
					if (buf[8] == '1') printf("version string");
					else if (buf[8] == '2') printf("hw revision");
					else if (buf[8] == '3') printf("layout");
					else printf("unknown (%c)", buf[8]);
					printf("\n");
					printf("     length: %d\n", i);
				}
				pdata = data;
				pdataend = data+i;
				while(pdata<pdataend) {
					char buf2[9];
					if (buf[8] == '2' && pdata != data) {
						memset(buf2, 0, 9);
						strncpy(buf2, (char *)pdata, 8);
					}
					if (v) {
						printf("       ");
						if (buf[8] == '1') printf("version");
						else if (buf[8] == '2' && pdata == data) printf("device");
						else if (buf[8] == '2' && pdata != data) printf("hw revision");
						else if (buf[8] == '3') printf("layout");
						else printf("data");
						if (buf[8] == '2' && pdata != data)
							printf(": %s\n", buf2);
						else if (buf[8] != '3' && buf[8] != '/')
							printf(": %s\n", pdata);
						else
							printf(": (not printing)\n");
					}
					if (buf[8] == '1') {
						strcpy(header.version, (char *)pdata);
					} else if (buf[8] == '2' && pdata == data) {
						strcpy(header.device, (char *)pdata);
					} else if (buf[8] == '2' && pdata != data) {
						if (header.hwrevs[0] == 0)
							strcpy(header.hwrevs, buf2);
						else {
							strcat(header.hwrevs, ",");
							strcat(header.hwrevs, buf2);
						}
					} else if (buf[8] == '3') {
						if (header.layout) free(header.layout);
						header.layout = malloc(strlen((char*)pdata)+1);
						if (header.layout) strcpy(header.layout, (char *)pdata);
					}
					if (buf[8] == '2' && pdata != data && strlen((char *)pdata) > 8)
						pdata += 8;
					else
						pdata += strlen((char*)pdata)+1;
					for(;*pdata=='\0' && pdata<pdataend; pdata++);
				}
			} else {
				/* End of comments, ignore one char and next is image data */
				lseek(header.fd, 1, SEEK_CUR);
				break;
			}
			if (read(header.fd, buf+8, 1)<1)
				return close(header.fd);
		}
		strcpy(header.name, header.type);
		if (header.device[0]) {
			strcat(header.name, "-");
			strcat(header.name, header.device);
		}
		if (header.hwrevs[0]) {
			strcat(header.name, "-");
			strcat(header.name, header.hwrevs);
		}
		if (header.version[0]) {
			strcat(header.name, "-");
			strcat(header.name, header.version);
		}
		/* callback */
		off = lseek(header.fd, 0, SEEK_CUR);
		if (v) {
			printf("   version: %s\n", header.version);
			printf("   device: %s\n", header.device);
			printf("   hwrevs: %s\n", header.hwrevs);
			printf("   name: %s\n", header.name);
			printf("   body-at:   0x%08x\n", (unsigned int)off);
		}
		if (piece_grep==NULL || (strstr(header.name, piece_grep))) {
			if (piece_grep)
				printf("==> (%s) %s\n", piece_grep, header.name);
			else
				printf("==> %s\n", header.name);
			if (fiasco_callback != NULL) {
				fiasco_callback(&header);
				if (header.layout) {
					free(header.layout);
					header.layout = NULL;
				}
				free(header.data);
				continue;
			} else {
				// ??huh
			}
		}
		// XXX dup
		lseek(header.fd, off, SEEK_SET);
		lseek(header.fd, header.size, SEEK_CUR);
		if (header.layout) {
			free(header.layout);
			header.layout = NULL;
		}
	}
	return close(header.fd);
}

void fiasco_data_read(struct header_t *header)
{
	header->data = (unsigned char *)malloc(header->size);
	if (header->data == NULL) {
		printf("Cannot alloc %d bytes\n", header->size);
		return;
	}
	if (read (header->fd, header->data, header->size) != header->size) {
		printf("Cannot read %d bytes\n", header->size);
		return;
	}
}

/* fiasco writer */
int fiasco_new(const char *filename, const char *name)
{
	int fd;
	//int len = htonl(strlen(name)+1+6+14);

	fd = open(filename, O_RDWR|O_CREAT, 0644);
	if (fd == -1)
		return -1;
#if 1
	if (write(fd, "\xb4\x00\x00\x00\x14\x00\x00\x00\x01\xe8\x0e", 11) != 11) {
		fprintf (stderr, "Cannot write 11 bytes to target file\n");
		close (fd);
		return -1;
	}
	if (write(fd, "OSSO UART+USB", 14) != 14) {
		fprintf (stderr, "Cannot write 14 bytes to target file\n");
		close (fd);
		return -1;
	}
#else
	/* 2nd format doesnt works. atm stay with old one */
	write(fd, "\xb4", 1);
	write(fd, &len, 4);
	/* version header */
	write(fd, "\x00\x00\x00\x02\xe8\x0e", 6);
	/* firmware type */
	write(fd, "OSSO UART+USB", 14);
	/* firmware name */
	write(fd, name, strlen(name));
	write(fd, "", 1);
#endif
	return fd;
}

int fiasco_add_eof(int fd)
{
#if 0
	unsigned char buf[120];
	if (fd == -1)
		return -1;
	memset(buf,'\xff', 120);
	write(fd, buf, 120);
#endif
	return 0;
}

int fiasco_add(int fd, const char *name, const char *file, const char *version)
{
	int gd,ret;
	unsigned int sz;
	unsigned char len;
	unsigned short hash;
	unsigned char *ptr = (unsigned char *)&hash;
	char buf[4096];
	char bname[32];

	if (fd == -1)
		return -1;
	if (file == NULL)
		return -1;
	gd = open(file, O_RDONLY);
	if (gd == -1)
		return -1;

	sz = htonl((unsigned int) lseek(gd, 0, SEEK_END));
	lseek(gd, 0, SEEK_SET);
	// 4 bytes big endian
	//write(fd, "T\x02\x2e\x19\x01\x01\x00", 7); // header?
	// header (old piece format)
	if (write (fd, "T\x01\x2e\x19\x01\x01\x00", 7) != 7) {
		fprintf (stderr, "Cannot write 7 bytes\n");
		return -1;
	}
	/* checksum */
	hash = do_hash_file(file);
	ptr[0]^=ptr[1]; ptr[1]=ptr[0]^ptr[1]; ptr[0]^=ptr[1];
	write(fd, &hash, 2);
	printf("hash: %04x\n", hash);

	memset(bname, '\0', 13);
	if (name == NULL)
		name = fpid_file(file);
	/* failback */
	if (name == NULL)
		name = file;

	strncpy(bname, name, 12);
	write(fd, bname, 12);
	write(fd, &sz, 4);
	if (version) {
		/* append metadata */
		write(fd, "\x00\x00\x00\x00\x31", 5);
		len = strlen(version);
		write(fd, &len, 1);
		write(fd, version, len);
		write(fd, "\x00\x9b", 2);
	} else {
		write(fd, "\x00\x00\x00\x00\x9b", 5);
	}

	while(1) {
		ret = read(gd, buf, 4096);
		if (ret<1)
			break;
		if (write(fd, buf, ret) != ret) {
			fprintf (stderr, "Cannot write %d bytes\n", ret);
			return -1;
		}
	}

	return 0;
}

int fiasco_pack(int optind, char *argv[])
{
	const char *file = argv[optind];
	char *type;
	int fd, ret;

	fd = fiasco_new(file, file); // TODO use a format here
	if (fd == -1)
		return 1;

	printf("Package: %s\n", file);
	while((file=argv[++optind])) {
		type = (char *)fpid_file(file);
		printf("Adding %s: %s..\n", type, file);
		ret = fiasco_add(fd, type, file, NULL);
		if (ret<0) {
			printf("Error\n");
			close(fd);
			return 1;
		}
	}
	fiasco_add_eof(fd);
	printf("Done!\n");
	close(fd);
	return 0;
}

/* local code */
#if 0
void my_callback(int fd, struct header_t *header)
{
	fiasco_data_read(header);
	//read(fd, buf, header->size);
	printf("Dumping %s\n", header->name);
	printf("DATA: %02x\n", header->data[0]);
	fiasco_data_free(header);
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
