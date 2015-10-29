#include <gtk/gtk.h>

/*
 * A program runner.
 */
int
main(int argc, char *argv[])
{
	GtkWidget	*window, *box, *label, *entry;
	GValue		 g_9 = G_VALUE_INIT;

	gtk_init(&argc, &argv);

	g_value_init(&g_9, G_TYPE_INT);
	g_value_set_int(&g_9, 9);

	window = gtk_dialog_new_with_buttons(
	    "bytestream",
	    NULL,
	    0,
	    "_Close", GTK_RESPONSE_CLOSE,
	    "_Run", GTK_RESPONSE_OK,
	    NULL);
	box = gtk_dialog_get_content_area(GTK_DIALOG(window));
	label = gtk_label_new("Desktop program or full path and options.");
	entry = gtk_entry_new();

	gtk_box_pack_start(GTK_BOX(box), label, /* expand */ 1, /* fill */ 1,
	    /* padding */ 3);
	gtk_box_pack_start(GTK_BOX(box), entry, /* expand */ 1, /* fill */ 1,
	    /* padding */ 3);

	gtk_dialog_set_default_response(GTK_DIALOG(window), GTK_RESPONSE_OK);
	gtk_entry_set_activates_default(GTK_ENTRY(entry), 1);
	g_object_set_property(G_OBJECT(box), "margin-left", &g_9);
	g_object_set_property(G_OBJECT(box), "margin", &g_9);

	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

	gtk_widget_show_all(window);

	gtk_main();

	return 0;
}
