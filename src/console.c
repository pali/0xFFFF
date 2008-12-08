/*
 *  0xFFFF - Open Free Fiasco Firmware Flasher
 *  Copyright (C) 2007, 2008  pancake <pancake@youterm.com>
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

#if HAVE_USB
#include "main.h"
#include "query.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

void cmd_exit(char *line)
{
	exit(0);
}

void cmd_help(char *line)
{
	printf("connect           connects via usb to nolo\n");
	printf("reboot            reboots remote host\n");
	printf("info              shows info of the remote system\n");
	printf("linfo             shows info of the local system\n");
	printf("shell             opens a shell (/bin/sh)\n");
	printf("badblocks [dev]   checks bad blocks on mtd (/dev/mtd1)\n");
	printf("dump [dir]        dumps the contents of /dev/mtd to dir\n");
	printf("nanddump [dev] [start] [len] [out] [ignore-badblocks] [ignore-oob]\n");
	printf("                  f.ex: nanddump /dev/mtd0 0x0 0x4000 xloader.bin 1 1\n");
	printf("exit              exits the shell\n");
	fflush(stdout);
}

void cmd_info(char *line)
{
	char *p, str[128];
	get_sw_version();
	get_hw_revision(str, 128); // get hardware revision:
	p = strstr(str, "hw_rev:");
	if (p) // TODO: delimit string by comma
		printf("SubVersionString autodetected: '%s'\n", p+7);
	else printf("SubVersionString autodetected: (error)\n");
	get_root_device(); // only for flashing
	get_usb_mode();
	get_rd_mode();
	get_rd_flags();
}

void cmd_nanddump(char *line)
{
	char dev[128];
	int from;
	int length;
	char out[128];
	int ignbb;
	int ignoob = -1;

	sscanf(line, "%127s 0x%x 0x%x %127s %d %d",
		(char *)&dev, (unsigned int*)&from, (unsigned int *)&length,
		(char *)&out, (int*)&ignbb, (int *)&ignoob);

	if (ignoob == -1) {
		eprintf("Invalid arguments.\n");
		eprintf("nanddump [dev] [start] [len] [out] [ignore-badblocks] [ignore-oob]\n");
		eprintf("                  f.ex: nanddump /dev/mtd0 0x0 0x4000 xloader.bin 1 1\n");
	} else
		nanddump(dev, from, length, out, ignbb, ignoob);
}

void cmd_dump(char *line)
{
	if (!line[0]) {
		printf("Usage: dump [path]\n");
		return;
	}
	while(line[0]==' ') line = line +1;
	reverse_extract_pieces(line);
}

void cmd_badblocks(char *line)
{
	if (!line[0]) {
		printf("Usage: dump [path]\n");
		return;
	}
	while(line[0]==' ') line = line +1;
	check_badblocks(line);
}

void cmd_connect(char *line)
{
	connect_via_usb();
}

void cmd_reboot(char *line)
{
	reboot_board();
}

void cmd_shell(char *line)
{
	system("/bin/sh");
}

#define CMDS 11
#define IS_CMD(x) !strcmp(console_commands[i].name, x)
#define CALL_CMD(x) console_commands[x].callback((char *)line)
#define FOREACH_CMD(x) for(x=0;x<CMDS;x++)

struct cmd_t {
	char *name;
	void (*callback)(char *);
} console_commands[CMDS] = {
	{ .name = "exit",      .callback = &cmd_exit },
	{ .name = "q",         .callback = &cmd_exit },
	{ .name = "connect",   .callback = &cmd_connect },
	{ .name = "reboot",    .callback = &cmd_reboot },
	{ .name = "badblocks", .callback = &cmd_badblocks},
	{ .name = "help",      .callback = &cmd_help },
	{ .name = "?",         .callback = &cmd_help },
	{ .name = "info",      .callback = &cmd_info },
	{ .name = "dump",      .callback = &cmd_dump },
	{ .name = "nanddump",  .callback = &cmd_nanddump },
	{ .name = "shell",     .callback = &cmd_shell }
};

static int console_command(const char *line)
{
	int i;
	char command[11];

	command[0] = '\0';
	sscanf(line, "%10s", command);
	line = line + strlen(command);

	FOREACH_CMD(i)
		if (IS_CMD(command))
			CALL_CMD(i);
	//
	return 1;
}

int console_prompt()
{
	char line[1024];

	do {
		write(1, "0xFFFF> ", 8);
		line[0] = '\0';
		fgets(line, 1024, stdin);
		if (feof(stdin)) {
			write(1, "\n", 1);
			break;
		}
		if (line[0]) line[strlen(line)-1] = '\0';
		line[1023] = '\0';
	} while(console_command(line));

	return 0;
}
#endif
