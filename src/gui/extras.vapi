namespace Extras {
	public class External {
		[CCode (cname = "system")]
		public static int system(string cmd);
		[CCode (cname = "fpid_file")]
		public static string fpid_file(string file);
		[CCode (cname = "fpid_size")]
		public static long fpid_size(string file);
	}
}
