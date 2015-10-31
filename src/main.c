#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

void	run_entry(GtkDialog *, gint, gpointer);
void	gui_warn(char *);

int	run_cmd(const char *, char **);

GtkListStore	*collect_apps();
GtkWidget	*apps_tree_new();
void		 apps_list_insert_files(GtkListStore *, char *, DIR *, size_t);

/*
 * A program runner.
 */
int
main(int argc, char *argv[])
{
	GtkWidget	*window, *box, *label, *apps_tree, *scrollable;
	GValue		 g_9 = G_VALUE_INIT;

	gtk_init(&argc, &argv);

	g_value_init(&g_9, G_TYPE_INT);
	g_value_set_int(&g_9, 9);

	window = gtk_dialog_new_with_buttons(
	    "bytestream",
	    NULL,
	    0,
	    "_Close", GTK_RESPONSE_CLOSE,
	    NULL);
	box = gtk_dialog_get_content_area(GTK_DIALOG(window));
	label = gtk_label_new("Select program.");
	scrollable = gtk_scrolled_window_new(NULL, NULL);
	apps_tree = apps_tree_new();

	gtk_widget_set_size_request(window, 400, 300);
	g_object_set_property(G_OBJECT(box), "margin", &g_9);

	gtk_container_add(GTK_CONTAINER(scrollable), apps_tree);
	gtk_box_pack_start(GTK_BOX(box), label, /* expand */ 0, /* fill */ 1,
	    /* padding */ 3);
	gtk_box_pack_start(GTK_BOX(box), scrollable, /* expand */ 1,
	    /* fill */ 1, /* padding */ 3);

	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

	gtk_widget_show_all(window);

	gtk_main();

	return 0;
}

/*
 * Return a GtkTreeView* populated with data from all desktop entries.
 */
GtkWidget *
apps_tree_new()
{
	GtkWidget		*apps_tree;
	GtkListStore		*apps;
	GtkTreeViewColumn	*name_col;

	apps = collect_apps();
	apps_tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(apps));

	name_col = gtk_tree_view_column_new_with_attributes(
	    "Name", gtk_cell_renderer_text_new(),
	    "text", 0,
	    NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(apps_tree), name_col);

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(apps_tree), FALSE);
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(apps_tree), TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(apps_tree), 0);

	return apps_tree;
}

/*
 * Return a GtkListStore* populated with data from all desktop entries.
 */
GtkListStore *
collect_apps()
{
	GtkListStore	*apps;
	DIR		*dirp;
	char		*dir;
	size_t		 len;
	const gchar *const *dirs;

	apps = gtk_list_store_new(1, G_TYPE_STRING);

	for (dirs = g_get_system_data_dirs(); *dirs; dirs++) {
		len = strlen(*dirs) + 14;
		if ((dir = calloc(len, sizeof(char))) == NULL)
			err(1, "calloc");
		if (snprintf(dir, len, "%s/%s", *dirs, "applications") >= (int)len)
			err(1, "snprintf");

		if ((dirp = opendir(dir)) == NULL)
			goto cont;

		apps_list_insert_files(apps, dir, dirp, len);

		if (dirp && closedir(dirp) < 0)
			err(1, "closedir");

cont:
		free(dir);
	}

	return apps;
}

/*
 * Insert all desktop files in a directory into the GtkListStore.
 */
void
apps_list_insert_files(GtkListStore *apps, char *dir, DIR *dirp, size_t len)
{
	size_t		 len_fn;
	char		*fn = NULL, *name = NULL;
	struct dirent	*dp;
	GKeyFile	*key_file = NULL;
	GError		*error;

	while ((dp = readdir(dirp)) != NULL) {
		if (dp->d_namlen <= 8)
			goto cont;
		if (strncmp(dp->d_name + dp->d_namlen - 8, ".desktop", 8) != 0)
			goto cont;

		len_fn = dp->d_namlen + len + 1;
		if ((fn = calloc(len_fn, sizeof(char))) == NULL)
			err(1, "calloc");
		if (snprintf(fn, len_fn, "%s/%s", dir, dp->d_name) >= (int)len_fn)
			err(1, "snprintf");

		key_file = g_key_file_new();
		if (!g_key_file_load_from_file(key_file, fn, G_KEY_FILE_NONE, &error))
			errx(1, "%s", error->message);

		name = g_key_file_get_locale_string(key_file, "Desktop Entry",
		    "Name", NULL, &error);
		if (name == NULL) {
			warnx("g_key_file_get_locale_string: %s",
			    error->message);
			goto cont;
		}

		gtk_list_store_insert_with_values(apps, NULL, -1, 0, name, -1);

cont:
		free(name);
		free(fn);
		if (key_file)
			g_key_file_free(key_file);
	}
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
