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
#if HAVE_SQUEUE
#include "squeue/squeue.h"

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
	squeue_push(p, "nice shot!", 0);
	printf("childa (%s)\n", msg);
}

int queue_mode()
{
	int pid = 0;
	char *msg;

	pid = dofork();
	if (pid) {
		wait(pid);
		return 0;
	} else {
		p = squeue_open("/tmp/0xFFFF.1", Q_CREAT);
		q = squeue_open("/tmp/0xFFFF.2", Q_CREAT);
		if (p == NULL || q == NULL) {
			fprintf(stderr, "Cannot open queue files\n");
			return 0;
		}
		pid = dofork();
		if (pid) {
			printf( "Entering into shared queue server mode.\n"
			"Type $ kill -9 %d # to stop\n"
			"NOTE: Manually remove the /tmp/.0xFFFF.* files to solve perm problems\n", pid);
		} else {
			printf("Waiting for a client in shared queues..\n");
			setsid();
			while(1) {
				msg = squeue_get(q, 1);
				if (msg) {
					process_message(msg);
					squeue_pop(q);
				}
			}
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
