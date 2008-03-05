#include "squeue.h"
#include <unistd.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <signal.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#define POOL_SIZE ITEM_MAX*ITEM_SIZE
#define ITEM_SIZE 80
#define ITEM_MAX 10

int squeue_release(const char *file)
{
	int shmid;
	key_t k = ftok(file, 0xa3);
	
	if (k == -1) {
		perror("ftok");
		return -1;
	}
	
	shmid = shmget(k, POOL_SIZE, 0666);
	if (shmid == -1)
		return -1; // not released
	return shmctl(shmid, IPC_RMID,NULL);
}

struct squeue_t *squeue_open(const char *file, int mode)
{
	struct squeue_t *q;
	char *pool;
	int shmid;
	key_t k = ftok(file, 0xa3);
_retry:
	shmid = shmget(k, POOL_SIZE, 0666);
	if (shmid == -1) {
		if (mode == Q_WAIT) {
			usleep(200);
			goto _retry;
		}
		shmid = shmget(k, POOL_SIZE, 0666|IPC_EXCL|((mode==Q_CREAT)?IPC_CREAT:0));
		if (shmid == -1) {
			if (mode == Q_WAIT) {
				usleep(200);
				goto _retry;
			}
			perror("shmget");
			return NULL;
		}
	}

	if (mode == Q_WAIT)
		mode = Q_OPEN;

	pool = shmat(shmid, NULL, k);
	if (((int)pool) == -1) {
		perror("shmat\n");
		return NULL;
	}

	if (mode == Q_CREAT)
		memset(pool, '\0', POOL_SIZE);

	q = (struct squeue_t *)malloc(sizeof(struct squeue_t));
	memset(q, '\0', sizeof(struct squeue_t));
	q->shmid = shmid;
	q->pool  = pool;
	q->mode  = mode;

	return q;
}

void squeue_free(struct squeue_t *q)
{
	free(q);
}

int squeue_close(struct squeue_t *q)
{
	char *pool;

	if (q==NULL)
		return -1;

	pool = q->pool;
	shmctl(q->shmid, IPC_RMID, NULL);
	free(q);
	return shmdt(pool);
}

int squeue_push(struct squeue_t *q, const char *str, int lock)
{
	int i;

	if (q==NULL)
		return -1;
//	if (q->mode == Q_CREAT) {
//		printf("squeue_push: cannot push from the creator\n");
//		return -1;
//	}

	if (str==NULL||strlen(str)>ITEM_SIZE)
		return -1;
	do {
		if (q->squeue_idx >= ITEM_MAX) {
			while (q->pool[0]!='\0') {
				q->squeue_locks++;
				if (!lock) {
					printf("buffer is full\n");
					return -1;
				}
				usleep(200);
			}
			q->squeue_idx = 0;
		}
		
		for(i=q->squeue_idx*ITEM_SIZE;i<POOL_SIZE;i+=ITEM_SIZE) {
			if (q->pool[i]=='\0') {
				strcpy(q->pool+i, str);
				q->squeue_idx++;
				return 1;
			}
		}
		q->squeue_locks++;
	} while (lock);
	q->squeue_lost++;

	return 0;
}

int squeue_pop(struct squeue_t *q)
{
	int i;
	int two = 1;

	if (q==NULL)
		return -1;
//	if (q->mode == Q_OPEN) { // cannot pop when not owner of squeue
//		fprintf(stderr, "squeue_pop: cannot pop when you are not owner.\n");
//		return -1;
//	}
	for(two=1;two--;q->head_idx = 0)
		for(i=q->head_idx*ITEM_SIZE;i<POOL_SIZE;i+=ITEM_SIZE) {
			if (q->pool[i]!='\0') {
				q->pool[i]='\0';
				q->head_idx++;
				q->squeue_pops++;
				return 1;
			}
		}
	q->squeue_oops++;
	fprintf(stderr, "WARNING: race condition detected in squeue.\n");
	return 0;
}

char *squeue_get(struct squeue_t *q, int lock)
{
	int i=0;
	int two = 1;

	if(q==NULL)
		return NULL;
//	if (q->mode == Q_OPEN) { // cannot pop when not owner of squeue
//		fprintf(stderr, "squeue_get: cannot pop when you are not owner.\n");
//		return NULL;
//	}
	do {
		for(two=1;two--;q->head_idx = 0) {
			for(i=q->head_idx*ITEM_SIZE;i<POOL_SIZE;i+=ITEM_SIZE)
				if (q->pool[i]!='\0')
					return q->pool+i;
		}
		if (lock)
			usleep(500);
	} while (lock);
	return NULL;
}

void squeue_stats(struct squeue_t *q)
{
	if(q==NULL)
		return;
	printf("Queue locks: %d\n", q->squeue_locks);
	printf("Queue lost: %d\n", q->squeue_lost);
	printf("Queue oops: %d\n", q->squeue_oops);
	printf("Queue pops: %d\n", q->squeue_pops);
}

#if _MAIN_
struct squeue_t *q;

int sigc()
{
	squeue_stats(q);
	exit(1);
}

int main()
{
	char buf[102];
	int i;
	int pid;

	signal(SIGINT, sigc);

	squeue_release("/dev/null");

	pid = fork();
	if (pid) {
		q = squeue_open("/dev/null", Q_CREAT);
		if (q == NULL) {
			perror("oops");
			kill(pid, SIGINT);
			return;
		}
		squeue_push(q, "Hello World", 0);
		squeue_push(q, "jeje msg 2", 0);

		while(1) {
			char *uh = squeue_get(q,1);
			if (uh) {
				printf("get: (%s)\n", uh);
				squeue_pop(q);
			}
			usleep(200);
		}
		squeue_close(q);
	} else {
		q = squeue_open("/dev/null", Q_WAIT);
		if (q == NULL) {
			perror("oops");
			return;
		}
		for(i=0;i<128;i++) {
			sprintf(buf, "puta%d", i);
			squeue_push(q, buf, 1);
			usleep(100);
		}
		squeue_stats(q);
		printf("cya\n");
		squeue_close(q);
	}

	return 0;
}
#endif
