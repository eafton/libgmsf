#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libpe/pe.h>
#include <gmsf.h>
#include "util.h"

struct _GMSFStylesPrivate {
    pe_ctx_t ctx;
	pe_resources_t *res;	
	gchar *filename;
		
	guint target_dpi;
	
	guint version;
	gchar **color_schemes;
	gchar **size_schemes;
	gchar *filemap;
    GKeyFile *themesini;
    
	const gchar *selected_color;
    const gchar *selected_size;
    GKeyFile *selectedini;
    
    GHashTable *image_cache;
    
    struct {
		gboolean flat_menus;
		gchar *css;
		gchar *xml;
		gint captionbarheight;
		gint min_depth;
		GMSFSize smcaptionbar_size;
		GMSFSize scrollbar_size;
		GMSFColor window;
		GMSFColor menubar;
		GMSFColor menu;
		GMSFColor background;
		GMSFColor btnface;
		GMSFColor highlight;
		GMSFColor activecaption;
		GMSFColor captiontext;
		GMSFColor inactivecaption;
		GMSFColor inactivecaptiontext;
		GMSFColor gradientactivecaption;
		GMSFColor gradientinactivecaption;
		GMSFColor highlighttext;
		GMSFColor menuhilight;
		GMSFColor hottracking;
		GMSFColor btnshadow;
		GMSFColor graytext;
		GMSFColor btnhighlight;
		GMSFColor dkshadow3d;
		GMSFColor light3d;
		gchar *captionfont;
		gchar *smallcaptionfont;
		gchar *menufont;
		gchar *statusfont;
		gchar *msgboxfont;
		gchar *icontitlefont;
	} sysmetrics;
	
    struct {
		GMSFColor edge_light_color;
		GMSFColor edge_high_light_color;
		GMSFColor edge_shadow_color;
		GMSFColor edge_dk_shadow_color;
		GMSFColor edge_fill_color;
	} globals;
};

enum {
	PROP_FILENAME = 1,
	PROP_VERSION,
	PROP_COLOR_SCHEMES,
	PROP_SIZE_SCHEMES,
	PROP_SELECTED_COLOR_SCHEME,
	PROP_SELECTED_SIZE_SCHEME,
	PROP_TARGET_DPI
};

G_DEFINE_TYPE(GMSFStyles, gmsf_styles, G_TYPE_OBJECT)

static gint count_digits(gchar *str) {
	gchar *tmp;
	gint ret;
	
	ret = 0;
	tmp = str;
    do {
		ret += g_ascii_isdigit(*tmp);
    } while (*tmp++);
    
    return ret;
}

static gboolean is_all_digits(gchar *str) {
	gchar *tmp;
	gboolean ret;
	
	ret = TRUE;
	tmp = str;
    do {
		if (!g_ascii_isdigit(*tmp) && (*tmp != '\0')) {
			ret = FALSE;
		}
    } while (*tmp++);

    return ret;
}

static GMSFColor parse_color(gchar *str) {
	guint8 r, g, b;
	gchar *d;
	char *k;
	gint i;
	
	i = 0;
	r = g = b = 0;
	d = g_strdup(str);
	k = strtok(d, " ");	
	while (k) {
		if (k) {
			if (count_digits(k) <= 3) {
				if (!i) {
					r = atoi(k);
				} else if (i == 1) {
					g = atoi(k);
				} else {
					b = atoi(k);
				}
			}
		}
		
		k = strtok(NULL, " ");
		i++;
	}
	g_free(d);
	
	return GMSF_COLOR_PACK(r, g, b, 255);
}

static gchar *font_to_pango(gchar *str) {
	gchar *d, *family, *size, *style, *pango;
	char *k;
	gint i;
	
	i = 0;
	family = size = style = NULL;
	d = g_strdup(str);
	k = strtok(d, ",");	
	while (k) {
		if (k) {
			if (!i) {
				family = g_strdup(k);
			} else {
				gchar *t;
				
				t = g_strstrip(g_strdup(k));
				if (is_all_digits(t)) {
					size = g_strdup(t);
				} else {
					style = g_strdup(t);
					style[0] = g_ascii_toupper(style[0]);
				}
				g_free(t);
			}
		}
		
		k = strtok(NULL, ",");
		i++;
	}
	
	if (!style) {
		pango = g_strconcat(family, " ", size, NULL);
	} else {
		pango = g_strconcat(family, " ", style, " ", size, NULL);
		g_free(style);
	}
	g_free(d);
	g_free(size);
	g_free(family);
	return pango;
}

/* If the alpha component of 'key'is not 0, colorkey is applied using gdk_pixbuf_add_alpha. If it is 0, no colorkey is applied */
GdkPixbuf *load_bitmap_cached(GMSFStyles *styles, const gchar *name, GMSFColor key) {
	GdkPixbuf *pixbuf;
	gchar *nname;
	gchar *tmp;
	
	tmp = nname = g_ascii_strup(name, -1);
    do {
        if (*tmp == '\\') {
			*tmp = '_';
		}
		
        if (*tmp == '/') {
			*tmp = '_';
		}
		
        if (*tmp == '.') {
			*tmp = '_';
		}
    } while (*tmp++);

	pixbuf = g_hash_table_lookup(styles->priv->image_cache, nname);
	if (pixbuf) {
		g_free(nname);
	} else {
		gpointer extracted;
		gsize sz;
		gboolean free;
		
		extracted = _pe_extract_resource(&styles->priv->ctx, styles->priv->res, nname, NULL, &sz, &free);
		pixbuf = _load_bitmap(extracted, sz);
		
		if (GMSF_COLOR_ALPHA(key)) {
			GdkPixbuf *old;
	
			old = pixbuf;
			pixbuf = gdk_pixbuf_add_alpha(old, TRUE, GMSF_COLOR_RED(key), GMSF_COLOR_GREEN(key), GMSF_COLOR_BLUE(key));
			gdk_pixbuf_unref(old);
		}
		
		g_hash_table_insert(styles->priv->image_cache, nname, pixbuf);
		if (free) {
			g_free(extracted);
		}
	}
	
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
	
	/* globals table */
	str = g_key_file_get_string(styles->priv->selectedini, "globals", "EdgeLightColor", NULL);
	styles->priv->globals.edge_light_color = parse_color(str);
	g_free(str);

	str = g_key_file_get_string(styles->priv->selectedini, "globals", "EdgeHighLightColor", NULL);
	styles->priv->globals.edge_high_light_color = parse_color(str);
	g_free(str);

	str = g_key_file_get_string(styles->priv->selectedini, "globals", "EdgeShadowColor", NULL);
	styles->priv->globals.edge_shadow_color = parse_color(str);
	g_free(str);
	
	str = g_key_file_get_string(styles->priv->selectedini, "globals", "EdgeDkShadowColor", NULL);
	styles->priv->globals.edge_dk_shadow_color = parse_color(str);
	g_free(str);

	str = g_key_file_get_string(styles->priv->selectedini, "globals", "EdgeFillColor", NULL);
	styles->priv->globals.edge_fill_color = parse_color(str);
	g_free(str);
	
	/* sys metrics table */
	styles->priv->sysmetrics.scrollbar_size.width = g_key_file_get_integer(styles->priv->selectedini, "SysMetrics", "ScrollbarWidth", NULL);
	styles->priv->sysmetrics.scrollbar_size.height = g_key_file_get_integer(styles->priv->selectedini, "SysMetrics", "ScrollbarHeight", NULL);

	str = g_key_file_get_string(styles->priv->selectedini, "SysMetrics", "Window", NULL);
	styles->priv->sysmetrics.window = parse_color(str);
	g_free(str);

	str = g_key_file_get_string(styles->priv->selectedini, "SysMetrics", "MenuBar", NULL);
	styles->priv->sysmetrics.menubar = parse_color(str);
	g_free(str);
	
	str = g_key_file_get_string(styles->priv->selectedini, "SysMetrics", "Menu", NULL);
	styles->priv->sysmetrics.menu = parse_color(str);
	g_free(str);

	str = g_key_file_get_string(styles->priv->selectedini, "SysMetrics", "Background", NULL);
	styles->priv->sysmetrics.background = parse_color(str);
	g_free(str);

	str = g_key_file_get_string(styles->priv->selectedini, "SysMetrics", "Btnface", NULL);
	styles->priv->sysmetrics.btnface = parse_color(str);
	g_free(str);

	str = g_key_file_get_string(styles->priv->selectedini, "SysMetrics", "Highlight", NULL);
	styles->priv->sysmetrics.highlight = parse_color(str);
	g_free(str);

	str = g_key_file_get_string(styles->priv->selectedini, "SysMetrics", "ActiveCaption", NULL);
	styles->priv->sysmetrics.activecaption = parse_color(str);
	g_free(str);

	str = g_key_file_get_string(styles->priv->selectedini, "SysMetrics", "CaptionText", NULL);
	styles->priv->sysmetrics.captiontext = parse_color(str);
	g_free(str);

	str = g_key_file_get_string(styles->priv->selectedini, "SysMetrics", "InactiveCaption", NULL);
	styles->priv->sysmetrics.inactivecaption = parse_color(str);
	g_free(str);

	str = g_key_file_get_string(styles->priv->selectedini, "SysMetrics", "InactiveCaptionText", NULL);
	styles->priv->sysmetrics.inactivecaptiontext = parse_color(str);
	g_free(str);
	
	str = g_key_file_get_string(styles->priv->selectedini, "SysMetrics", "GradientActiveCaption", NULL);
	styles->priv->sysmetrics.gradientactivecaption = parse_color(str);
	g_free(str);

	str = g_key_file_get_string(styles->priv->selectedini, "SysMetrics", "GradientInactiveCaption", NULL);
	styles->priv->sysmetrics.gradientinactivecaption = parse_color(str);
	g_free(str);

	str = g_key_file_get_string(styles->priv->selectedini, "SysMetrics", "HighlightText", NULL);
	styles->priv->sysmetrics.highlighttext = parse_color(str);
	g_free(str);

	str = g_key_file_get_string(styles->priv->selectedini, "SysMetrics", "MenuHilight", NULL);
	styles->priv->sysmetrics.menuhilight = parse_color(str);
	g_free(str);

	str = g_key_file_get_string(styles->priv->selectedini, "SysMetrics", "Hottracking", NULL);
	styles->priv->sysmetrics.hottracking = parse_color(str);
	g_free(str);

	str = g_key_file_get_string(styles->priv->selectedini, "SysMetrics", "BtnShadow", NULL);
	styles->priv->sysmetrics.btnshadow = parse_color(str);
	g_free(str);

	str = g_key_file_get_string(styles->priv->selectedini, "SysMetrics", "GrayText", NULL);
	styles->priv->sysmetrics.graytext = parse_color(str);
	g_free(str);
	
	str = g_key_file_get_string(styles->priv->selectedini, "SysMetrics", "BtnHighlight", NULL);
	styles->priv->sysmetrics.btnhighlight = parse_color(str);
	g_free(str);

	str = g_key_file_get_string(styles->priv->selectedini, "SysMetrics", "DkShadow3d", NULL);
	styles->priv->sysmetrics.dkshadow3d = parse_color(str);
	g_free(str);
	
	str = g_key_file_get_string(styles->priv->selectedini, "SysMetrics", "Light3d", NULL);
	styles->priv->sysmetrics.light3d = parse_color(str);
	g_free(str);
	
	styles->priv->sysmetrics.css = g_key_file_get_string(styles->priv->selectedini, "SysMetrics", "CssName", NULL);
	styles->priv->sysmetrics.xml = g_key_file_get_string(styles->priv->selectedini, "SysMetrics", "XmlName", NULL);
	styles->priv->sysmetrics.min_depth = g_key_file_get_integer(styles->priv->selectedini, "SysMetrics", "MinColorDepth", NULL);
	styles->priv->sysmetrics.flat_menus = g_key_file_get_boolean(styles->priv->selectedini, "SysMetrics", "FlatMenus", NULL);

	styles->priv->sysmetrics.captionbarheight = g_key_file_get_integer(styles->priv->selectedini, "SysMetrics", "CaptionBarHeight", NULL);
	styles->priv->sysmetrics.smcaptionbar_size.width = g_key_file_get_integer(styles->priv->selectedini, "SysMetrics", "SMCaptionBarWidth", NULL);
	styles->priv->sysmetrics.smcaptionbar_size.height = g_key_file_get_integer(styles->priv->selectedini, "SysMetrics", "SMCaptionBarHeight", NULL);

	str = g_key_file_get_string(styles->priv->selectedini, "SysMetrics", "CaptionFont", NULL);
	styles->priv->sysmetrics.captionfont = font_to_pango(str);
	g_free(str);
	
	str = g_key_file_get_string(styles->priv->selectedini, "SysMetrics", "SmallCaptionFont", NULL);
	styles->priv->sysmetrics.smallcaptionfont = font_to_pango(str);
	g_free(str);

	str = g_key_file_get_string(styles->priv->selectedini, "SysMetrics", "MenuFont", NULL);
	styles->priv->sysmetrics.menufont = font_to_pango(str);
	g_free(str);

	str = g_key_file_get_string(styles->priv->selectedini, "SysMetrics", "MsgBoxFont", NULL);
	styles->priv->sysmetrics.msgboxfont = font_to_pango(str);
	g_free(str);

	str = g_key_file_get_string(styles->priv->selectedini, "SysMetrics", "IconTitleFont", NULL);
	styles->priv->sysmetrics.icontitlefont = font_to_pango(str);
	g_free(str);

}

static void _finalize(GObject *object) {
	GMSFStyles *styles;

	styles = GMSF_STYLES(object);

	g_free(styles->priv->sysmetrics.css);
	g_free(styles->priv->sysmetrics.xml);
	g_free(styles->priv->sysmetrics.captionfont);
	g_free(styles->priv->sysmetrics.smallcaptionfont);
	g_free(styles->priv->sysmetrics.menufont);
	g_free(styles->priv->sysmetrics.msgboxfont);
	g_free(styles->priv->sysmetrics.icontitlefont);

	g_hash_table_destroy(styles->priv->image_cache);
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
		case PROP_TARGET_DPI:
			styles->priv->target_dpi = g_value_get_uint(value);
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
		case PROP_TARGET_DPI:
			g_value_set_uint(value, styles->priv->target_dpi);
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
    g_object_class_install_property(g_class, PROP_TARGET_DPI, g_param_spec_uint("target-dpi", "nick", "blurb", 0, 96, G_MAXUINT, G_PARAM_READWRITE));

	g_type_class_add_private(klass, sizeof(GMSFStylesPrivate));
}	

static void gmsf_styles_init(GMSFStyles *styles) {
	styles->priv = G_TYPE_INSTANCE_GET_PRIVATE(styles, GMSF_TYPE_STYLES, GMSFStylesPrivate);
	styles->priv->target_dpi = 96;
	styles->priv->selectedini = NULL;
	styles->priv->image_cache = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)gdk_pixbuf_unref);
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

guint gmsf_styles_get_target_dpi(GMSFStyles *styles) {
	return styles->priv->target_dpi;
}

void gmsf_styles_set_target_dpi(GMSFStyles *styles, guint dpi) {
	styles->priv->target_dpi = dpi;
	g_object_notify(G_OBJECT(styles), "target-dpi");
}

void _get_scheme_info(GMSFStyles *styles, const gchar *prefix, const gchar *scheme,  gchar **display_name, gchar **tooltip) {
	gchar *sectionname;
	
	sectionname = g_strconcat(prefix, scheme, NULL);
	if (display_name) {
		*display_name = g_key_file_get_string(styles->priv->themesini, sectionname, "DisplayName", NULL);
	}
	if (tooltip) {
		*tooltip = g_key_file_get_string(styles->priv->themesini, sectionname, "ToolTip", NULL);
	}
	g_free(sectionname);
}


void gmsf_styles_get_color_scheme_info(GMSFStyles *styles, const gchar *scheme,  gchar **display_name, gchar **tooltip) {
	_get_scheme_info(styles, "ColorScheme.", scheme, display_name, tooltip);
}

void gmsf_styles_get_size_scheme_info(GMSFStyles *styles, const gchar *scheme, gchar **display_name, gchar **tooltip) {
	_get_scheme_info(styles, "Size.", scheme, display_name, tooltip);
}

void gmsf_styles_get_documentation(GMSFStyles *styles, GMSFStylesDocumentation *to_fill) {
	to_fill->display_name = g_key_file_get_string(styles->priv->themesini, "documentation", "DisplayName", NULL);
	to_fill->tooltip = g_key_file_get_string(styles->priv->themesini, "documentation", "ToolTip", NULL);
	to_fill->author = g_key_file_get_string(styles->priv->themesini, "documentation", "Author", NULL);
	to_fill->company = g_key_file_get_string(styles->priv->themesini, "documentation", "Company", NULL);
	to_fill->copyright = g_key_file_get_string(styles->priv->themesini, "documentation", "Copyright", NULL);
	to_fill->url = g_key_file_get_string(styles->priv->themesini, "documentation", "URL", NULL);
	to_fill->version = g_key_file_get_string(styles->priv->themesini, "documentation", "Version", NULL);
	to_fill->description = g_key_file_get_string(styles->priv->themesini, "documentation", "Description", NULL);
	to_fill->theme_name = g_key_file_get_string(styles->priv->themesini, "documentation", "ThemeName", NULL);
	to_fill->wmp_skin = g_key_file_get_string(styles->priv->themesini, "documentation", "WmpSkinName", NULL);
	to_fill->last_updated = g_key_file_get_string(styles->priv->themesini, "documentation", "LastUpdated", NULL);
}

void gmsf_styles_documentation_free_fields(GMSFStylesDocumentation *doc) {
	g_free(doc->display_name);
	g_free(doc->tooltip);
	g_free(doc->author);
	g_free(doc->company);
	g_free(doc->copyright);
	g_free(doc->url);
	g_free(doc->version);
	g_free(doc->description);
	g_free(doc->theme_name);
	g_free(doc->wmp_skin);
	g_free(doc->last_updated);
}
