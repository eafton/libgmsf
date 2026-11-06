#include <glib-object.h>
#include <gmsf-export.h>

#ifndef GMSF_H
#define GMSF_H

G_BEGIN_DECLS

#define GMSF_VERSION_MAJOR 0
#define GMSF_VERSION_MINOR 0

typedef guint32 GMSFColor;
#define GMSF_COLOR_PACK(r, g, b, a) ((r << 24) | (g << 16) | (b << 8) | (a))
#define GMSF_COLOR_RED(c) ((c >> 24) & 0xff)
#define GMSF_COLOR_GREEN(c) ((c >> 16) & 0xff )
#define GMSF_COLOR_BLUE(c) ((c >> 8) & 0xff)
#define GMSF_COLOR_ALPHA(c) ((c & 0xff)
#define GMSF_COLOR_FILL_GDK_RGBA_POINTER(g, c) g->red = ((gdouble)GMSF_COLOR_RED(c) / 255.0f); g->green = ((gdouble)GMSF_COLOR_GREEN(c) / 255.0f); g->blue = ((gdouble)GMSF_COLOR_BLUE(c) / 255.0f); g->alpha = ((gdouble)GMSF_COLOR_ALPHA(c) / 255.0f)
#define GMSF_COLOR_FILL_GDK_RGBA(g, c) GMSF_COLOR_FILL_GDK_RGBA_POINTER((&g), c);

#define GMSF_TYPE_STYLES (gmsf_styles_get_type())
#define GMSF_STYLES(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GMSF_TYPE_STYLES, GMSFStyles))
#define GMSF_STYLES_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GMSF_TYPE_STYLES, GMSFStylesClass))
#define GMSF_IS_STYLES(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GMSF_TYPE_STYLES))
#define GMSF_IS_STYLES_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GMSF_TYPE_STYLES))
#define GMSF_STYLES_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), GMSF_TYPE_STYLES, GMSFStylesClass))

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
GMSF_EXPORT const gchar **gmsf_styles_get_color_schemes(GMSFStyles *styles);
GMSF_EXPORT const gchar **gmsf_styles_get_size_schemes(GMSFStyles *styles);
GMSF_EXPORT void gmsf_styles_select_scheme(GMSFStyles *styles, const gchar *color, const gchar *size);

G_END_DECLS

#endif
