#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

void	run_entry(GtkDialog *, gint, gpointer);
void	gui_warn(char *);

int	run_cmd(const char *, char **);

gboolean	print_cell(GtkTreeModel *, GtkTreePath *, GtkTreeIter *, gpointer);

/*
 * A program runner.
 */
int
main(int argc, char *argv[])
{
	const gchar *const *dirs;
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

	/* add apps from desktop entries */
	GtkListStore	*apps;
	DIR		*dirp;
	struct dirent	*dp;
	char		*dir, *fn = NULL, *name = NULL;
	size_t		 len;
	GKeyFile	*key_file;
	GError		*error;
	GtkWidget	*apps_tree;
	GtkTreeViewColumn	*name_col;

	apps = gtk_list_store_new(1, G_TYPE_STRING);
	for (dirs = g_get_system_data_dirs(); *dirs; dirs++) {
		len = strlen(*dirs) + 14;
		if ((dir = calloc(len, sizeof(char))) == NULL)
			err(1, "calloc");
		if (snprintf(dir, len, "%s/%s", *dirs, "applications") >= (int)len)
			err(1, "snprintf");
		if ((dirp = opendir(dir)) == NULL)
			goto cont;

		while ((dp = readdir(dirp)) != NULL)
			if (dp->d_namlen > 8 &&
			    strncmp(dp->d_name + dp->d_namlen - 8,
				    ".desktop", 8) == 0) {
				if ((fn = calloc(dp->d_namlen + len + 1,
						    sizeof(char))) == NULL)
					err(1, "calloc");
				if (snprintf(fn, dp->d_namlen + len + 1,
					    "%s/%s", dir, dp->d_name) >=
				    dp->d_namlen + (int)len + 1)
					err(1, "snprintf");

				key_file = g_key_file_new();
				if (!g_key_file_load_from_file(key_file,
					    fn, G_KEY_FILE_NONE, &error))
					errx(1, "%s", error->message);

				name = g_key_file_get_locale_string(key_file,
				    "Desktop Entry", "Name", NULL, &error);
				if (name == NULL) {
					warnx("g_key_file_get_locale_string: %s",
					    error->message);
					goto cont;
				}

				gtk_list_store_insert_with_values(
				    apps, NULL, -1,
				    0, name,
				    -1);

				free(name); name = NULL;
				free(fn); fn = NULL;
				g_key_file_free(key_file);
			}

cont:
		free(name);
		free(fn);

		if (dirp && closedir(dirp) < 0)
			err(1, "closedir");

		free(dir);
	}

	gtk_tree_model_foreach(GTK_TREE_MODEL(apps), print_cell, NULL);
	apps_tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(apps));
	name_col = gtk_tree_view_column_new_with_attributes(
	    "Name",
	    gtk_cell_renderer_text_new(),
	    "text", 0,
	    NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(apps_tree), name_col);
	gtk_box_pack_start(GTK_BOX(box), apps_tree, /* expand */ 1, /* fill */ 1,
	    /* padding */ 3);
	/* that was the app code */

	gtk_widget_show_all(window);

	gtk_main();

	return 0;
}

gboolean
print_cell(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	GValue	value = G_VALUE_INIT;

	gtk_tree_model_get_value(model, iter, 0, &value);

	fprintf(stderr, "print_cell: %s\n", g_value_get_string(&value));

	g_value_unset(&value);
	return FALSE;
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
