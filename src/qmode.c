/*
 *  0xFFFF - Open Free Fiasco Firmware Flasher
 *  Copyright (C) 2008  pancake <pancake@youterm.com>
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
#include "os.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

#if HAVE_SQUEUE

struct squeue_t *q;
struct squeue_t *p;

int fork_enabled = 0;
int dofork()
{
	if (fork_enabled)
		return fork();
	return 0;
}

void process_message(char *msg)
{
	char *str;
	char *arg;
	int c=1;
	if (msg == NULL)
		return;
	printf("[x] (%s)\n", msg);
	str = strdup(msg);
	arg = strchr(str, ':');
	if (c!=0) {
		arg[0]='\0';
		arg = arg +1;
		if (!strcmp(str, "flash")) {
			const char *type = fpid_file(arg);
			if (type == NULL) {
				squeue_push2(p, "error", "Unknown piece format", 1);
			} else flash_image(arg, type, NULL, NULL, NULL);
		} else
		if (!strcmp(str, "reset")) {
			if (reboot_board() == 0) {
				squeue_push2(p,"info", "Device reboots", 1);
			} else squeue_push2(p,"error", "Cannot reboot device", 1);
		} else
		if (!strcmp(str, "info")) {
			get_rd_flags();
			squeue_push2(p, "info", strbuf, 1);
			get_nolo_version();
			squeue_push2(p, "info", strbuf, 1);
			get_usb_mode();
			squeue_push2(p, "info", strbuf, 1);
		} else
			squeue_push2(p, "error", "invalid command", 0);
	} else {
		squeue_push2(p, "error", "invalid command format", 0);
	}
	free(str);
}

static void cc()
{
	squeue_close(p);
	squeue_close(q);
	printf("pipes closed\n");
	squeue_release("/tmp/0xFFFF.1");
	squeue_release("/tmp/0xFFFF.2");
	exit(1);
}

int queue_mode()
{
	int pid = 0;
	char *msg;

	signal(SIGINT, cc);

	pid = dofork();
	if (pid) {
		wait(&pid);
		return 0;
	} else {
		p = squeue_open("/tmp/0xFFFF.1", Q_CREAT);
		q = squeue_open("/tmp/0xFFFF.2", Q_CREAT);
		if (p == NULL || q == NULL) {
			fprintf(stderr, "Cannot open queue files\n");
			return 0;
		}
		chmod("/tmp/0xFFFF.1", 0666);
		chmod("/tmp/0xFFFF.2", 0666);
		pid = dofork();
		if (pid) {
			printf( "Entering into shared queue server mode.\n"
			"Type $ kill -9 %d # to stop\n"
			"NOTE: Manually remove the /tmp/.0xFFFF.* files to solve perm problems\n", pid);
		} else {
			do {
#if HAVE_USB
				if (connect_via_usb()) {
					fprintf(stderr, "Cannot connect to device. It is possibly not in boot stage.\n");
					squeue_push2(p, "error", "Cannot connect to the device", 1);
					return 0;
				}
#endif
				printf("Waiting for a client in shared queues..\n");
				setsid();
				while(1) {
					msg = squeue_get(q, 1);
					if (msg) {
						process_message(msg);
						squeue_pop(q);
					}
				}
			printf("Connection restarted\n");
			} while (1);
		}
	}
	return 0;
}
#else
int queue_mode()
{
	/* dummy */
	fprintf(stderr, "No HAVE_SQUEUE support\n");
}
#endif
