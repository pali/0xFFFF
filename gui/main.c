/*
 *  0xFFFF - Open Free Fiasco Firmware Flasher
 *  Copyright (C) 2007  pancake <pancake@youterm.com>
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

#include <glade/glade.h>
#include <gtk/gtk.h>

#define U(x) printf("%d\n",x); fflush(stdout);

	GladeXML *xml;
int main(int argc, char **argv)
{
	GtkWidget *w;

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
{
	GtkComboBox *piece = GTK_COMBO_BOX(glade_xml_get_widget(xml, "piece"));
	gtk_combo_box_set_active(piece, 0);
}

	gtk_widget_show_all(w);

	gtk_object_unref(GTK_OBJECT(xml));

	gtk_main();
	return 0;	
}
extern void gui_run();
extern void gui_run()
{
	printf("run\n");
}

extern void gui_add();
extern void gui_add()
{
	GtkTreeView *tree = glade_xml_get_widget(xml, "tree");
	GtkFileChooser *file = GTK_FILE_CHOOSER(glade_xml_get_widget(xml, "file"));
	GtkComboBox *piece = GTK_COMBO_BOX(glade_xml_get_widget(xml, "piece"));

	printf("ADD %s\n", gtk_file_chooser_get_filename(file));
	printf("ADD %d\n", gtk_combo_box_get_active(piece));
}

extern void gui_information();
void gui_information()
{
	printf("FUCKMENOT!\n");
	fflush(stdout);
}
