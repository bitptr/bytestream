#include <sys/wait.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

enum {
	NAME_COLUMN,
	EXEC_COLUMN,
	NUM_COLUMNS,
};

void		 run_cmd(const char *);
GtkListStore	*collect_apps();
GtkWidget	*apps_tree_new();
void		 apps_list_insert_files(GtkListStore *, char *, DIR *, size_t);
void		 app_selected(GtkTreeView *, GtkTreePath *,
    GtkTreeViewColumn *, gpointer);

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
	g_signal_connect(apps_tree, "row-activated", G_CALLBACK(app_selected), NULL);

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
	    "text", NAME_COLUMN,
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

	apps = gtk_list_store_new(NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);

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
	char		*fn = NULL, *name_v = NULL, *exec_v = NULL;
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

		name_v = g_key_file_get_locale_string(key_file,
		    G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_NAME,
		    NULL, &error);
		if (name_v == NULL) {
			warnx("g_key_file_get_locale_string: %s",
			    error->message);
			goto cont;
		}

		exec_v = g_key_file_get_locale_string(key_file,
		    G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_EXEC,
		    NULL, &error);
		if (name_v == NULL) {
			warnx("g_key_file_get_locale_string: %s",
			    error->message);
			goto cont;
		}

		gtk_list_store_insert_with_values(apps, NULL, -1,
		    NAME_COLUMN, name_v,
		    EXEC_COLUMN, exec_v,
		    -1);

cont:
		free(exec_v);
		free(name_v);
		free(fn);
		if (key_file)
			g_key_file_free(key_file);
	}
}


/*
 * Pull out the executable from the selected entry, and run it.
 */
void
app_selected(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn
    *column, gpointer user_data)
{
	const char	*exec_v;
	GtkTreeModel	*model;
	GtkTreeIter	 iter;
	GValue		 value = G_VALUE_INIT;

	if ((model = gtk_tree_view_get_model(tree_view)) == NULL) {
		warnx("gtk_tree_view_get_model is NULL");
		goto done;
	}

	if (!gtk_tree_model_get_iter(model, &iter, path)) {
		warnx("gtk_tree_model_get_iter: path does not exist");
		goto done;
	}

	gtk_tree_model_get_value(model, &iter, EXEC_COLUMN, &value);
	if (!G_VALUE_HOLDS_STRING(&value)) {
		warnx("gtk_tree_model_get_value: exec is not a string");
		goto done;
	}

	exec_v = g_value_get_string(&value);

	run_cmd(exec_v);

	gtk_main_quit();

done:
	g_value_unset(&value);
}

/*
 * Run the command.
 */
void
run_cmd(const char *cmd)
{
	int	 status;
	pid_t	 pid;
	gchar	**argv;
	GError	*errors;

	if (!g_shell_parse_argv(cmd, NULL, &argv, &errors)) {
		warnx("g_shell_parse_argv: %s", errors->message);
		return;
	}

	switch (pid = fork()) {
	case -1:
		warn("fork");
		return;
	case 0:
		if (setsid() < 0) {
			warn("setsid");
			return;
		}

		switch (fork()) {
		case -1:
			warn("fork");
			return;
		case 0:
			execvp(argv[0], argv);
			errx(1, "command failed: %s", cmd);
			break;
		default:
			exit(0);
			break;
		}
	default:
		/* parent */
		waitpid(pid, &status, 0);
		break;
	}
}
