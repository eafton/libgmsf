#include <stdio.h>
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libpe/pe.h>
#include "util.h"

GdkPixbuf *_load_bitmap(gpointer data, gsize sz) {
	GdkPixbufLoader *loader;
    GdkPixbuf *pixbuf;
	const guint8 *bytes;
    
    loader = gdk_pixbuf_loader_new();
    bytes = data;
    if (bytes[0] == 'B' && bytes[1] == 'M') {
        gdk_pixbuf_loader_write(loader, (const guchar *)bytes, sz, NULL);
        gdk_pixbuf_loader_close(loader, NULL);
        pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
    } else {
		gsize bmp_sz;
		
		bytes = _dib_to_bmp(bytes, sz, &bmp_sz);
        gdk_pixbuf_loader_write(loader, (const guchar *)bytes, bmp_sz, NULL);
        gdk_pixbuf_loader_close(loader, NULL);
        pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
		g_free((gpointer)bytes);
	}
	g_object_ref(pixbuf);
    g_object_unref(loader);

	return pixbuf;
}

static guint32 read_le32(const guint8 *p) {
    return (guint32)p[0] | ((guint32)p[1] << 8) | ((guint32)p[2] << 16) | ((guint32)p[3] << 24);
}

static guint16 read_le16(const guint8 *p) {
    return (guint16)p[0] | ((guint16)p[1] << 8);
}

guint8 *_dib_to_bmp(const guint8 *data, gsize size, gsize *out_size) {
    const guint8 *bytes;
    guint32 header_size;
    guint16 bit_count;
    guint32 compression;
    guint32 clr_used;
    guint32 palette_bytes;
    guint32 pixel_offset;
    guint32 file_size;
    guint8 *result;
    guint8 file_header[14];
    gboolean is_32bit_alpha;

	bytes = data;
    if (out_size) {
        *out_size = 0;
	}
	
    if (!data || size < 40) {
        return NULL;
	}
	
    header_size = read_le32(bytes);
    bit_count = read_le16(bytes + 14);
    compression = read_le32(bytes + 16);
    clr_used = read_le32(bytes + 32);
    is_32bit_alpha = (bit_count == 32 && header_size == 40 && !compression);

    palette_bytes = 0;
    if (bit_count > 0 && bit_count <= 8) {
        palette_bytes = (clr_used ? clr_used : ((guint32)1 << bit_count)) * 4;
    }

    if (is_32bit_alpha) {
        pixel_offset = 14 + 56;
        file_size = pixel_offset + ((guint32)size - header_size);
    } else {
        pixel_offset = 14 + header_size + ((compression == 3 && header_size == 40) ? 12 : 0) + palette_bytes;
        file_size = 14 + (guint32)size;
    }

    file_header[0] = 'B';
    file_header[1] = 'M';
    file_header[2] = (guint8)(file_size & 0xff);
    file_header[3] = (guint8)((file_size >> 8) & 0xff);
    file_header[4] = (guint8)((file_size >> 16) & 0xff);
    file_header[5] = (guint8)((file_size >> 24) & 0xff);
    file_header[6] = 0;
    file_header[7] = 0;
    file_header[8] = 0;
    file_header[9] = 0;
    file_header[10] = (guint8)(pixel_offset & 0xff);
    file_header[11] = (guint8)((pixel_offset >> 8) & 0xff);
    file_header[12] = (guint8)((pixel_offset >> 16) & 0xff);
    file_header[13] = (guint8)((pixel_offset >> 24) & 0xff);

    if (is_32bit_alpha) {
        result = g_malloc(file_size);
        memcpy(result, file_header, 14);

        result[14] = 56;
        result[15] = 0;
        result[16] = 0;
        result[17] = 0;

        memcpy(result + 18, bytes + 4, 12);

        result[30] = 3;
        result[31] = 0;
        result[32] = 0;
        result[33] = 0;

        memcpy(result + 34, bytes + 20, 20);

        result[54] = 0;
        result[55] = 0;
        result[56] = 0xFF;
        result[57] = 0;

        result[58] = 0;
        result[59] = 0xFF;
        result[60] = 0;
        result[61] = 0;

        result[62] = 0xFF;
        result[63] = 0;
        result[64] = 0;
        result[65] = 0;

        result[66] = 0;
        result[67] = 0;
        result[68] = 0;
        result[69] = 0xFF;

        memcpy(result + 70, bytes + 40, size - 40);
    } else {
        result = g_malloc(file_size);
        memcpy(result, file_header, 14);
        memcpy(result + 14, data, size);
    }

    if (out_size) {
        *out_size = file_size;
	}
	
    return result;
}

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
