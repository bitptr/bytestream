/*
 * Copyright (c) 2015 Mike Burns <mike@mike-burns.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define _BSD_SOURCE 1

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/wait.h>
#include <err.h>
#include <getopt.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <gtk/gtk.h>

#include "entrycellrenderer.h"
#include "compat.h"

enum {
	NAME_COLUMN,
	EXEC_COLUMN,
	FCODE_COLUMN,
	ICON_COLUMN,
	TERM_COLUMN,
	NUM_COLUMNS,
};

enum field_code {
	NO_PLACEHOLDER = 1 << 0,
	SINGLE_FILE_PLACEHOLDER = 1 << 1,
	MULTI_FILE_PLACEHOLDER = 1 << 2,
	SINGLE_URL_PLACEHOLDER = 1 << 3,
	MULTI_URL_PLACEHOLDER = 1 << 4,
};

struct state {
	char		*cmd;		/* The command to run */
	char		*name;		/* The program name to run, if any */
	uint8_t		 shift_pressed;	/* Whether shift is being held */
	uint8_t		 flags;		/* Command flags as set by the desktop entry */
	gboolean	 use_term;	/* Whether to run the command in a terminal */
};

__dead void		 usage();
static struct state	*init_state(void);
static void		 free_state(struct state *);
static void		 run_app(struct state *);
static uint8_t		 run_cmd(struct state *);
static void		 exec_cmd(const char *);
static GtkListStore	*collect_apps();
static uint8_t	 	 field_codes(char *);
static uint8_t		 fill_in_flags(char **, uint8_t);
static uint8_t		 add_terminal(char **);
static char		*fill_in_command(const char *, const char *, uint8_t);
static const char	*placeholder_from_flags(uint8_t);
static void		 handle_response(GtkDialog *, gint, gpointer);
static GtkWidget	*apps_tree_new();
static void		 apps_list_insert_files(GtkListStore *, GHashTable *,
    char *, DIR *, size_t);
static void		 app_selected(GtkTreeView *, GtkTreePath *,
    GtkTreeViewColumn *, gpointer);
static gboolean		 key_pressed(GtkWidget *, GdkEvent *, gpointer);
static gboolean		 run_desktop_entry(GtkTreeModel *, GtkTreePath *,
    GtkTreeIter *, gpointer);
static void		 collect_apps_in_dir(GtkListStore *, GHashTable *,
    const char *);

static GtkWidget	*window = NULL;

/*
 * A program runner.
 */
int
main(int argc, char *argv[])
{
	int		 ch;
	GtkWidget	*box, *label, *apps_tree, *scrollable;
	GValue		 g_9 = G_VALUE_INIT;
	GtkBindingSet	*binding_set;
	struct state	*st;

	st = init_state();

	while ((ch = getopt(argc, argv, "")) != -1)
		usage();
	argc -= optind;
	argv += optind;

	gtk_init(&argc, &argv);

	if (argc > 1)
		usage();

	if (argc == 1) {
		st->name = strdup(argv[0]);
		run_app(st);
		return 0;
	}

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
	label = gtk_label_new("Select program.");
	scrollable = gtk_scrolled_window_new(NULL, NULL);
	if ((apps_tree = apps_tree_new()) == NULL)
		return 1;

	gtk_widget_set_size_request(window, 400, 300);
	g_object_set_property(G_OBJECT(box), "margin", &g_9);

	gtk_container_add(GTK_CONTAINER(scrollable), apps_tree);
	gtk_box_pack_start(GTK_BOX(box), label, /* expand */ 0, /* fill */ 1,
	    /* padding */ 3);
	gtk_box_pack_start(GTK_BOX(box), scrollable, /* expand */ 1,
	    /* fill */ 1, /* padding */ 3);

	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(window, "response", G_CALLBACK(handle_response), apps_tree);
	g_signal_connect(window, "key-press-event", G_CALLBACK(key_pressed), st);
	g_signal_connect(apps_tree, "row-activated", G_CALLBACK(app_selected), st);

	binding_set = gtk_binding_set_by_class(G_OBJECT_GET_CLASS(apps_tree));
	gtk_binding_entry_add_signal(
	    binding_set, GDK_KEY_Return, GDK_SHIFT_MASK, "select-cursor-row",
	    1, G_TYPE_BOOLEAN, TRUE);
	gtk_binding_entry_add_signal(
	    binding_set, GDK_KEY_ISO_Enter, GDK_SHIFT_MASK, "select-cursor-row",
	    1, G_TYPE_BOOLEAN, TRUE);
	gtk_binding_entry_add_signal(
	    binding_set, GDK_KEY_KP_Enter, GDK_SHIFT_MASK, "select-cursor-row",
	    1, G_TYPE_BOOLEAN, TRUE);

	gtk_widget_show_all(window);

	gtk_main();

	free_state(st);
	return 0;
}

/*
 * Show usage information, and then quit.
 */
__dead void
usage()
{
	printf("usage: bytestream [entry name]\n");
	exit(0);
}

/*
 * Initialize an empty state structure. This structure is used to communicate
 * in callbacks.
 */
struct state *
init_state(void)
{
	struct state	*st;

	if ((st = malloc(sizeof(struct state))) == NULL)
		err(1, NULL);

	st->cmd = NULL;
	st->name = NULL;
	st->shift_pressed = 0;
	st->flags = 0;
	st->use_term = 0;

	return st;
}

/*
 * Free a state structure.
 */
static void
free_state(struct state *st)
{
	if (st) {
		free(st->cmd);
		free(st->name);
		free(st);
	}
}

/*
 * Run a specific application by name.
 */
static void
run_app(struct state *st)
{
	GtkListStore	*apps;

	apps = collect_apps();
	if (apps != NULL)
		gtk_tree_model_foreach(
		    GTK_TREE_MODEL(apps), run_desktop_entry, st);
}

/*
 * If the given row in the model is the name we're looking for, run it.
 * Otherwise keep looking.
 */
gboolean
run_desktop_entry(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter,
    gpointer data)
{
	char		*name_v;
	struct state	*st;
	GValue	 	 value = G_VALUE_INIT;

	st = (struct state *)data;

	gtk_tree_model_get_value(model, iter, NAME_COLUMN, &value);
	if (!G_VALUE_HOLDS_STRING(&value)) {
		warnx("gtk_tree_model_get_value: name is not a string");
		return FALSE;
	}
	name_v = g_value_dup_string(&value);
	g_value_unset(&value);

	if (strlen(name_v) != strlen(st->name) ||
	    strncmp(name_v, st->name, strlen(st->name)) != 0)
		return FALSE;

	gtk_tree_model_get_value(model, iter, EXEC_COLUMN, &value);
	if (!G_VALUE_HOLDS_STRING(&value)) {
		warnx("gtk_tree_model_get_value: exec is not a string");
		return TRUE;
	}
	st->cmd = g_value_dup_string(&value);
	g_value_unset(&value);

	gtk_tree_model_get_value(model, iter, FCODE_COLUMN, &value);
	if (!G_VALUE_HOLDS_UINT(&value)) {
		warnx("gtk_tree_model_get_value: flags are not an integer");
		return TRUE;
	}
	st->flags = g_value_get_uint(&value);
	g_value_unset(&value);

	gtk_tree_model_get_value(model, iter, TERM_COLUMN, &value);
	if (!G_VALUE_HOLDS_BOOLEAN(&value)) {
		warnx("gtk_tree_model_get_value: term is not a Boolean");
		return TRUE;
	}
	st->use_term = g_value_get_boolean(&value);
	g_value_unset(&value);

	if (run_cmd(st))
		return TRUE;

	return TRUE;
}

/*
 * Handle the buttons on the dialog.
 */
void
handle_response(GtkDialog *dialog, gint response_id, gpointer user_data)
{
	GtkTreeView		*tree_view;
	GtkTreePath		*path = NULL;
	GtkTreeViewColumn	*column = NULL;

	if (response_id != GTK_RESPONSE_OK) {
		gtk_main_quit();
		return;
	}

	tree_view = (GtkTreeView *)user_data;

	gtk_tree_view_get_cursor(tree_view, &path, &column);
	if (path == NULL || column == NULL)
		return;

	app_selected(tree_view, path, column, NULL);

	gtk_tree_path_free(path);
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
	GValue		 	 g_3 = G_VALUE_INIT;
	GtkCellRenderer		*cellr;

	g_value_init(&g_3, G_TYPE_INT);
	g_value_set_int(&g_3, 3);

	if ((apps = collect_apps()) == NULL)
	    return NULL;
	apps_tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(apps));

	cellr = bs_cell_renderer_entry_new();
	name_col = gtk_tree_view_column_new_with_attributes(
	    "Entry", cellr,
	    "name", NAME_COLUMN,
	    "exec", EXEC_COLUMN,
	    "icon", ICON_COLUMN,
	    NULL);
	g_object_set_property(G_OBJECT(cellr), "xpad", &g_3);
	g_object_set_property(G_OBJECT(cellr), "ypad", &g_3);
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
	GHashTable		*entry_files;

	apps = gtk_list_store_new(NUM_COLUMNS,
	    G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_BOOLEAN);
	if (apps == NULL)
		return NULL;

	entry_files = g_hash_table_new_full(g_str_hash, g_str_equal, free, NULL);

	collect_apps_in_dir(apps, entry_files, g_get_user_data_dir());

	for (dirs = g_get_system_data_dirs(); *dirs; dirs++)
		collect_apps_in_dir(apps, entry_files, *dirs);

	g_hash_table_unref(entry_files);

	return apps;
}

/*
 * Populate a GtkListStore* with data from desktop entries appearing in the
 * given data directory.
 */
void
collect_apps_in_dir(GtkListStore *apps, GHashTable *entry_files,
    const char *data_dir)
{
	DIR	*dirp;
	char	*dir;
	int	 ret;
	size_t	 len;

	len = strlen(data_dir) + 14;
	if ((dir = calloc(len, sizeof(char))) == NULL)
		err(1, NULL);
	ret = snprintf(dir, len, "%s/%s", data_dir, "applications");
	if (ret < 0 || (size_t)ret >= len)
		err(1, NULL);

	if ((dirp = opendir(dir)) == NULL)
		goto done;

	apps_list_insert_files(apps, entry_files, dir, dirp, len);

	if (dirp && closedir(dirp) < 0)
		err(1, NULL);

done:
	free(dir);
}

/*
 * Insert all desktop files in a directory into the GtkListStore.
 */
void
apps_list_insert_files(GtkListStore *apps, GHashTable *entry_files, char *dir,
    DIR *dirp, size_t len)
{
	char		*fn = NULL, *name_v = NULL, *exec_v = NULL;
	char		*icon_v = NULL;
	int		 ret;
	size_t		 len_name, len_fn;
	uint8_t		 field_code_flags;
	struct dirent	*dp;
	GKeyFile	*key_file = NULL;
	GError		*error = NULL;
	gboolean	 nodisplay_v, hidden_v, use_term_v;

	while ((dp = readdir(dirp)) != NULL) {
		len_name = strlen(dp->d_name);
		if (len_name <= 8)
			goto cont;
		if (strncmp(dp->d_name + len_name - 8, ".desktop", 8) != 0)
			goto cont;

		len_fn = len_name + len + 1;
		if ((fn = calloc(len_fn, sizeof(char))) == NULL)
			err(1, NULL);
		ret = snprintf(fn, len_fn, "%s/%s", dir, dp->d_name);
		if (ret < 0 || (size_t)ret >= len_fn)
			err(1, NULL);

		key_file = g_key_file_new();
		if (!g_key_file_load_from_file(key_file, fn, G_KEY_FILE_NONE, &error))
			errx(1, "%s", error->message);

		nodisplay_v = g_key_file_get_boolean(key_file,
		    G_KEY_FILE_DESKTOP_GROUP,
		    G_KEY_FILE_DESKTOP_KEY_NO_DISPLAY, NULL);
		if (nodisplay_v != FALSE)
			goto cont;

		name_v = g_key_file_get_locale_string(key_file,
		    G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_NAME,
		    NULL, &error);
		if (name_v == NULL) {
			warnx("g_key_file_get_locale_string: %s",
			    error->message);
			g_clear_error(&error);
			goto cont;
		}

		if (!g_hash_table_add(entry_files, strdup(name_v)))
			goto cont;

		hidden_v = g_key_file_get_boolean(key_file,
		    G_KEY_FILE_DESKTOP_GROUP,
		    G_KEY_FILE_DESKTOP_KEY_HIDDEN, NULL);
		if (hidden_v != FALSE)
			goto cont;

		exec_v = g_key_file_get_locale_string(key_file,
		    G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_EXEC,
		    NULL, &error);
		if (exec_v == NULL) {
			warnx("g_key_file_get_locale_string: %s",
			    error->message);
			g_clear_error(&error);
			goto cont;
		}

		field_code_flags = field_codes(exec_v);

		icon_v = g_key_file_get_locale_string(key_file,
		    G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_ICON,
		    NULL, &error);
		g_clear_error(&error);

		use_term_v = g_key_file_get_boolean(key_file,
		    G_KEY_FILE_DESKTOP_GROUP,
		    G_KEY_FILE_DESKTOP_KEY_TERMINAL, NULL);
		g_clear_error(&error);

		gtk_list_store_insert_with_values(apps, NULL, -1,
		    NAME_COLUMN, name_v,
		    EXEC_COLUMN, exec_v,
		    FCODE_COLUMN, field_code_flags,
		    ICON_COLUMN, icon_v,
		    TERM_COLUMN, use_term_v,
		    -1);

cont:
		free(exec_v); exec_v = NULL;
		free(name_v); name_v = NULL;
		free(icon_v); icon_v = NULL;
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
uint8_t
field_codes(char *cmd)
{
	uint8_t	flags = 0, found_percent = 0;;

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
	GtkTreeModel	*model;
	GtkTreeIter	 iter;
	GValue		 value = G_VALUE_INIT;
	struct state	*st;

	st = (struct state *)user_data;

	if ((model = gtk_tree_view_get_model(tree_view)) == NULL) {
		warnx("gtk_tree_view_get_model is NULL");
		return;
	}

	if (!gtk_tree_model_get_iter(model, &iter, path)) {
		warnx("gtk_tree_model_get_iter: path does not exist");
		return;
	}

	gtk_tree_model_get_value(model, &iter, EXEC_COLUMN, &value);
	if (!G_VALUE_HOLDS_STRING(&value)) {
		warnx("gtk_tree_model_get_value: exec is not a string");
		return;
	}
	st->cmd = g_value_dup_string(&value);
	g_value_unset(&value);

	gtk_tree_model_get_value(model, &iter, FCODE_COLUMN, &value);
	if (!G_VALUE_HOLDS_UINT(&value)) {
		warnx("gtk_tree_model_get_value: flags are not an integer");
		return;
	}
	st->flags = g_value_get_uint(&value);
	g_value_unset(&value);

	gtk_tree_model_get_value(model, &iter, TERM_COLUMN, &value);
	if (!G_VALUE_HOLDS_BOOLEAN(&value)) {
		warnx("gtk_tree_model_get_value: term is not a Boolean");
		return;
	}
	st->use_term = g_value_get_boolean(&value);
	g_value_unset(&value);

	if (run_cmd(st))
		gtk_main_quit();
}

/*
 * A key has been pressed or released. If that key is shift, update the state.
 */
gboolean
key_pressed(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	struct state	*st;
	st = (struct state *)user_data;

	switch (event->type) {
	case GDK_KEY_PRESS:
		if (event->key.keyval == GDK_KEY_Shift_L ||
		    event->key.keyval == GDK_KEY_Shift_R)
			st->shift_pressed = 1;
		break;
	case GDK_KEY_RELEASE:
		if (event->key.keyval == GDK_KEY_Shift_L ||
		    event->key.keyval == GDK_KEY_Shift_R)
			st->shift_pressed = 0;
		break;
	default:
		break;
	}

	return FALSE;
}

/*
 * Handle commands with flags and options, and ultimately run the command.
 */
uint8_t
run_cmd(struct state *st)
{
	char		*new_cmd = NULL;

	new_cmd = strdup(st->cmd);

	if (st->flags) {
		if (st->shift_pressed) {
			new_cmd = fill_in_command(st->cmd, "", st->flags);
			if (new_cmd == NULL) {
				warnx("fill_in_command failed");
				goto done;
			}
		} else if (!fill_in_flags(&new_cmd, st->flags))
			goto done;
	}

	if (st->use_term)
		if (!add_terminal(&new_cmd))
			goto done;

	exec_cmd(new_cmd);
	free(new_cmd);
	return 1;

done:
	free(new_cmd);
	return 0;
}

/*
 * Get text and use that to fill in the placeholder.
 */
uint8_t
fill_in_flags(char **cmd, uint8_t flags)
{
	uint8_t		 ret = 0;
	const char	*text = NULL;
	GtkWidget	*dialog, *box, *entry, *label = NULL;
	GtkEntryBuffer	*buf;
	GValue		 g_9 = G_VALUE_INIT;

	g_value_init(&g_9, G_TYPE_INT);
	g_value_set_int(&g_9, 3);

	dialog = gtk_dialog_new_with_buttons(
	    "Command options",
	    GTK_WINDOW(window),
	     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
	     "_Close", GTK_RESPONSE_CLOSE,
	     "_Run", GTK_RESPONSE_OK,
	     NULL);
	box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	entry = gtk_entry_new();

	gtk_widget_set_size_request(dialog, 300, 20);
	g_object_set_property(G_OBJECT(box), "margin", &g_9);
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);

	if (flags & SINGLE_FILE_PLACEHOLDER)
		label = gtk_label_new("File name");

	if (flags & SINGLE_URL_PLACEHOLDER)
		label = gtk_label_new("URI");

	if (flags & MULTI_FILE_PLACEHOLDER)
		label = gtk_label_new("Files");

	if (flags & MULTI_URL_PLACEHOLDER)
		label = gtk_label_new("URIs");

	if (label) {
		gtk_box_pack_start(GTK_BOX(box), label, /* expand */ 0,
		    /* fill */ 1, /* padding */ 3);
		gtk_box_pack_start(GTK_BOX(box), entry, /* expand */ 1,
		    /* fill */ 1, /* padding */ 3);
	}

	gtk_widget_show_all(box);

	switch (gtk_dialog_run(GTK_DIALOG(dialog))) {
	case GTK_RESPONSE_OK:
		buf = gtk_entry_get_buffer(GTK_ENTRY(entry));
		text = gtk_entry_buffer_get_text(buf);

		if ((*cmd = fill_in_command(*cmd, text, flags)) == NULL) {
			warnx("fill_in_command failed");
			break;
		}

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

	return ret;
}

/*
 * Prefix the cmd with a terminal emulator.
 */
uint8_t
add_terminal(char **cmd)
{
	char	*emulator, *new_cmd;
	int	 ret;
	size_t	 len_new_cmd;

	if ((emulator = getenv("TERMINAL")) == NULL)
		emulator = strdup("xterm");
	if (emulator == NULL) {
		warn("getenv or strdup");
		return 0;
	}

	len_new_cmd = strlen(*cmd) + strlen(emulator) + 5;
	if ((new_cmd = calloc(len_new_cmd, sizeof(char))) == NULL) {
		warn("calloc");
		return 0;
	}

	ret = snprintf(new_cmd, len_new_cmd, "%s -e %s", emulator, *cmd);
	if (ret < 0 || (size_t)ret >= len_new_cmd) {
		warn("snprintf");
		free(new_cmd);
		return 0;
	}

	*cmd = strdup(new_cmd);
	free(new_cmd);
	return 1;
}

/*
 * Execute the command.
 */
void
exec_cmd(const char *cmd)
{
	int	 status;
	pid_t	 pid;
	gchar	**argv;
	GError	*errors = NULL;

	if (!g_shell_parse_argv(cmd, NULL, &argv, &errors)) {
		if (errors)
			warnx("g_shell_parse_argv: %s", errors->message);
		else
			warnx("g_shell_parse_argv failed on: %s", cmd);
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

/*
 * Replace the first placeholder with the text entered by the user.
 */
char *
fill_in_command(const char *cmd, const char *interp, uint8_t flags)
{
	char		*new_cmd = NULL, *p;
	const char	*placeholder;
	size_t		 len_cmd, len_interp, len_ph, len_p;

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

/*
 * Determine the placeholder from the flags.
 */
const char *
placeholder_from_flags(uint8_t flags)
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
