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

// configuration for nanddump //
int	ignoreerrors = 1;	// ignore errors
int	pretty_print = 0;	// print nice in ascii
int	noecc = 0;		// don't error correct
int	omitoob = 1;		// omit oob data
int	omitbad = 1;
// configuration for nanddump //

int nanddump(char *mtddev, unsigned long start_addr, unsigned long length, char *dumpfile)
{
	unsigned char readbuf[2048];
	unsigned char oobbuf[64];
	struct nand_oobinfo none_oobinfo = {
		.useecc = MTD_NANDECC_OFF,
	};
	unsigned long ofs, end_addr = 0;
	unsigned long long blockstart = 1;
	int i, fd, ofd, bs, badblock = 0;
	struct mtd_oob_buf oob = {0, 16, oobbuf};
	mtd_info_t meminfo;
	char pretty_buf[80];
	int oobinfochanged = 0 ;
	int badblocks = 1;
	struct nand_oobinfo old_oobinfo;
	struct mtd_ecc_stats stat1, stat2;
	int eccstats = 0;

	printf("\nExtracting %s from %s...\n", dumpfile, mtddev);

	/* Open MTD device */
	if ((fd = open(mtddev, O_RDONLY)) == -1) {
		perror("open flash");
		return 1;
	}

	/* Fill in MTD device capability structure */
	if (ioctl(fd, MEMGETINFO, &meminfo) != 0) {
		perror("MEMGETINFO");
		close(fd);
		return 1;
	}

	/* Make sure device page sizes are valid */
	if (!(meminfo.oobsize == 64 && meminfo.writesize == 2048) &&
	    !(meminfo.oobsize == 16 && meminfo.writesize == 512) &&
	    !(meminfo.oobsize == 8 && meminfo.writesize == 256)) {
		fprintf(stderr, "Unknown flash (not normal NAND)\n");
		close(fd);
		return 1;
	}
	/* Read the real oob length */
	oob.length = meminfo.oobsize;

	if (noecc)  {
		switch (ioctl(fd, MTDFILEMODE, (void *) MTD_MODE_RAW)) {
		case -ENOTTY:
			if (ioctl (fd, MEMGETOOBSEL, &old_oobinfo) != 0) {
				perror ("MEMGETOOBSEL");
				close (fd);
				exit (1);
			}
			if (ioctl (fd, MEMSETOOBSEL, &none_oobinfo) != 0) {
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

	/* Print informative message */
	fprintf(stderr, "Block size %u, page size %u, OOB size %u\n",
		meminfo.erasesize, meminfo.writesize, meminfo.oobsize);
	fprintf(stderr,
		"Dumping data starting at 0x%08x and ending at 0x%08x...\n",
		(unsigned int) start_addr, (unsigned int) end_addr);

	/* Dump the flash contents */
	for (ofs = start_addr; ofs < end_addr ; ofs+=bs) {

		progressbar(ofs, end_addr);

		// new eraseblock , check for bad block
		if (blockstart != (ofs & (~meminfo.erasesize + 1))) {
			blockstart = ofs & (~meminfo.erasesize + 1);
			if ((badblock = ioctl(fd, MEMGETBADBLOCK, &blockstart)) < 0) {
				perror("ioctl(MEMGETBADBLOCK)");
				goto closeall;
			}
		}

		if (badblock) {
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
		if (pretty_print) {
			for (i = 0; i < bs; i += 16) {
				sprintf(pretty_buf,
					"0x%08x: %02x %02x %02x %02x %02x %02x %02x "
					"%02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
					(unsigned int) (ofs + i),  readbuf[i],
					readbuf[i+1], readbuf[i+2],
					readbuf[i+3], readbuf[i+4],
					readbuf[i+5], readbuf[i+6],
					readbuf[i+7], readbuf[i+8],
					readbuf[i+9], readbuf[i+10],
					readbuf[i+11], readbuf[i+12],
					readbuf[i+13], readbuf[i+14],
					readbuf[i+15]);
				write(ofd, pretty_buf, 60);
			}
		} else
			write(ofd, readbuf, bs);


		if (badblock) {
			printf("Oops badblock %d at 0x%lx !\n", badblocks++, ofs);
			memset (readbuf, 0xff, meminfo.oobsize);
		} else {
			/* Read OOB data and exit on failure */
			oob.start = ofs;
			if (ioctl(fd, MEMREADOOB, &oob) != 0) {
				perror("ioctl(MEMREADOOB)");
				goto closeall;
			}
		}

		if (omitoob)
			continue;

		/* Write out OOB data */
		if (pretty_print) {
			if (meminfo.oobsize < 16) {
				sprintf(pretty_buf, "  OOB Data: %02x %02x %02x %02x %02x %02x "
					"%02x %02x\n",
					oobbuf[0], oobbuf[1], oobbuf[2],
					oobbuf[3], oobbuf[4], oobbuf[5],
					oobbuf[6], oobbuf[7]);
				write(ofd, pretty_buf, 48);
				continue;
			}

			for (i = 0; i < meminfo.oobsize; i += 16) {
				sprintf(pretty_buf, "  OOB Data: %02x %02x %02x %02x %02x %02x "
					"%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
					oobbuf[i], oobbuf[i+1], oobbuf[i+2],
					oobbuf[i+3], oobbuf[i+4], oobbuf[i+5],
					oobbuf[i+6], oobbuf[i+7], oobbuf[i+8],
					oobbuf[i+9], oobbuf[i+10], oobbuf[i+11],
					oobbuf[i+12], oobbuf[i+13], oobbuf[i+14],
					oobbuf[i+15]);
				write(ofd, pretty_buf, 60);
			}
		} else
			write(ofd, oobbuf, meminfo.oobsize);
	}

	/* reset oobinfo */
	if (oobinfochanged == 1) {
		if (ioctl (fd, MEMSETOOBSEL, &old_oobinfo) != 0) {
			perror ("MEMSETOOBSEL");
			close(fd);
			close(ofd);
			return 1;
		}
	}
	/* Close the output file and MTD device */
	close(fd);
	close(ofd);

	/* Exit happy */
	return 0;

 closeall:
	/* The new mode change is per file descriptor ! */
	if (oobinfochanged == 1) {
		if (ioctl (fd, MEMSETOOBSEL, &old_oobinfo) != 0)  {
			perror ("MEMSETOOBSEL");
		}
	}
	close(fd);
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

int reverse_extract_pieces(char *dir)
{
	char reply;
	chdir(dir);

	nanddump("/dev/mtd0", is_n800()?0x200:0, 0x003600, "xloader.bin");
	//rf_extract("/dev/mtd0", is_n800()?0x200:0, 0x003600, "xloader.bin");
	nanddump("/dev/mtd0", 0x004000, 0x01ffff,  "secondary.bin");
	//rf_extract("/dev/mtd0", 0x004000, 0x01ffff,  "secondary.bin");
	nanddump("/dev/mtd2", 0x000800, 0x200000,  "zImage");
	//rf_extract("/dev/mtd2", 0x000800, 0x200000,  "zImage");
	nanddump("/dev/mtd3", 0x000000, 0x1D00000, "initfs.jffs2");
	//rf_extract("/dev/mtd3", 0x000000, 0x1D00000, "initfs.jffs2");
	printf("\nWARNING: the rootfs extraction on n800 is known to be buggy! feedback is welcome.\n\n");
	printf("Extract rootfs? (y/N): "); fflush(stdout);
	read(0, &reply, 1);
	if (reply=='y'||reply=='Y') {
		nanddump("/dev/mtd4", 0x000000, 0x6000000, "rootfs.jffs2");
		//rf_extract("/dev/mtd4", 0x000000, 0x6000000, "rootfs.jffs2");
	}
	else	printf("*** Ignoring rootfs\n");
	rf_strip("xloader.bin");
	rf_strip("secondary.bin");
	rf_strip("zImage");
//	rf_strip("initfs.jffs2"); 	// do not strip initfs, is 2MB long
					//and can be useful for data recovery
	printf("Identifying extracted files...\n");
	printf("%s: xloader\n", fpid_file("xloader.bin"));
	printf("%s: secondary.bin\n", fpid_file("secondary.bin"));
	printf("%s: zImage\n", fpid_file("zImage"));
	printf("%s: initfs.jffs2\n", fpid_file("initfs.jffs2"));

	return 1;
}
