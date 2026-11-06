#include <stdio.h>
#include <glib.h>
#include <libpe/pe.h>

static const pe_resource_node_t *first_data_entry(const pe_resource_node_t *node) {
    const pe_resource_node_t *child;
    const pe_resource_node_t *found;

    if (!node) {
        return NULL;
	}
	
    found = NULL;

    if (node->type == LIBPE_RDT_DATA_ENTRY) {
        return node;
	}
	
    for (child = node->childNode; child; child = child->nextNode) {
        found = first_data_entry(child);
        if (found) {
			return found;
		}
    }
    return NULL;
}

static const pe_resource_node_t *resource_node_by_name(const pe_resource_node_t *node, const gchar *name) {
    const pe_resource_node_t *child;
    const pe_resource_node_t *found;

    if (!node) {
        return NULL;
	}
	
    found = NULL;

    if (node->name && !g_ascii_strcasecmp(node->name, name)) {
        return node;
	}
	
    for (child = node->childNode; child; child = child->nextNode) {
        found = resource_node_by_name(child, name);
        if (found) {
            return found;
        }
    }

    return NULL;
}

gpointer _pe_extract_resource(pe_ctx_t *ctx, pe_resources_t *res, const gchar *name, const gchar *ch, gsize *sz, gboolean *free_needed) {
    const pe_resource_node_t *target_node;
    void *content;

	target_node = NULL;
	content = NULL;
    if (sz) {
        *sz = 0;
	}

    if (res->root_node) {
        target_node = resource_node_by_name(res->root_node, name);

        if (target_node) {
            const pe_resource_node_t *data_node;
            
            if (ch) {
				data_node = first_data_entry(resource_node_by_name(target_node, ch));
			} else {
				data_node = first_data_entry(target_node);
			}
			
            if (data_node && data_node->type == LIBPE_RDT_DATA_ENTRY) {
                IMAGE_RESOURCE_DATA_ENTRY *entry;
                
                entry = data_node->raw.dataEntry;
                if (entry) {
                    gsize offset;
                    
                    if (sz) {
						*sz = entry->Size;
					}
					
                    offset = pe_rva2ofs(ctx, entry->OffsetToData);
					if (ctx->stream) {
						gsize cur;
						
						content = g_malloc(entry->Size);
						cur = ftell(ctx->stream);
						fseek(ctx->stream, offset, SEEK_SET);
						fread(content, 1, entry->Size, ctx->stream);
						fseek(ctx->stream, cur, SEEK_SET);
						
						if (free_needed) {
							*free_needed = TRUE;
						}
					} else if (ctx->map_addr && ctx->map_size) {
						content = ctx->map_addr + offset;
						
						if (free_needed) {
							*free_needed = FALSE;
						}
					}
                }
            }
        }
    }

    return content;
}
