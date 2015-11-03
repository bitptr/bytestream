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

#ifndef _ENTRTYCELLRENDERER_H
#define _ENTRTYCELLRENDERER_H

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

#endif /* _ENTRTYCELLRENDERER_H */
