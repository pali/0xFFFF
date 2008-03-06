using GLib;

[CCode (lower_case_cprefix = "squeue_", cheader_filename = "squeue.h")]
namespace SQueues {
	[CCode (cname = "struct squeue_t", free_function = "squeue_free")]
	public struct SQueue {
		int shmid;
		int mode;
		char *pool;
		/* pointers */
		int head_idx;
		int queue_idx;
		/* counters */
		int locks;
		int lost;
		int oops;
		int pops;
		[CCode (cname = "squeue_open")]
		public static SQueue* open(string file, int mode);
		[CCode (cname = "squeue_push")]
		public int push(string msg, int l);
		[CCode (cname = "squeue_push2")]
		public int push2(string cmd, string msg, int l);
		[CCode (cname = "squeue_get")]
		public weak string get(int l);
		[CCode (cname = "squeue_pop")]
		public int pop();
		[CCode (cname = "squeue_stats")]
		public int stats();
		[CCode (cname = "squeue_close")]
		public int close();
		[CCode (cname = "squeue_free")]
		public int free();
		[CCode (cname = "squeue_release")]
		public static int release(string file);
		
	}

	[CCode (cprefix = "Q_", cheader_fileneme = "squeue.h")]
        public enum SQueueMode {
		OPEN   = 0,
		CREAT  = 1,
		WAIT   = 2
	}
}
