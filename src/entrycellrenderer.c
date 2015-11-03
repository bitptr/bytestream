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

#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include "entrycellrenderer.h"
#include "compat.h"

#define CELL_HEIGHT 32

enum {
	PROP_0,
	PROP_NAME,
	PROP_EXEC,
	PROP_ICON,
	NUM_PROPS,
};

struct _BsCellRendererEntryPrivate {
	char	*name;
	char	*exec;
	char	*icon;
};

static void	bs_cell_renderer_entry_class_init(BsCellRendererEntryClass *);
static void	bs_cell_renderer_entry_init(BsCellRendererEntry *);
static void	bs_cell_renderer_entry_get_property(GObject *, guint, GValue *,
    GParamSpec *);
static void	bs_cell_renderer_entry_set_property(GObject *, guint,
    const GValue *, GParamSpec *);
static void	bs_cell_renderer_entry_get_size(GtkCellRenderer *,
    GtkWidget *, const GdkRectangle *, gint *, gint *, gint *, gint *);
static void	bs_cell_renderer_entry_render(GtkCellRenderer *,
    cairo_t *, GtkWidget *, const GdkRectangle *,
    const GdkRectangle *, GtkCellRendererState);
char		*resolve_icon(char *);

G_DEFINE_TYPE_WITH_PRIVATE(
    BsCellRendererEntry, bs_cell_renderer_entry, GTK_TYPE_CELL_RENDERER)

static void
bs_cell_renderer_entry_class_init(BsCellRendererEntryClass *klass)
{
	GObjectClass		*object_class;
	GtkCellRendererClass	*cell_class;

	object_class = G_OBJECT_CLASS(klass);
	cell_class = GTK_CELL_RENDERER_CLASS(klass);

	object_class->get_property = bs_cell_renderer_entry_get_property;
	object_class->set_property = bs_cell_renderer_entry_set_property;

	cell_class->get_size = bs_cell_renderer_entry_get_size;
	cell_class->render = bs_cell_renderer_entry_render;

	/* property: "name" */
	g_object_class_install_property(object_class,
	    PROP_NAME,
	    g_param_spec_string("name", "Name", "The name of the entry",
		    NULL, G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

	/* property: "exec" */
	g_object_class_install_property(object_class,
	    PROP_EXEC,
	    g_param_spec_string("exec", "Exec", "The Exec from the entry",
		    NULL, G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

	/* property: "icon" */
	g_object_class_install_property(object_class,
	    PROP_ICON,
	    g_param_spec_string("icon", "Icon", "The Icon from the entry",
		    NULL, G_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
}

static void
bs_cell_renderer_entry_init(BsCellRendererEntry *cell)
{
	cell->priv = bs_cell_renderer_entry_get_instance_private(cell);
	cell->priv->name = NULL;
	cell->priv->exec = NULL;
	cell->priv->icon = NULL;
}

GtkCellRenderer *
bs_cell_renderer_entry_new(void)
{
	return g_object_new(BS_TYPE_CELL_RENDERER_ENTRY, NULL);
}

static void
bs_cell_renderer_entry_get_property(GObject *object, guint param_id,
    GValue *value, GParamSpec *pspec)
{
	BsCellRendererEntry		*cell;
	BsCellRendererEntryPrivate	*priv;

	cell = BS_CELL_RENDERER_ENTRY(object);
	priv = cell->priv;

	switch (param_id) {
	case PROP_NAME:
		g_value_set_string(value, priv->name);
		break;
	case PROP_EXEC:
		g_value_set_string(value, priv->exec);
		break;
	case PROP_ICON:
		g_value_set_string(value, priv->icon);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
	}
}

static void
bs_cell_renderer_entry_set_property(GObject *object, guint param_id,
    const GValue *value, GParamSpec *pspec)
{
	BsCellRendererEntry		*cell;
	BsCellRendererEntryPrivate	*priv;

	cell = BS_CELL_RENDERER_ENTRY(object);
	priv = cell->priv;

	switch (param_id) {
	case PROP_NAME:
		free(priv->name);
		priv->name = g_value_dup_string(value);
		g_object_notify_by_pspec(object, pspec);
		break;
	case PROP_EXEC:
		free(priv->exec);
		priv->exec = g_value_dup_string(value);
		g_object_notify_by_pspec(object, pspec);
		break;
	case PROP_ICON:
		free(priv->icon);
		priv->icon = resolve_icon(g_value_dup_string(value));
		g_object_notify_by_pspec(object, pspec);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
	}
}

static void
bs_cell_renderer_entry_get_size(GtkCellRenderer *cell,
    GtkWidget *widget, const GdkRectangle *cell_area, gint *x_offset,
    gint *y_offset, gint *width, gint *height)
{
	int	xpad, ypad;

	g_object_get(cell, "xpad", &xpad, "ypad", &ypad, NULL);

	if (height)
		*height = CELL_HEIGHT + 2 * ypad;
	if (width)
		*width = 300 + 2 * xpad;
}

static void
bs_cell_renderer_entry_render(GtkCellRenderer *cellr, cairo_t *cr,
    GtkWidget *widget, const GdkRectangle *background_area,
    const GdkRectangle *cell_area, GtkCellRendererState flags)
{
	int		 		 name_width, name_height;
	int				 xpad, ypad, icon_offset = CELL_HEIGHT;
	BsCellRendererEntry		*cell;
	BsCellRendererEntryPrivate	*priv;
	GtkStyleContext			*style_ctx;
	PangoContext			*pango_ctx;
	PangoLayout			*name_layout, *cmd_layout;
	PangoAttrList			*list;
	PangoAttribute			*attr;
	GdkPixbuf			*icon = NULL;
	GError				*error = NULL;

	cell = BS_CELL_RENDERER_ENTRY(cellr);
	priv = cell->priv;

	style_ctx = gtk_widget_get_style_context(widget);
	pango_ctx = gtk_widget_get_pango_context(widget);
	name_layout = pango_layout_new(pango_ctx);
	cmd_layout = pango_layout_new(pango_ctx);
	attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
	list = pango_attr_list_new();

	g_object_get(cellr, "xpad", &xpad, "ypad", &ypad, NULL);

	pango_layout_set_text(name_layout, priv->name, -1);
	pango_layout_set_text(cmd_layout, priv->exec, -1);
	pango_layout_get_pixel_size(name_layout, &name_width, &name_height);

	attr->start_index = 0;
	attr->end_index = strlen(priv->name);
	pango_attr_list_insert(list, attr);
	pango_layout_set_attributes(name_layout, list);

	if (priv->icon)
		icon = gdk_pixbuf_new_from_file_at_size(priv->icon, CELL_HEIGHT, -1, &error);
	if (icon)
		gtk_render_icon(style_ctx, cr, icon, cell_area->x + xpad,
		    cell_area->y + ypad);

	gtk_render_layout(style_ctx, cr,
	    icon_offset + xpad + cell_area->x + xpad,
	    cell_area->y + ypad, name_layout);
	gtk_render_layout(style_ctx, cr,
	    icon_offset + xpad + cell_area->x + xpad,
	    cell_area->y + name_height + ypad, cmd_layout);

	pango_attr_list_unref(list);
}

char *
resolve_icon(char *name)
{
	char		*fn;
	struct stat	sb;
	GtkIconInfo	*info;

	if (!name)
		return NULL;

	if (stat(name, &sb) == 0)
		return name;

	info = gtk_icon_theme_lookup_icon(gtk_icon_theme_get_default(), name,
	    CELL_HEIGHT, 0);

	if (info) {
		fn = strdup(gtk_icon_info_get_filename(info));
		g_object_unref(info);
		return fn;
	}

	return NULL;
}
