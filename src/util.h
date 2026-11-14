#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libpe/pe.h>

#ifndef PE_UTIL_H
#define PE_UTIL_H

gpointer _pe_extract_resource(pe_ctx_t *ctx, pe_resources_t *res, const gchar *name, const gchar *ch, gsize *sz, gboolean *free_needed);
guint8 *_dib_to_bmp(const guint8 *data, gsize size, gsize *out_size);
GdkPixbuf *_load_bitmap(gpointer data, gsize sz);

#endif
