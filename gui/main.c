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
