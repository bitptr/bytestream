#include <err.h>
#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>

void	run_entry(GtkDialog *, gint, gpointer);
void	gui_warn(char *);

int	run_cmd(const char *, char **);

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
	g_object_set_property(G_OBJECT(box), "margin", &g_9);

	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(window, "response", G_CALLBACK(run_entry), entry);

	gtk_widget_show_all(window);

	gtk_main();

	return 0;
}

/*
 * Run the program specified in the entry.
 */
void
run_entry(GtkDialog *dialog, gint response_id, gpointer user_data)
{
	const char	*cmd;
	char		*errstr;
	GtkEntry	*entry;

	if (response_id != GTK_RESPONSE_OK)
		return;

	entry = GTK_ENTRY(user_data);

	if (gtk_entry_get_text_length(entry) < 1)
		return;

	cmd = gtk_entry_get_text(entry);

	if (run_cmd(cmd, &errstr) < 0) {
		warn(errstr);
		gui_warn(errstr);
		free(errstr);
	} else
		gtk_widget_destroy(GTK_WIDGET(dialog));
}

void
gui_warn(char *errstr)
{
	fprintf(stderr, "errstr: %s\n", errstr);
}

/*
 * Run the command.
 *
 * If a system call fails, return -1. Set errstr to a message prefix.
 * On success return 0.
 */
int
run_cmd(const char *cmd, char **errstr)
{
	int	ret = 0;
	pid_t	 pid;
	gchar	**argv;
	GError	*errors;

	*errstr = NULL;

	if (!g_shell_parse_argv(cmd, NULL, &argv, &errors)) {
		g_error_free(errors);
		ret = -1;
		if ((errstr = calloc(20, sizeof(char))) == NULL)
			warn("calloc");
		else
			*errstr = "g_shell_parse_argv";
		/* TODO show a better err message, using errors */
		goto done;
	}

	switch (pid = fork()) {
	case -1:
		ret = -1;
		if ((errstr = calloc(5, sizeof(char))) == NULL)
			warn("calloc");
		else
			*errstr = "fork";
		break;
	case 0:
		if (setsid() < 0) {
			ret = -1;
			if ((errstr = calloc(7, sizeof(char))) == NULL)
				warn("calloc");
			else
				*errstr = "setsid";
			goto done;
		}
		execv(argv[0], argv);
		/* TODO find a way to gui_errx from here */
		errx(1, "command failed: %s", cmd);
		break;
	default:
		/* parent */
		break;
	}

done:
	return ret;
}
