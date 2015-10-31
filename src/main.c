#include <sys/wait.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

enum {
	NAME_COLUMN,
	EXEC_COLUMN,
	FCODE_COLUMN,
	NUM_COLUMNS,
};

enum field_code {
	NO_PLACEHOLDER = 1 << 0,
	SINGLE_FILE_PLACEHOLDER = 1 << 1,
	MULTI_FILE_PLACEHOLDER = 1 << 2,
	SINGLE_URL_PLACEHOLDER = 1 << 3,
	MULTI_URL_PLACEHOLDER = 1 << 4,
};

int		 run_cmd(const char *, int);
void		 exec_cmd(const char *);
GtkListStore	*collect_apps();
void		 collect_apps_in_dir(GtkListStore *, const char *);
int		 field_codes(char *);
char		*fill_in_command(const char *, char *, int);
const char	*placeholder_from_flags(int);
GtkWidget	*apps_tree_new();
void		 apps_list_insert_files(GtkListStore *, char *, DIR *, size_t);
void		 app_selected(GtkTreeView *, GtkTreePath *,
    GtkTreeViewColumn *, gpointer);

static GtkWidget	*window;

/*
 * A program runner.
 */
int
main(int argc, char *argv[])
{
	GtkWidget	*box, *label, *apps_tree, *scrollable;
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
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(apps),
	    NAME_COLUMN, GTK_SORT_ASCENDING);

	return apps_tree;
}

/*
 * Return a GtkListStore* populated with data from all desktop entries.
 */
GtkListStore *
collect_apps()
{
	GtkListStore		*apps;
	const gchar *const	*dirs;

	apps = gtk_list_store_new(
	    NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);

	for (dirs = g_get_system_data_dirs(); *dirs; dirs++)
		collect_apps_in_dir(apps, *dirs);

	collect_apps_in_dir(apps, g_get_user_data_dir());

	return apps;
}

/*
 * Populate a GtkListStore* with data from desktop entries appearing in the
 * given data directory.
 */
void
collect_apps_in_dir(GtkListStore *apps, const char *data_dir)
{
	DIR	*dirp;
	char	*dir;
	size_t	 len;

	len = strlen(data_dir) + 14;
	if ((dir = calloc(len, sizeof(char))) == NULL)
		err(1, "calloc");
	if (snprintf(dir, len, "%s/%s", data_dir, "applications") >= (int)len)
		err(1, "snprintf");

	if ((dirp = opendir(dir)) == NULL)
		goto done;

	apps_list_insert_files(apps, dir, dirp, len);

	if (dirp && closedir(dirp) < 0)
		err(1, "closedir");

done:
	free(dir);
}

/*
 * Insert all desktop files in a directory into the GtkListStore.
 */
void
apps_list_insert_files(GtkListStore *apps, char *dir, DIR *dirp, size_t len)
{
	size_t		 len_fn;
	char		*fn = NULL, *name_v = NULL, *exec_v = NULL;
	int		 field_code_flags;
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
		if (exec_v == NULL) {
			warnx("g_key_file_get_locale_string: %s",
			    error->message);
			goto cont;
		}

		field_code_flags = field_codes(exec_v);

		gtk_list_store_insert_with_values(apps, NULL, -1,
		    NAME_COLUMN, name_v,
		    EXEC_COLUMN, exec_v,
		    FCODE_COLUMN, field_code_flags,
		    -1);

cont:
		free(exec_v); exec_v = NULL;
		free(name_v); name_v = NULL;
		free(fn); fn = NULL;
		if (key_file) {
			g_key_file_free(key_file);
			key_file = NULL;
		}
	}
}

/*
 * Identify which field code placeholders are used in the exec statement.
 */
int
field_codes(char *cmd)
{
	int	flags = 0, found_percent = 0;;

	for (; *cmd; cmd++) {
		switch (*cmd) {
		case '%':
			found_percent = !found_percent;
			break;
		case 'f':
			if (found_percent)
				flags |= SINGLE_FILE_PLACEHOLDER;
			found_percent = 0;
			break;
		case 'F':
			if (found_percent)
				flags |= MULTI_FILE_PLACEHOLDER;
			found_percent = 0;
			break;
		case 'u':
			if (found_percent)
				flags |= SINGLE_URL_PLACEHOLDER;
			found_percent = 0;
			break;
		case 'U':
			if (found_percent)
				flags |= MULTI_URL_PLACEHOLDER;
			found_percent = 0;
			break;
		}
	}

	return flags;
}

/*
 * Pull out the executable from the selected entry, and run it.
 */
void
app_selected(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn
    *column, gpointer user_data)
{
	int		 field_code_flags;
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

	exec_v = g_value_dup_string(&value);
	g_value_unset(&value);

	gtk_tree_model_get_value(model, &iter, FCODE_COLUMN, &value);
	if (!G_VALUE_HOLDS_INT(&value)) {
		warnx("gtk_tree_model_get_value: flags are not an integer");
		goto done;
	}
	field_code_flags = g_value_get_int(&value);

	if (run_cmd(exec_v, field_code_flags))
		gtk_main_quit();

done:
	g_value_unset(&value);
}

int
run_cmd(const char *cmd, int flags)
{
	int		 ret = 0;
	char		*text = NULL, *new_cmd = NULL;
	GtkWidget	*dialog, *box, *scrollable, *entry, *label = NULL;
	GtkTextBuffer	*buf;
	GtkTextIter	 start, end;

	if (!flags) {
		exec_cmd(cmd);
		return 1;
	}

	dialog = gtk_dialog_new_with_buttons(
	    "Command options",
	    GTK_WINDOW(window),
	     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
	     "_Close", GTK_RESPONSE_CLOSE,
	     "_Run", GTK_RESPONSE_OK,
	     NULL);
	box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	scrollable = gtk_scrolled_window_new(NULL, NULL);
	entry = gtk_text_view_new();

	if (flags & SINGLE_FILE_PLACEHOLDER) {
		label = gtk_label_new("File name");
	}

	if (flags & SINGLE_URL_PLACEHOLDER) {
		label = gtk_label_new("URI");
	}

	if (flags & MULTI_FILE_PLACEHOLDER) {
		label = gtk_label_new("Files");
	}

	if (flags & MULTI_URL_PLACEHOLDER) {
		label = gtk_label_new("URIs");
	}

	if (label) {
		gtk_container_add(GTK_CONTAINER(scrollable), entry);
		gtk_box_pack_start(GTK_BOX(box), label, /* expand */ 0,
		    /* fill */ 1, /* padding */ 3);
		gtk_box_pack_start(GTK_BOX(box), scrollable, /* expand */ 1,
		    /* fill */ 1, /* padding */ 3);
	}

	gtk_widget_show_all(box);

	switch (gtk_dialog_run(GTK_DIALOG(dialog))) {
	case GTK_RESPONSE_OK:
		buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(entry));
		gtk_text_buffer_get_start_iter(buf, &start);
		gtk_text_buffer_get_end_iter(buf, &end);
		text = gtk_text_buffer_get_text(buf, &start, &end, TRUE);

		if ((new_cmd = fill_in_command(cmd, text, flags)) == NULL) {
			warnx("fill_in_command failed");
			goto done;
		}

		exec_cmd(new_cmd);

		ret = 1;
		break;
	case GTK_RESPONSE_CLOSE:
	case GTK_RESPONSE_NONE:
	case GTK_RESPONSE_DELETE_EVENT:
		break;
	default:
		warnx("unknown result from gtk_dialog_run");
		break;
	}

	gtk_widget_destroy(dialog);

done:
	free(new_cmd);
	free(text);
	return ret;
}

/*
 * Run the command.
 */
void
exec_cmd(const char *cmd)
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

char *
fill_in_command(const char *cmd, char *interp, int flags)
{
	char		*new_cmd = NULL, *p;
	const char	*placeholder;
	int		 len_cmd, len_interp, len_ph, len_p;

	placeholder = placeholder_from_flags(flags);
	if (!*placeholder)
		return (char *)cmd;

	if ((p = strstr(cmd, placeholder)) == NULL)
		return (char *)cmd;

	len_p = strlen(p);
	len_ph = strlen(placeholder);
	len_cmd = strlen(cmd);
	len_interp = strlen(interp);

	if ((new_cmd = calloc(len_cmd - len_ph + len_interp, sizeof(char))) == NULL) {
		warn("calloc");
		goto err;
	}

	strlcpy(new_cmd, cmd, len_cmd + len_p + 1);
	snprintf(new_cmd + (p - cmd), len_interp + len_p-len_ph + 1, "%s%s",
	    interp, p + 2);

	return new_cmd;

err:
	free(p);
	free(new_cmd);
	return NULL;
}

const char *
placeholder_from_flags(int flags)
{
	if (flags & SINGLE_FILE_PLACEHOLDER)
		return "%f";
	if (flags & MULTI_FILE_PLACEHOLDER)
		return "%F";
	if (flags & SINGLE_URL_PLACEHOLDER)
		return "%u";
	if (flags & MULTI_URL_PLACEHOLDER)
		return "%U";

	return NULL;
}
