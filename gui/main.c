#include <glade/glade.h>
#include <gtk/gtk.h>

#define U(x) printf("%d\n",x); fflush(stdout);

int main(int argc, char **argv)
{
	GtkWidget *w;
	GladeXML *xml;

	gtk_init(&argc, &argv);

	xml = glade_xml_new("g0xFFFF.glade", "main_window", "");
	//xml = glade_xml_new("test.glade", "window1", NULL);
	if(!xml) {
		g_warning("We could not load the interface!");
		return 1;
	}

	glade_xml_signal_autoconnect(xml);

	w = glade_xml_get_widget(xml, "main_window");
	gtk_signal_connect(GTK_OBJECT(w), "destroy",
		GTK_SIGNAL_FUNC(gtk_main_quit),NULL);
	gtk_widget_show_all(w);

	gtk_object_unref(GTK_OBJECT(xml));

	gtk_main();
	return 0;	
}
