#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib-object.h>
#include <gmsf-export.h>

#ifndef GMSF_H
#define GMSF_H

G_BEGIN_DECLS

/* Version */
#define GMSF_VERSION_MAJOR 0
#define GMSF_VERSION_MINOR 0

/* Basic Types */
typedef guint32 GMSFColor;
#define GMSF_COLOR_PACK(r, g, b, a) ((r << 24) | (g << 16) | (b << 8) | (a))
#define GMSF_COLOR_RED(c) ((c >> 24) & 0xff)
#define GMSF_COLOR_GREEN(c) ((c >> 16) & 0xff)
#define GMSF_COLOR_BLUE(c) ((c >> 8) & 0xff)
#define GMSF_COLOR_ALPHA(c) (c & 0xff)
#define GMSF_COLOR_FILL_GDK_RGBA_POINTER(g, c) (g)->red = ((gdouble)GMSF_COLOR_RED(c) / 255.0f); (g)->green = ((gdouble)GMSF_COLOR_GREEN(c) / 255.0f); (g)->blue = ((gdouble)GMSF_COLOR_BLUE(c) / 255.0f); (g)->alpha = ((gdouble)GMSF_COLOR_ALPHA(c) / 255.0f);
#define GMSF_COLOR_FILL_GDK_RGBA(g, c) GMSF_COLOR_FILL_GDK_RGBA_POINTER(&g, c);

typedef struct _GMSFSize {
    gint width;
    gint height;
} GMSFSize;

typedef struct _GMSFRectangle {
    gint x;
    gint y;
    gint width;
    gint height;
} GMSFRectangle;

/* Styles */
#define GMSF_TYPE_STYLES (gmsf_styles_get_type())
#define GMSF_STYLES(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GMSF_TYPE_STYLES, GMSFStyles))
#define GMSF_STYLES_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GMSF_TYPE_STYLES, GMSFStylesClass))
#define GMSF_IS_STYLES(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GMSF_TYPE_STYLES))
#define GMSF_IS_STYLES_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GMSF_TYPE_STYLES))
#define GMSF_STYLES_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), GMSF_TYPE_STYLES, GMSFStylesClass))

typedef enum {
    GMSF_THEME_PART_IMAGE_BACKGROUND,
    GMSF_THEME_PART_IMAGE_GLYPH
} GMSFThemePartImage;

typedef enum {
    GMSF_THEME_PART_STATE_NONE = 0x0,
    GMSF_THEME_PART_STATE_HILITE = 0x1,
    GMSF_THEME_PART_STATE_ACTIVE = 0x2,
    GMSF_THEME_PART_STATE_DEFAULT = 0x4,
    GMSF_THEME_PART_STATE_DISABLED = 0x8,
    GMSF_THEME_PART_STATE_UNCHECKED = 0x10,
    GMSF_THEME_PART_STATE_CHECKED = 0x20,
    GMSF_THEME_PART_STATE_MIXED = 0x40
} GMSFThemePartState;

typedef enum {
    GMSF_THEME_IMAGE_TYPE_BORDER_FILL,
    GMSF_THEME_IMAGE_TYPE_IMAGE_FILE
} GMSFThemeImageType;

typedef enum {
    GMSF_THEME_SIZING_TYPE_TRUE_SIZE,
    GMSF_THEME_SIZING_TYPE_STRETCH,
    GMSF_THEME_SIZING_TYPE_TILE
} GMSFThemeSizingType;

typedef enum {
    GMSF_THEME_FILL_TYPE_SOLID,
    GMSF_THEME_FILL_TYPE_HORIZONTAL_GRADIENT,
    GMSF_THEME_FILL_TYPE_VERTICAL_GRADIENT
} GMSFThemeFillType;

typedef struct _GMSFThemePartInfo {
    GMSFThemeImageType bg_type;
    GMSFThemeSizingType sizing_type;

    /* Metrics */
    GMSFSize min_size;
    GMSFSize normal_size;
    gint border_size;
    GMSFRectangle sizing_margins;
    GMSFRectangle content_margins;

    /* Color/Fill */
    GMSFThemeFillType fill_type;
    GMSFColor fill_color;
    GMSFColor border_color;
    GMSFColor text_color;
    gint gradient_n_stops;
    GMSFColor *gradient_stops;

    /* Images */
    GdkPixbuf *bg_image;
    GdkPixbuf *glyph_image;
} GMSFThemePartInfo;

typedef struct {
	gchar *display_name;
	gchar *last_updated;
	gchar *tooltip;
	gchar *author;
	gchar *company;
	gchar *copyright;
	gchar *url;
	gchar *version;
	gchar *description;
	gchar *theme_name;
	gchar *wmp_skin;	
} GMSFStylesDocumentation;

typedef struct _GMSFStyles GMSFStyles;
typedef struct _GMSFStylesClass GMSFStylesClass;
typedef struct _GMSFStylesPrivate GMSFStylesPrivate;

struct _GMSFStyles {
	GObject parent_instance;
	GMSFStylesPrivate *priv;
};

struct _GMSFStylesClass {
	GObjectClass parent_class;
};

GMSF_EXPORT GType gmsf_styles_get_type(void) G_GNUC_CONST;
GMSF_EXPORT GMSFStyles *gmsf_styles_new_from_file(const gchar *filename);
GMSF_EXPORT guint gmsf_styles_get_version(GMSFStyles *styles);
GMSF_EXPORT guint gmsf_styles_get_target_dpi(GMSFStyles *styles);
GMSF_EXPORT void gmsf_styles_set_target_dpi(GMSFStyles *styles, guint dpi);
GMSF_EXPORT void gmsf_styles_get_part_info(GMSFStyles *styles, const gchar *part, GMSFThemePartState state, GMSFThemePartInfo *info);
GMSF_EXPORT const gchar **gmsf_styles_get_color_schemes(GMSFStyles *styles);
GMSF_EXPORT const gchar **gmsf_styles_get_size_schemes(GMSFStyles *styles);
GMSF_EXPORT void gmsf_styles_get_color_scheme_info(GMSFStyles *styles, const gchar *scheme, gchar **display_name, gchar **tooltip);
GMSF_EXPORT void gmsf_styles_get_size_scheme_info(GMSFStyles *styles, const gchar *scheme, gchar **display_name, gchar **tooltip);
GMSF_EXPORT void gmsf_styles_select_scheme(GMSFStyles *styles, const gchar *color, const gchar *size);
GMSF_EXPORT void gmsf_styles_get_documentation(GMSFStyles *styles, GMSFStylesDocumentation *to_fill);
GMSF_EXPORT void gmsf_styles_documentation_free_fields(GMSFStylesDocumentation *doc);

/* RTF */
#define GMSF_TYPE_RICH_TEXT (gmsf_rich_text_get_type())
#define GMSF_RICH_TEXT(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GMSF_TYPE_RICH_TEXT, GMSFRichText))
#define GMSF_RICH_TEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GMSF_TYPE_RICH_TEXT, GMSFRichTextClass))
#define GMSF_IS_RICH_TEXT(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GMSF_TYPE_RICH_TEXT))
#define GMSF_IS_RICH_TEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GMSF_TYPE_RICH_TEXT))
#define GMSF_RICH_TEXT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), GMSF_TYPE_RICH_TEXT, GMSFRichTextClass))

typedef struct _GMSFRichText GMSFRichText;
typedef struct _GMSFRichTextClass GMSFRichTextClass;
typedef struct _GMSFRichTextPrivate GMSFRichTextPrivate;

struct _GMSFRichText{
	GObject parent_instance;
	GMSFRichTextPrivate *priv;
};

struct _GMSFRichTextClass {
	GObjectClass parent_class;
};

GMSF_EXPORT GType gmsf_rich_text_get_type(void) G_GNUC_CONST;
GMSF_EXPORT GMSFRichText *gmsf_rich_text_new_from_fd(gint fd);
GMSF_EXPORT GMSFRichText *gmsf_rich_text_new_from_file(const gchar *file);
GMSF_EXPORT GMSFRichText *gmsf_rich_text_new_from_data(gpointer data, gsize sz);

G_END_DECLS

#endif
