#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include "entrycellrenderer.h"

#define CELL_HEIGHT 30

enum {
	PROP_0,
	PROP_NAME,
	PROP_EXEC,
	NUM_PROPS,
};

struct _BsCellRendererEntryPrivate {
	char	*name;
	char	*exec;
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
}

static void
bs_cell_renderer_entry_init(BsCellRendererEntry *cell)
{
	cell->priv = bs_cell_renderer_entry_get_instance_private(cell);
	cell->priv->name = NULL;
	cell->priv->exec = NULL;
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
	int				 xpad, ypad;
	BsCellRendererEntry		*cell;
	BsCellRendererEntryPrivate	*priv;
	GtkStyleContext			*style_ctx;
	PangoContext			*pango_ctx;
	PangoLayout			*name_layout, *cmd_layout;
	PangoAttrList			*list;
	PangoAttribute			*attr;

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

	gtk_render_layout(style_ctx, cr, cell_area->x + xpad,
	    cell_area->y + ypad, name_layout);
	gtk_render_layout(style_ctx, cr, cell_area->x + xpad,
	    cell_area->y + name_height + ypad, cmd_layout);

	pango_attr_list_unref(list);
}
