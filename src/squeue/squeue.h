struct squeue_t {
	int shmid;
	int mode;
	char *pool;
	/* pointers */
	int head_idx;
	int squeue_idx;
	/* counters */
	int squeue_locks;
	int squeue_lost;
	int squeue_oops;
	int squeue_pops;
};


#define Q_OPEN 0
#define Q_CREAT 1
#define Q_WAIT 2

int squeue_release(const char *file);
struct squeue_t *squeue_open(const char *file, int init);
int squeue_close(struct squeue_t *q);
void squeue_free(struct squeue_t *q);
int squeue_push(struct squeue_t *q, const char *str, int lock);
int squeue_pop(struct squeue_t *q);
char *squeue_get(struct squeue_t *q, int lock);
void squeue_stats(struct squeue_t *q);
