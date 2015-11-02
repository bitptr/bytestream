#ifndef ENTRTYCELLRENDERER_H
#define ENTRTYCELLRENDERER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define BS_TYPE_CELL_RENDERER_ENTRY bs_cell_renderer_entry_get_type()
#define BS_CELL_RENDERER_ENTRY(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), BS_TYPE_CELL_RENDERER_ENTRY, BsCellRendererEntry))

typedef struct _BsCellRendererEntry        BsCellRendererEntry;
typedef struct _BsCellRendererEntryClass   BsCellRendererEntryClass;
typedef struct _BsCellRendererEntryPrivate BsCellRendererEntryPrivate;

struct _BsCellRendererEntry {
	GtkCellRenderer			 parent;
	BsCellRendererEntryPrivate	*priv;
};

struct _BsCellRendererEntryClass {
	GtkCellRendererClass	parent_class;
};

GType		 bs_cell_renderer_entry_get_type(void);
GtkCellRenderer	*bs_cell_renderer_entry_new(void);

G_END_DECLS

#endif /* ENTRTYCELLRENDERER_H */
