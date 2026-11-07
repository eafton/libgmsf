#include <glib-object.h>
#include <gio/gio.h>
#include <gmsf.h>

struct _GMSFRichTextPrivate {
	GInputStream *stream;
};

G_DEFINE_TYPE(GMSFRichText, gmsf_rich_text, G_TYPE_OBJECT)

static void _finalize(GObject *object) {
	GMSFRichText *rtf;

	rtf = GMSF_RICH_TEXT(object);
	g_object_unref(rtf->priv->stream);
    G_OBJECT_CLASS(gmsf_rich_text_parent_class)->finalize(object);
}

static void gmsf_rich_text_class_init(GMSFRichTextClass *klass) {
	GObjectClass *g_class;

	g_class = G_OBJECT_CLASS(klass);
	g_class->finalize = _finalize;
	g_type_class_add_private(klass, sizeof(GMSFRichTextPrivate));
}	

static void gmsf_rich_text_init(GMSFRichText *rtf) {
	rtf->priv = G_TYPE_INSTANCE_GET_PRIVATE(rtf, GMSF_TYPE_RICH_TEXT, GMSFRichTextPrivate);
}

GMSFRichText *gmsf_rich_text_new_from_stream(GInputStream *stream) {
	GMSFRichText *rtf;
	
	rtf = GMSF_RICH_TEXT(g_object_new(GMSF_TYPE_RICH_TEXT, NULL));
	rtf->priv->stream = g_object_ref(stream);
	return rtf;
}

GMSFRichText *gmsf_rich_text_new_from_file(const gchar *file) {
	GMSFRichText *rtf;
	GFile *cfile;
	
	rtf = GMSF_RICH_TEXT(g_object_new(GMSF_TYPE_RICH_TEXT, NULL));
    cfile = g_file_new_for_path(file);
	rtf->priv->stream = G_INPUT_STREAM(g_file_read(cfile, NULL, NULL));
	g_object_unref(cfile);
	return rtf;
}

GMSFRichText *gmsf_rich_text_new_from_data(gpointer data, gsize sz) {
	GMSFRichText *rtf;
	
	rtf = GMSF_RICH_TEXT(g_object_new(GMSF_TYPE_RICH_TEXT, NULL));
	rtf->priv->stream = g_memory_input_stream_new_from_data(data, sz, NULL);
	return rtf;
}

