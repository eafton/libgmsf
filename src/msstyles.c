#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libpe/pe.h>
#include <gmsf.h>
#include "peutil.h"

struct _GMSFStylesPrivate {
    pe_ctx_t ctx;
	pe_resources_t *res;	
	gchar *filename;
		
	guint version;
	gchar **color_schemes;
	gchar **size_schemes;
	gchar *filemap;
    GKeyFile *themesini;
    
	const gchar *selected_color;
    const gchar *selected_size;
    GKeyFile *selectedini;
};

enum {
	PROP_FILENAME = 1,
	PROP_VERSION,
	PROP_COLOR_SCHEMES,
	PROP_SIZE_SCHEMES,
	PROP_SELECTED_COLOR_SCHEME,
	PROP_SELECTED_SIZE_SCHEME
};

G_DEFINE_TYPE(GMSFStyles, gmsf_styles, G_TYPE_OBJECT)

static guint32 read_le32(const guint8 *p) {
    return (guint32)p[0] | ((guint32)p[1] << 8) | ((guint32)p[2] << 16) | ((guint32)p[3] << 24);
}

static guint16 read_le16(const guint8 *p) {
    return (guint16)p[0] | ((guint16)p[1] << 8);
}

static guint8 *dib_to_bmp(const guint8 *data, gsize size, gsize *out_size) {
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

static GdkPixbuf *load_bitmap(gpointer data, gsize sz) {
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
		
		bytes = dib_to_bmp(bytes, sz, &bmp_sz);
        gdk_pixbuf_loader_write(loader, (const guchar *)bytes, bmp_sz, NULL);
        gdk_pixbuf_loader_close(loader, NULL);
        pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
		g_free((gpointer)bytes);
	}
	g_object_ref(pixbuf);
    g_object_unref(loader);

	return pixbuf;
}

static gchar *clean_ini_comments(const gchar *text) {
    GString *out;
    const gchar *p;

    out = g_string_new(NULL);
    for (p = text; *p; p++) {
        if (*p == ';') {
            while (*p && *p != '\n') {
                p++;
			}
            
            if (!*p) {
                break;
			}
        }
        g_string_append_c(out, *p);
    }
    return g_string_free(out, FALSE);
}

static void select_scheme_ini(GMSFStyles *styles) {
	GdkPixbuf *pixbuf;
	gpointer extracted;
	gchar **test;
	gchar *cur, *str;
	gsize sz;
	gint color_i, size_i, file_i, size_sz, i;
	gboolean free;

	size_sz = color_i = size_i = file_i = 0;
	
	for (i = 0; styles->priv->color_schemes[i]; i++) {
		if (!strcmp(styles->priv->color_schemes[i], styles->priv->selected_color)) {
			color_i = i;
		}
	}

	for (i = 0; styles->priv->size_schemes[i]; i++) {
		if (!strcmp(styles->priv->size_schemes[i], styles->priv->selected_size)) {
			size_i = i;
		}
		size_sz++;
	}
	
	cur = styles->priv->filemap;
    file_i = (size_sz * color_i) + size_i;
    for (i = 0; i < file_i; i++) {
        cur += strlen(cur) + 1;
    }

	if (styles->priv->selectedini) {
		g_key_file_free(styles->priv->selectedini);	
	}
	
	extracted = _pe_extract_resource(&styles->priv->ctx, styles->priv->res, "TEXTFILE", cur, &sz, &free);
	str = g_convert(extracted, (gssize)sz, "UTF-8", "UTF-16LE", NULL, NULL, NULL);
	cur = clean_ini_comments(str);
	styles->priv->selectedini = g_key_file_new();
    g_key_file_load_from_data(styles->priv->selectedini, cur, -1, G_KEY_FILE_NONE, NULL);
	g_free(cur);
	g_free(str);
	if (free) {
		g_free(extracted);
	}

	test = g_key_file_get_groups(styles->priv->selectedini, NULL);
	for (i = 0; test[i]; i++) {
		puts(test[i]);
	}
	g_strfreev(test);
	
	extracted = _pe_extract_resource(&styles->priv->ctx, styles->priv->res, "BLUE_RADIOBUTTON25_BMP", NULL, &sz, &free);
	pixbuf = load_bitmap(extracted, sz);
	gdk_pixbuf_save(pixbuf, "test.png", "png", NULL, NULL);
	gdk_pixbuf_unref(pixbuf);
	if (free) {
		g_free(extracted);
	}
}

static void _finalize(GObject *object) {
	GMSFStyles *styles;

	styles = GMSF_STYLES(object);

	g_free(styles->priv->filename);
	g_free(styles->priv->filemap);
	g_strfreev(styles->priv->size_schemes);
	g_strfreev(styles->priv->color_schemes);
	g_key_file_free(styles->priv->themesini);	
	g_key_file_free(styles->priv->selectedini);	
	pe_unload(&styles->priv->ctx);

    G_OBJECT_CLASS(gmsf_styles_parent_class)->finalize(object);
}

static void _constructed(GObject *object) {
	GMSFStyles *styles;
	gchar *str;
	gchar *cur;
	gpointer extracted;
	gsize sz, c, a;
	gboolean free;
	guint i;
	
	styles = GMSF_STYLES(object);
    G_OBJECT_CLASS(gmsf_styles_parent_class)->constructed(object);
    
	pe_load_file(&styles->priv->ctx, styles->priv->filename);
	pe_parse(&styles->priv->ctx);    
	styles->priv->res = pe_resources(&styles->priv->ctx);
	
	/* VERSİON */
	sz = 0;
	styles->priv->version = 0;
    extracted = _pe_extract_resource(&styles->priv->ctx, styles->priv->res, "PACKTHEM_VERSION", NULL, &sz, &free);
	if (extracted && sz == 2) {
		styles->priv->version = *(guint16*)extracted;
	}
	
	/* COLORNAMES */
	i = 0;
	extracted = _pe_extract_resource(&styles->priv->ctx, styles->priv->res, "COLORNAMES", NULL, &sz, &free);
	cur = str = g_convert(extracted, (gssize)sz, "UTF-8", "UTF-16LE", NULL, NULL, NULL);
	while(*cur) {
		cur += strlen(cur) + 1;
		i++;
	}
	styles->priv->color_schemes = g_new(gchar *,  i + 1);
	i = 0;
	cur = str;
	while(*cur) {
		styles->priv->color_schemes[i] = g_strdup(cur);
		cur += strlen(cur) + 1;
		i++;
	}
	styles->priv->color_schemes[i] = NULL;
	g_free(str);
	if (free) {
		g_free(extracted);
	}
	
	/* SIZENAMES */
	i = 0;
	extracted = _pe_extract_resource(&styles->priv->ctx, styles->priv->res, "SIZENAMES", NULL, &sz, &free);
	cur = str = g_convert(extracted, (gssize)sz, "UTF-8", "UTF-16LE", NULL, NULL, NULL);
	while(*cur) {
		cur += strlen(cur) + 1;
		i++;
	}
	styles->priv->size_schemes = g_new(gchar *,  i + 1);
	i = 0;
	cur = str;
	while(*cur) {
		styles->priv->size_schemes[i] = g_strdup(cur);
		cur += strlen(cur) + 1;
		i++;
	}
	styles->priv->size_schemes[i] = NULL;
	g_free(str);
	if (free) {
		g_free(extracted);
	}
	
	/* FILERESNAMES */
	extracted = _pe_extract_resource(&styles->priv->ctx, styles->priv->res, "FILERESNAMES", NULL, &sz, &free);
	styles->priv->filemap = g_convert(extracted, (gssize)sz, "UTF-8", "UTF-16LE", NULL, NULL, NULL);
	if (free) {
		g_free(extracted);
	}
	
	/* THEMES_INI */
	extracted = _pe_extract_resource(&styles->priv->ctx, styles->priv->res, "TEXTFILE", "THEMES_INI", &sz, &free);
	str = g_convert(extracted, (gssize)sz, "UTF-8", "UTF-16LE", NULL, NULL, NULL);
	cur = clean_ini_comments(str);
    styles->priv->themesini = g_key_file_new();
    g_key_file_load_from_data(styles->priv->themesini, cur, -1, G_KEY_FILE_NONE, NULL);
	g_free(cur);
	g_free(str);
	if (free) {
		g_free(extracted);
	}
}

static void _set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
	GMSFStyles *styles;
	
	styles = GMSF_STYLES(object);
	switch (prop_id) {
		case PROP_SELECTED_COLOR_SCHEME:
			styles->priv->selected_color = g_value_get_string(value);	
			select_scheme_ini(styles);
			break;
		case PROP_SELECTED_SIZE_SCHEME:
			styles->priv->selected_size	= g_value_get_string(value);	
			select_scheme_ini(styles);
			break;
		case PROP_FILENAME:
			styles->priv->filename = g_value_dup_string(value);
			break;
		case PROP_VERSION:
			styles->priv->version = g_value_get_uint(value);
			break;
		case PROP_COLOR_SCHEMES:
			g_strfreev(styles->priv->color_schemes);
			styles->priv->color_schemes = g_strdupv(g_value_get_boxed(value)); 
			break;
		case PROP_SIZE_SCHEMES:
			g_strfreev(styles->priv->size_schemes);
			styles->priv->size_schemes = g_strdupv(g_value_get_boxed(value)); 
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}

static void _get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
	GMSFStyles *styles;
	
	styles = GMSF_STYLES(object);
	switch (prop_id) {
		case PROP_FILENAME:
			g_value_set_string(value, styles->priv->filename);
			break;
		case PROP_VERSION:
			g_value_set_uint(value, styles->priv->version);
			break;
		case PROP_COLOR_SCHEMES:
			g_value_set_boxed(value, styles->priv->color_schemes);
			break;
		case PROP_SIZE_SCHEMES:
			g_value_set_boxed(value, styles->priv->size_schemes);
			break;
		case PROP_SELECTED_COLOR_SCHEME:
			g_value_set_string(value, styles->priv->selected_color);
			break;
		case PROP_SELECTED_SIZE_SCHEME:
			g_value_set_string(value, styles->priv->selected_size);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}

static void gmsf_styles_class_init(GMSFStylesClass *klass) {
	GObjectClass *g_class;

	g_class = G_OBJECT_CLASS(klass);
	
	g_class->get_property = _get_property;
	g_class->set_property = _set_property;
	g_class->constructed = _constructed;
	g_class->finalize = _finalize;
	
    g_object_class_install_property(g_class, PROP_FILENAME, g_param_spec_string("filename", "nick", "blurb", NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property(g_class, PROP_VERSION, g_param_spec_uint("version", "nick", "blurb", 0, 200, 0, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property(g_class, PROP_COLOR_SCHEMES, g_param_spec_boxed("color-schemes", "nick", "blurb", G_TYPE_STRV, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property(g_class, PROP_SIZE_SCHEMES, g_param_spec_boxed("size-schemes", "nick", "blurb", G_TYPE_STRV, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property(g_class, PROP_SELECTED_COLOR_SCHEME, g_param_spec_string("selected-color-scheme", "nick", "blurb", NULL, G_PARAM_READWRITE));
    g_object_class_install_property(g_class, PROP_SELECTED_SIZE_SCHEME, g_param_spec_string("selected-size-scheme", "nick", "blurb", NULL, G_PARAM_READWRITE));

	g_type_class_add_private(klass, sizeof(GMSFStylesPrivate));
}	

static void gmsf_styles_init(GMSFStyles *styles) {
	styles->priv = G_TYPE_INSTANCE_GET_PRIVATE(styles, GMSF_TYPE_STYLES, GMSFStylesPrivate);
	styles->priv->selectedini = NULL;
}

GMSFStyles *gmsf_styles_new_from_file(const gchar *filename) {
	return g_object_new(GMSF_TYPE_STYLES, "filename", filename, NULL);	
}

guint gmsf_styles_get_version(GMSFStyles *styles) {
	return styles->priv->version;
}

const gchar **gmsf_styles_get_color_schemes(GMSFStyles *styles) {
	return (const gchar **)styles->priv->color_schemes;
}

const gchar **gmsf_styles_get_size_schemes(GMSFStyles *styles) {
	return (const gchar **)styles->priv->size_schemes;
}

void gmsf_styles_select_scheme(GMSFStyles *styles, const gchar *color, const gchar *size) {
	styles->priv->selected_color = color;
	styles->priv->selected_size = size;
	g_object_notify(G_OBJECT(styles), "selected-color-scheme");
	g_object_notify(G_OBJECT(styles), "selected-size-scheme");
	select_scheme_ini(styles);
}
