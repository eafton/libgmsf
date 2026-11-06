#include <glib-object.h>
#include <gmsf-export.h>

#ifndef GMSF_H
#define GMSF_H

G_BEGIN_DECLS

#define GMSF_VERSION_MAJOR 0
#define GMSF_VERSION_MINOR 0

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
