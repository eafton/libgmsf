#include <glib.h>
#include <libpe/pe.h>

#ifndef PE_UTIL_H
#define PE_UTIL_H

gpointer _pe_extract_resource(pe_ctx_t *ctx, pe_resources_t *res, const gchar *name, const gchar *ch, gsize *sz, gboolean *free_needed);

#endif
