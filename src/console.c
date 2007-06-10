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
	printf("connect     connects via usb to nolo\n");
	printf("info        shows info of the remote system\n");
	printf("linfo       shows info of the local system\n");
	printf("shell       opens a shell (/bin/sh)\n");
	printf("dump [dir]  dumps the contents of /dev/mtd to dir\n");
	printf("exit        exits the shell\n");
	fflush(stdout);
}

void cmd_info(char *line)
{
	get_hw_revision(); // get hardware revision:
	get_root_device(); // only for flashing
	get_usb_mode();
	get_rd_mode();
	get_rd_flags();
}

void cmd_dump(char *line)
{
	if (!line[0]) {
		printf("Usage: dump [path]\n");
		return;
	}
	reverse_extract_pieces(line);
}

void cmd_connect(char *line)
{
	connect_via_usb();
}

void cmd_shell(char *line)
{
	system("/bin/sh");
}

#define CMDS 8
#define IS_CMD(x) !strcmp(console_commands[i].name, x)
#define CALL_CMD(x) console_commands[x].callback((char *)line)
#define FOREACH_CMD(x) for(x=0;x<CMDS;x++)

struct cmd_t {
	char *name;
	void (*callback)(char *);
} console_commands[CMDS] = {
	{ .name = "exit",    .callback = &cmd_exit },
	{ .name = "q",       .callback = &cmd_exit },
	{ .name = "connect", .callback = &cmd_connect },
	{ .name = "help",    .callback = &cmd_help },
	{ .name = "?",       .callback = &cmd_help },
	{ .name = "info",    .callback = &cmd_info },
	{ .name = "dump",    .callback = &cmd_dump },
	{ .name = "shell",   .callback = &cmd_shell }
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
		if (line[0]) line[strlen(line)-1] = '\0';
		line[1023] = '\0';
	} while(console_command(line));

	return 0;
}
