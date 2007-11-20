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
#include "hexdump.h"
#include <usb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Extracts a piece from an mtd device
 * -----------------------------------
 * This function is known to be buggy, It does not takes care about
 * badblocks and oob data. It is replaced by nanddump(), but will
 * probably be fixed in the future or it will die.
 */
int rf_extract(char *dev, off_t from, off_t to, char *file)
{
	off_t i, blk = 0xfffff;
	char buf[0xfffff];
	FILE *fs = fopen(dev, "rb");
	FILE *fd = fopen(file, "wb+");

	if (from>to) {
		printf("Bad range %lld - %lld for %s\n", from, to, file);
		goto __rf_extract_exit;
	}
	if (fs == NULL) { perror(dev);  goto __rf_extract_exit; }
	if (fd == NULL) { perror(file); goto __rf_extract_exit; }

	printf("Extracting %s...\n", file);
	fseek(fs, from, SEEK_SET);

	for(i=from;i<to;i+=blk) {
		int ret;
		if (i+blk>to) blk = to-i;
		ret = fread(buf, blk, 1, fs);
		progressbar(i, to);
		if (!ret || feof(fs)) break;
		fwrite(buf, blk, 1, fd);
	}

	printf("\r%s: %lld bytes dumped from %s\n",
		file, to-from, dev);

__rf_extract_exit:
	if (fs) fclose(fs);
	if (fd) fclose(fd);

	return 1;
}

/*
 * This function was covardly copied from nanddump.c @ mtd-utils-20060907
 */
#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <asm/types.h>
#include <mtd/mtd-user.h>

#define M_RDONLY   0x00000001
#define M_RDRW     0x00000002
#define M_OMITOOB  0x00000010
#define M_OMITBAD  0x00000011
#define M_OMITECC  0x00000012

int mtd_open(char *file, mtd_info_t *meminfo, int *oobinfochanged, 
	struct nand_oobinfo *old_oobinfo, int *eccstats, int flags)
{
	int fd;
	*oobinfochanged = 0 ;
	
	fd = open(file, (flags&M_RDONLY)?O_RDONLY:O_RDWR);
	if (fd == -1) {
		perror("mtd_open");
		return -1;
	}
	/* Fill in MTD device capability structure */
	if (ioctl(fd, MEMGETINFO, meminfo) != 0) {
		perror("MEMGETINFO");
		close(fd);
		return 1;
	}

	/* Make sure device page sizes are valid */
	if (!(meminfo->oobsize == 64 && meminfo->writesize == 2048) &&
	    !(meminfo->oobsize == 16 && meminfo->writesize == 512) &&
	    !(meminfo->oobsize == 8  && meminfo->writesize == 256)) {
		fprintf(stderr, "Unknown flash (not normal NAND)\n");
		close(fd);
		return -1;
	}

	return fd;
}

int mtd_close(int fd, struct nand_oobinfo *old_oobinfo, int oobinfochanged)
{
	if (fd == -1)
		return 1;

	/* reset oobinfo */
	if (oobinfochanged == 1) {
		if (ioctl (fd, MEMSETOOBSEL, &old_oobinfo) != 0) {
			perror ("MEMSETOOBSEL");
			close(fd);
			return 1;
		}
	}
	/* Close the output file and MTD device */
	return close(fd);
}

// configuration for nanddump //
//int	noecc = 0;		// don't error correct
int	omitoob = 1;		// omit oob data
int	omitbad = 1;
// configuration for nanddump //

#define CONFIGURE_FLAGS(x) \
omitoob = x & M_OMITOOB; \
omitbad = x & M_OMITBAD;

int check_badblocks(char *mtddev)
{
	int fd;
	int oobinfochanged = 0 ;
	int badblock = 0;
	int badblocks = 1;
	int eccstats = 0;
	unsigned long int i;
	unsigned long long blockstart = 1;
	unsigned char oobbuf[64];
	struct nand_oobinfo old_oobinfo;
	struct mtd_oob_buf oob = {0, 16, oobbuf};
	struct mtd_ecc_stats stat1, stat2;
	mtd_info_t meminfo;

	fd = mtd_open(mtddev, &meminfo, &oobinfochanged, &old_oobinfo, &eccstats, M_RDONLY);

	if (fd == -1) {
		printf("cannot open mtd\n");
		return 1;
	}

        fprintf(stderr, "Block size %u, page size %u, OOB size %u\n",
                meminfo.erasesize, meminfo.writesize, meminfo.oobsize);
        fprintf(stderr, "Size %u, flags %u, type 0x%x\n",
                meminfo.size, meminfo.flags, (int)meminfo.type);


	oob.length = meminfo.oobsize;
	for(i = 0; i < meminfo.size; i+= meminfo.writesize) {

		// new eraseblock , check for bad block
		if (blockstart != (i & (~meminfo.erasesize + 1))) {
			blockstart = i & (~meminfo.erasesize + 1);
			if ((badblock = ioctl(fd, MEMGETBADBLOCK, &blockstart)) < 0) {
				perror("ioctl(MEMGETBADBLOCK)");
				goto closeall;
			}
		}

		if (badblock) {
			if (omitbad) {
				printf("Bad block found at 0x%lx\n", i);
				continue;
			}
		} else {
			char readbuf[2048]; // XXX hardcoded like mtd-utils?? ugly!
			// dummy -- should be removed
			if (pread(fd, readbuf, meminfo.writesize, i) != meminfo.writesize) {
				perror("pread");
				goto closeall;
			}
		}

		/* ECC stats available ? */
		if (eccstats) {
			if (ioctl(fd, ECCGETSTATS, &stat2)) {
				perror("ioctl(ECCGETSTATS)");
				goto closeall;
			}
			if (stat1.failed != stat2.failed)
				fprintf(stderr, "ECC: %d uncorrectable bitflip(s)"
					" at offset 0x%08lx\n",
					stat2.failed - stat1.failed, i);
			if (stat1.corrected != stat2.corrected)
				fprintf(stderr, "ECC: %d corrected bitflip(s) at"
					" offset 0x%08lx\n",
					stat2.corrected - stat1.corrected, i);
			stat1 = stat2;
		}

		if (badblock) {
			printf("Oops badblock %d at 0x%lx !\n", badblocks++, i);
		} else {
			/* Read OOB data and exit on failure */
			oob.start = i;
			if (ioctl(fd, MEMREADOOB, &oob) != 0) {
				perror("ioctl(MEMREADOOB)");
				goto closeall;
			}
		}

		/* Write out OOB data */
		if (badblock)
		D dump_bytes(oobbuf, meminfo.oobsize);
	}

	mtd_close(fd, &old_oobinfo, oobinfochanged);
	return 0;

closeall:
	mtd_close(fd, &old_oobinfo, oobinfochanged);
	return 1;
}

// XXX warning ioob is not used
int nanddump(char *mtddev, unsigned long start_addr, unsigned long length, char *dumpfile, int isbl, int ioob)
{
	unsigned char readbuf[2048];
	int oobinfochanged = 0 ;
	unsigned char oobbuf[64];
	unsigned long ofs, end_addr = 0;
	unsigned long long blockstart = 1;
	struct nand_oobinfo none_oobinfo = {
		.useecc = MTD_NANDECC_OFF,
	};
	int fd, ofd, bs, badblock = 0;
	struct mtd_oob_buf oob = {0, 16, oobbuf};
	mtd_info_t meminfo;
	int badblocks = 1;
	struct nand_oobinfo old_oobinfo;
	int eccstats = 0;
	struct mtd_ecc_stats stat1, stat2;
	int flags = M_RDONLY;

	printf("\nExtracting %s from %s...\n", dumpfile, mtddev);

	fd = mtd_open(mtddev, &meminfo, &oobinfochanged, &old_oobinfo, &eccstats, flags);

	if (meminfo.writesize == 0) {
		printf("What the fuck.. this is not an MTD device!\n");
		close(fd);
		return 1;
	}

	/* Read the real oob length */
	oob.length = meminfo.oobsize;
        fprintf(stderr, "Block size %u, page size %u, OOB size %u\n",
                meminfo.erasesize, meminfo.writesize, meminfo.oobsize);
        fprintf(stderr, "Size %u, flags %u, type 0x%x\n",
                meminfo.size, meminfo.flags, (int)meminfo.type);

	if (flags & M_OMITECC)  { // (noecc)
		switch (ioctl(fd, MTDFILEMODE, (void *) MTD_MODE_RAW)) {
		case -ENOTTY:
			if (ioctl (fd, MEMGETOOBSEL, old_oobinfo) != 0) {
				perror ("MEMGETOOBSEL");
				close (fd);
				exit (1);
			}
			if (ioctl (fd, MEMSETOOBSEL, none_oobinfo) != 0) {
				perror ("MEMSETOOBSEL");
				close (fd);
				exit (1);
			}
			oobinfochanged = 1;
			break;

		case 0:
			oobinfochanged = 2;
			break;
		default:
			perror ("MTDFILEMODE");
			close (fd);
			return 1;
		}
	} else {
		/* check if we can read ecc stats */
		if (!ioctl(fd, ECCGETSTATS, &stat1)) {
			eccstats = 1;
			fprintf(stderr, "ECC failed: %d\n", stat1.failed);
			fprintf(stderr, "ECC corrected: %d\n", stat1.corrected);    
			fprintf(stderr, "Number of bad blocks: %d\n", stat1.badblocks);    
			fprintf(stderr, "Number of bbt blocks: %d\n", stat1.bbtblocks);    
		} else
			perror("No ECC status information available");
	}

	/* Open output file for writing. If file name is "-", write to standard
	 * output. */
	if (!dumpfile) {
		ofd = STDOUT_FILENO;
	} else if ((ofd = open(dumpfile, O_WRONLY | O_TRUNC | O_CREAT, 0644))== -1) {
		perror ("open outfile");
		close(fd);
		return 1;
	}

	/* Initialize start/end addresses and block size */
	if (length)
		end_addr = start_addr + length;
	if (!length || end_addr > meminfo.size)
		end_addr = meminfo.size;

	bs = meminfo.writesize;

	fprintf(stderr,
		"Dumping data starting at 0x%08x and ending at 0x%08x...\n",
		(unsigned int) start_addr, (unsigned int) end_addr);

	/* Dump the flash contents */
	for (ofs = start_addr; ofs < end_addr ; ofs+=bs) {
		// new eraseblock , check for bad block
		if (blockstart != (ofs & (~meminfo.erasesize + 1))) {
			blockstart = ofs & (~meminfo.erasesize + 1);
			if ((badblock = ioctl(fd, MEMGETBADBLOCK, &blockstart)) < 0) {
				perror("ioctl(MEMGETBADBLOCK)");
				goto closeall;
			}
		}

		if (!isbl && badblock) {
			if (omitbad)
				continue;
			memset (readbuf, 0xff, bs);
		} else {
			/* Read page data and exit on failure */
			if (pread(fd, readbuf, bs, ofs) != bs) {
				perror("pread");
				goto closeall;
			}
		}

		// TODO exist on n800??? // remove code?
		/* ECC stats available ? */
		if (eccstats) {
			if (ioctl(fd, ECCGETSTATS, &stat2)) {
				perror("ioctl(ECCGETSTATS)");
				goto closeall;
			}
			if (stat1.failed != stat2.failed)
				fprintf(stderr, "ECC: %d uncorrectable bitflip(s)"
					" at offset 0x%08lx\n",
					stat2.failed - stat1.failed, ofs);
			if (stat1.corrected != stat2.corrected)
				fprintf(stderr, "ECC: %d corrected bitflip(s) at"
					" offset 0x%08lx\n",
					stat2.corrected - stat1.corrected, ofs);
			stat1 = stat2;
		}

		/* Write out page data */
		//if (pretty_print) dump_bytes(readbuf, bs);
		progressbar(ofs, end_addr);

		// OOB STUFF //
		if (omitoob)
			continue;

		if (!isbl&&badblock) {
			printf("Oops badblock %d *ignored* at 0x%lx !\n", badblocks++, ofs);
			memset (readbuf, 0xff, meminfo.oobsize);
		} else {
			/* Read OOB data and exit on failure */
			oob.start = ofs;
			if (ioctl(fd, MEMREADOOB, &oob) != 0) {
				perror("ioctl(MEMREADOOB)");
				goto closeall;
			}
		}

		/* Write out OOB data */
		D dump_bytes(oobbuf, meminfo.oobsize);
		write(ofd, oobbuf, meminfo.oobsize);
	}

	mtd_close(fd, &old_oobinfo, oobinfochanged);
	close(ofd);

	return 0;

 closeall:
	mtd_close(fd, &old_oobinfo, oobinfochanged);
	close(ofd);
	return 1;
}

int rf_strip(char *file)
{
	FILE *fd = fopen(file, "rw");
	unsigned char buf[4096];
	int ret;
	off_t eof = 0;

	if (!fd) {
		fprintf(stderr, "Weird! can't open %s???\n", file);
		exit(0);
	}
	printf("Stripping padding from %s... ", file);
	fflush(stdout);
	fseek(fd, 0, SEEK_END);
	if (ftell(fd)>4096) {
		do {
			fseek(fd, -4096, SEEK_CUR);
			ret = fread(buf, 1, 4096, fd);
			fseek(fd, -4096, SEEK_CUR);
			for(ret--;ret>0;ret--)
				if (buf[ret]!=0xff) {
					fseek(fd, ret+1, SEEK_CUR);
					eof = ftell(fd);
					goto __done;
				}
		} while(1);
	}

__done:
	fclose(fd);
	if (eof) truncate(file, eof);
	printf("done at %d\n", (int)eof);

	return 1;
}

int is_n800()
{
	int n800 = 0;
	unsigned char buf[4];
	FILE *fd = fopen("/dev/mtd0", "rb");

	if (!fd) {
		fprintf(stderr, "Cannot open /dev/mtd0.\n");
		exit(1);
	}
	fread(buf, 4, 1,fd);
	if (!memcmp("\xff\xff\xff\xff", buf, 4))
		n800 = 1;
	
	fclose(fd);

	return n800;
}

int dump_config()
{
	mtd_info_t meminfo;
	int fd = open("/dev/mtd1", O_RDONLY);
	char buf[1024];
	int i=0,j;
	int ret;


	if (fd == -1) {
		perror("open /dev/mtd1");
		return 1;
	}
	if (ioctl(fd, MEMGETINFO, &meminfo) != 0) {
		perror("MEMGETINFO");
		close(fd);
		return 1;
	}

	while(i<0x60000) {
	iniloop:
		ret = read(fd, buf, 4);
		if (ret == -1)
			break;
		if (!memcmp(buf,"ConF", 4)) {
		loop:
			read(fd, buf, 4);
			if (ret == -1) break;
			printf("\n0x%08x : ConF %02x %02x %02x %02x : ", i,
				buf[0], buf[1], buf[2], buf[3]);
			fflush(stdout);
			while(1) {
				ret = read(fd, buf, 4);
				if (ret <1) {
					printf("Unexpected eof\n");
					break;
				}
				if (buf[0]=='\0'||buf[1]=='\0'||buf[2]=='\0'||buf[3]=='\0') {
					printf("%s  \t  ", buf);
					fflush(stdout);
					while(1) {
						ret = read(fd, buf, 4);
						if (ret <1) {
							printf("Unexpected eof\n");
							break;
						}
						if (!memcmp(buf,"\xff\xff\xff\xff", 4))
							goto iniloop;
						if (!memcmp(buf,"ConF", 4))
							goto loop;
						printf ("%02x %02x %02x %02x ",
							buf[0], buf[1], buf[2], buf[3]);
					}
					break;
				} else {
					write(1, buf,4);
				}
			}
		}
		j = lseek(fd, 0, SEEK_CUR);
		if (j==i) break; i = j;
	}
	close(fd);
	printf("\n");
	return 0;
}

int reverse_extract_pieces(char *dir)
{
	char reply;
	chdir(dir);

	// TODO: get values from /proc/mtd ???

	//rf_extract("/dev/mtd0", is_n800()?0x200:0, 0x003600, "xloader.bin");
	nanddump("/dev/mtd0", is_n800()?0x200:0, 0x4000, "xloader.bin", 1,0); // 0x3600 size?
	//rf_extract("/dev/mtd0", 0x004000, 0x01ffff,  "secondary.bin");
	nanddump("/dev/mtd0", 0x004000, 0,  "secondary.bin", 1,0);
	nanddump("/dev/mtd1", 0, 0,  "config.bin", 0,0);
	//rf_extract("/dev/mtd2", 0x000800, 0x200000,  "zImage");
	nanddump("/dev/mtd2", 0x000800, 0,  "zImage", 0,0);
	//rf_extract("/dev/mtd3", 0x000000, 0x1D00000, "initfs.jffs2");
	nanddump("/dev/mtd3", 0x000000, 0, "initfs.jffs2", 0,0);

	printf("\n\nExtract rootfs? (y/N): "); fflush(stdout);
	read(0, &reply, 1);
	if (reply=='y'||reply=='Y') {
		//rf_extract("/dev/mtd4", 0x000000, 0x6000000, "rootfs.jffs2");
		nanddump("/dev/mtd4", 0x000000, 0, "rootfs.jffs2", 0,0);
	} else	printf("*** Ignoring rootfs\n");

	printf("\n\nStrip dumped files? (y/N): "); fflush(stdout);
	read(0, &reply, 1);
	if (reply=='y'||reply=='Y') {
		rf_strip("xloader.bin");
		rf_strip("secondary.bin");
		rf_strip("zImage");
		rf_strip("initfs.jffs2");
	} else  printf("*** Ignoring strip\n");

	printf("\nIdentifying extracted files...\n");
	printf("%s: xloader\n", fpid_file("xloader.bin"));
	printf("%s: secondary.bin\n", fpid_file("secondary.bin"));
	printf("%s: config.bin\n", fpid_file("config.bin"));
	printf("%s: zImage\n", fpid_file("zImage"));
	printf("%s: initfs.jffs2\n", fpid_file("initfs.jffs2"));
	printf("%s: rootfs.jffs2\n", fpid_file("rootfs.jffs2"));

	return 1;
}
