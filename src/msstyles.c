#include <glib-object.h>
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
};

enum {
	PROP_FILENAME = 1,
	PROP_VERSION,
	PROP_COLOR_SCHEMES,
	PROP_SIZE_SCHEMES
};

G_DEFINE_TYPE(GMSFStyles, gmsf_styles, G_TYPE_OBJECT)

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

static void _finalize(GObject *object) {
	GMSFStyles *styles;

	styles = GMSF_STYLES(object);

	g_free(styles->priv->filename);
	g_free(styles->priv->filemap);
	g_strfreev(styles->priv->size_schemes);
	g_strfreev(styles->priv->color_schemes);
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
	puts(cur);
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

	g_type_class_add_private(klass, sizeof(GMSFStylesPrivate));
}	

static void gmsf_styles_init(GMSFStyles *styles) {
	styles->priv = G_TYPE_INSTANCE_GET_PRIVATE(styles, GMSF_TYPE_STYLES, GMSFStylesPrivate);
}

GMSFStyles *gmsf_styles_new_from_file(const gchar *filename) {
	return g_object_new(GMSF_TYPE_STYLES, "filename", filename, NULL);	
}

#if 0
void msstyles_select_scheme(msstyles_t *mss, size_t color_index, size_t size_index) {
	char *cur;
	int ri, i;
	
	cur = mss->filemap;
    ri = (mss->size_schemes_sz * color_index) + size_index;
    for (i = 0; i < ri; i++) {
        cur += strlen(cur) + 1;
    }
    mss->current_ini = cur;
}
#endif
