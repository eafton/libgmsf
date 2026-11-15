#include <glib.h>
#include <glib/gstdio.h>
#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gmsf.h>
#include "util.h"

struct _GMSFAgentPrivate {
	FILE *f;
	gchar *filename;
	
	GMSFAgentFlags flags;
	gushort animset_version_major, animset_version_minor;
	gushort version_major, version_minor;
	gushort width, height;
	guchar transparent_color_index;
	gchar *guid;
	GHashTable *localized_info;
	
	/* VOICEINFO */
	GMSFAgentVoiceInfo vi;
	
	/* BALLOONINFO */
	GMSFAgentBalloonInfo bi;
	
	GMSFColor *palette;
	gulong palette_sz;
	
	GdkPixbuf *tray_icon;
	
	gulong anim_info_locator[2];
	gulong img_info_locator[2];
	gulong aud_info_locator[2];
	
	GMSFAgentAnimationState *states;
	gushort states_sz;
};

enum {
	PROP_FILENAME = 1,
};

G_DEFINE_TYPE(GMSFAgent, gmsf_agent, G_TYPE_OBJECT)

GdkPixbuf *apply_alpha_mask(GdkPixbuf *src, GdkPixbuf *mask) {
	GdkPixbuf *ret;
	guchar *pixels, *mask_pixels;
	int width, height, rowstride, n_channels, mask_rowstride, mask_n_channels, y ,x;

	ret = gdk_pixbuf_add_alpha(src, FALSE, 0, 0, 0);
	width = gdk_pixbuf_get_width(ret);
	height = gdk_pixbuf_get_height(ret);
	rowstride = gdk_pixbuf_get_rowstride(ret);
	n_channels = gdk_pixbuf_get_n_channels(ret);
    pixels = gdk_pixbuf_get_pixels(ret);

    mask_rowstride = gdk_pixbuf_get_rowstride(mask);
    mask_n_channels = gdk_pixbuf_get_n_channels(mask);
    mask_pixels = gdk_pixbuf_get_pixels(mask);

    for (y = 0; y < height; y++) {
        guchar *dest_line, *mask_line;
      
        dest_line = pixels + y * rowstride;
        mask_line = mask_pixels + y * mask_rowstride;
        for (x = 0; x < width; x++) {
            guchar *dest_p, *mask_p;
            guint alpha_val;
            
			dest_p = dest_line + x * n_channels;
			mask_p = mask_line + x * mask_n_channels;

            if (mask_n_channels >= 4) {
                alpha_val = mask_p[3]; 
            } else if (mask_n_channels == 1) {
				alpha_val = *mask_p;
			} else {
                alpha_val = (mask_p[0] + mask_p[1] + mask_p[2]) / 3;
            }
			
            dest_p[3] = 255 - alpha_val;
        }
    }

	return ret;
}


static void free_localized_info(gpointer data) {
	GMSFAgentLocalizedInfo *info;
	
	info = data;
	g_free(info->name);
	g_free(info->desc);
	g_free(info->extra);
	g_free(info);
}

static gchar *read_string(FILE *handle) {
	gchar *ret;
	guint16 *str;
	guint32 sz, i;

	fread(&sz, sizeof(guint32), 1, handle);
	sz = GUINT32_FROM_LE(sz);
	if (!sz) {
		return NULL;
	}
	
	str = g_new(guint16, sz + 1);
	fread(str, sizeof(guint16), sz + 1, handle); 
	for (i = 0; i < sz; i++) {
		str[i] = GUINT16_FROM_LE(str[i]);
	}
	ret = g_utf16_to_utf8(str, sz, NULL, NULL, NULL);
	g_free(str);
	return ret;
}

static gchar *read_guid(FILE *handle) {
	guint32 guid_data1;
	guint16 guid_data23[2];
	guint8 guid_data4[8];

	fread(&guid_data1, sizeof(guint32), 1, handle);
	guid_data1 = GUINT32_FROM_LE(guid_data1);
	fread(guid_data23, sizeof(guint16), 2, handle);
	guid_data23[0] = GUINT16_FROM_LE(guid_data23[0]);
	guid_data23[1] = GUINT16_FROM_LE(guid_data23[1]);
	fread(guid_data4, sizeof(guint8), 8, handle);
	return g_strdup_printf("{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}", guid_data1, guid_data23[0], guid_data23[1], guid_data4[0], guid_data4[1], guid_data4[2], guid_data4[3], guid_data4[4], guid_data4[5], guid_data4[6], guid_data4[7]);
}

static void _finalize(GObject *object) {
	GMSFAgent *agent;
	
	agent = GMSF_AGENT(object);

	g_free(agent->priv->filename);
	gdk_pixbuf_unref(agent->priv->tray_icon);

    G_OBJECT_CLASS(gmsf_agent_parent_class)->finalize(object);
}

static void _set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) {
	GMSFAgent *agent;
	
	agent = GMSF_AGENT(object);
	switch (prop_id) {
		case PROP_FILENAME:
			agent->priv->filename = g_value_dup_string(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}

static void _get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) {
	GMSFAgent *agent;
	
	agent = GMSF_AGENT(object);
	switch (prop_id) {
		case PROP_FILENAME:
			g_value_set_string(value, agent->priv->filename);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}

static void _constructed(GObject *object) {
	GMSFAgent *agent;
	guint16 uspair[2];
	guint16 us;
	guint32 locator[2];
	guint32 magic;
	guint32 localized_info_offset;
	guint32 ul, ul2;
	gint32 l;
	guint8 ucquad[4];
	guint8 uc;
	gulong i;
	
	agent = GMSF_AGENT(object);
    G_OBJECT_CLASS(gmsf_agent_parent_class)->constructed(object);
	
	agent->priv->f = g_fopen(agent->priv->filename, "rb");
	
	/* ACSHEADER */
	fread(&magic, sizeof(guint32), 1, agent->priv->f);
	magic = GUINT32_FROM_LE(magic);
	if (magic != 0xABCDABC3) {
		/* error */
	}
	
	fread(locator, sizeof(guint32), 2, agent->priv->f);
	locator[0] = GUINT32_FROM_LE(locator[0]);
	locator[1] = GUINT32_FROM_LE(locator[1]);
	
	fread(agent->priv->anim_info_locator, sizeof(guint32), 2, agent->priv->f);
	agent->priv->anim_info_locator[0] = GUINT32_FROM_LE(agent->priv->anim_info_locator[0]);
	agent->priv->anim_info_locator[1] = GUINT32_FROM_LE(agent->priv->anim_info_locator[1]);

	fread(agent->priv->img_info_locator, sizeof(guint32), 2, agent->priv->f);
	agent->priv->img_info_locator[0] = GUINT32_FROM_LE(agent->priv->img_info_locator[0]);
	agent->priv->img_info_locator[1] = GUINT32_FROM_LE(agent->priv->img_info_locator[1]);

	fread(agent->priv->aud_info_locator, sizeof(guint32), 2, agent->priv->f);
	agent->priv->aud_info_locator[0] = GUINT32_FROM_LE(agent->priv->aud_info_locator[0]);
	agent->priv->aud_info_locator[1] = GUINT32_FROM_LE(agent->priv->aud_info_locator[1]);

	/* ACSCHARACTERINFO */
	fseek(agent->priv->f, locator[0], SEEK_SET);
	fread(uspair, sizeof(guint16), 2, agent->priv->f);
	agent->priv->version_major = GUINT16_FROM_LE(uspair[0]);
	agent->priv->version_minor = GUINT16_FROM_LE(uspair[1]);

	fread(locator, sizeof(guint32), 2, agent->priv->f);
	localized_info_offset = locator[0] = GUINT32_FROM_LE(locator[0]);
	locator[1] = GUINT32_FROM_LE(locator[1]);
	
	agent->priv->guid = read_guid(agent->priv->f);
	
	fread(uspair, sizeof(guint16), 2, agent->priv->f);
	agent->priv->width = GUINT16_FROM_LE(uspair[0]);
	agent->priv->height = GUINT16_FROM_LE(uspair[1]);

	fread(&uc, sizeof(guint8), 1, agent->priv->f);
	agent->priv->transparent_color_index = uc;
	
	agent->priv->flags = GMSF_AGENT_FLAGS_NONE;
	fread(&ul, sizeof(guint32), 1, agent->priv->f);
	ul = GUINT32_FROM_LE(ul);
	if ((ul >> 5) & 1) {
		agent->priv->flags |= GMSF_AGENT_FLAG_VOICE_OUTPUT;
	}
	if (((ul >> 9) & 1) && !((ul >> 8) & 1)) {
		agent->priv->flags |= GMSF_AGENT_FLAG_WORD_BALLOON;
	}
	ul2 = (ul >> 16) & 0x07;
	if (ul2 & 0x01) {
		agent->priv->flags |= GMSF_AGENT_FLAG_WORD_BALLOON_SIZE_TO_TEXT;
	}
	if (ul2 & 0x02) {
		agent->priv->flags |= GMSF_AGENT_FLAG_WORD_BALLOON_NO_AUTO_HIDE;
	}
	if (ul2 & 0x04) {
		agent->priv->flags |= GMSF_AGENT_FLAG_WORD_BALLOON_NO_AUTO_PACE;
	}
	if ((ul >> 20) & 1) {
		agent->priv->flags |= GMSF_AGENT_FLAG_STANDARD_ANIMATION_SET;
	}

	fread(uspair, sizeof(guint16), 2, agent->priv->f);
	agent->priv->animset_version_major = GUINT16_FROM_LE(uspair[0]);
	agent->priv->animset_version_minor = GUINT16_FROM_LE(uspair[1]);

	if (agent->priv->flags & GMSF_AGENT_FLAG_VOICE_OUTPUT) {
		agent->priv->vi.engine_guid = read_guid(agent->priv->f);
		agent->priv->vi.mode_guid = read_guid(agent->priv->f);
		fread(&ul, sizeof(guint32), 1, agent->priv->f);
		agent->priv->vi.speed = GUINT32_FROM_LE(ul);
		fread(&us, sizeof(guint16), 1, agent->priv->f);
		agent->priv->vi.pitch = GUINT16_FROM_LE(us);
		fread(&uc, sizeof(guint8), 1, agent->priv->f);

		if (uc) {
			fread(&us, sizeof(guint16), 1, agent->priv->f);
			agent->priv->vi.lang = GUINT16_FROM_LE(us);
			
			agent->priv->vi.lang_dialect = read_string(agent->priv->f);

			fread(&us, sizeof(guint16), 1, agent->priv->f);
			agent->priv->vi.gender = GUINT16_FROM_LE(us);

			fread(&us, sizeof(guint16), 1, agent->priv->f);
			agent->priv->vi.age = GUINT16_FROM_LE(us);

			agent->priv->vi.style = read_string(agent->priv->f);
		}
	}
	
	if (agent->priv->flags & GMSF_AGENT_FLAG_WORD_BALLOON) {
		fread(&agent->priv->bi.text_lines, sizeof(guint8), 1, agent->priv->f);
		fread(&agent->priv->bi.chars_per_line, sizeof(guint8), 1, agent->priv->f);
		
		fread(ucquad, sizeof(guint8), 4, agent->priv->f);
		agent->priv->bi.fg = GMSF_COLOR_PACK(ucquad[0], ucquad[1], ucquad[2], 255);
		fread(ucquad, sizeof(guint8), 4, agent->priv->f);
		agent->priv->bi.bg = GMSF_COLOR_PACK(ucquad[0], ucquad[1], ucquad[2], 255);
		fread(ucquad, sizeof(guint8), 4, agent->priv->f);
		agent->priv->bi.border = GMSF_COLOR_PACK(ucquad[0], ucquad[1], ucquad[2], 255);
				
		agent->priv->bi.font = read_string(agent->priv->f);
		fread(&l, sizeof(gint32), 1, agent->priv->f);
		agent->priv->bi.font_height = GINT32_FROM_LE(l);
		fread(&l, sizeof(gint32), 1, agent->priv->f);
		agent->priv->bi.weight = GINT32_FROM_LE(l);
		fread(&uc, sizeof(guint8), 1, agent->priv->f);
		if (uc) {
			agent->priv->flags |= GMSF_AGENT_FLAG_WORD_BALLOON_ITALICIZE;
		}
		fread(&uc, sizeof(guint8), 1, agent->priv->f);
	}
	
	fread(&ul, sizeof(guint32), 1, agent->priv->f);
	agent->priv->palette_sz = GUINT32_FROM_LE(ul);
	agent->priv->palette = g_new(GMSFColor, agent->priv->palette_sz);
	for (i = 0; i < agent->priv->palette_sz; i++) {
		fread(ucquad, sizeof(guint8), 4, agent->priv->f);
		agent->priv->palette[i] = GMSF_COLOR_PACK(ucquad[0], ucquad[1], ucquad[2], 255);
	}
	
	fread(&uc, sizeof(guint8), 1, agent->priv->f);
	if (uc) {
		GdkPixbuf *mono, *color;
		gpointer dib;
		gsize sz;
		
		agent->priv->flags |= GMSF_AGENT_FLAG_TRAY_ICON;
		
		fread(&ul, sizeof(guint32), 1, agent->priv->f);
		sz = GUINT32_FROM_LE(ul);
		dib = g_malloc(sz);
		fread(dib, 1, sz, agent->priv->f);
		mono = _load_bitmap(dib, sz);
		g_free(dib);
		
		fread(&ul, sizeof(guint32), 1, agent->priv->f);
		sz = GUINT32_FROM_LE(ul);
		dib = g_malloc(sz);
		fread(dib, 1, sz, agent->priv->f);
		color = _load_bitmap(dib, sz);
		g_free(dib);
		
		agent->priv->tray_icon = apply_alpha_mask(color, mono);
	
		gdk_pixbuf_unref(color);
		gdk_pixbuf_unref(mono);
	}

	fread(&us, sizeof(guint16), 1, agent->priv->f);
	agent->priv->states_sz = GUINT16_FROM_LE(us);
	agent->priv->states = g_new(GMSFAgentAnimationState, agent->priv->states_sz);
	for (i = 0; i < agent->priv->states_sz; i++) {
		guint16 j;

		agent->priv->states[i].name = read_string(agent->priv->f);
		
		fread(&us, sizeof(guint16), 1, agent->priv->f);
		us = GUINT16_FROM_LE(us);
		agent->priv->states[i].anims = g_new(gchar *, us + 1);
		for (j = 0; j < us; j++) {
			agent->priv->states[i].anims[j] = read_string(agent->priv->f);
		}
		agent->priv->states[i].anims[us] = NULL;
	}

	fread(&us, sizeof(guint16), 1, agent->priv->f);
	us = GUINT16_FROM_LE(us);
	for (i = 0; i < us; i++) {
		GMSFAgentLocalizedInfo *info;
		guint16 us2;
		
		fread(&us2, sizeof(guint16), 1, agent->priv->f);
		us2 = GUINT16_FROM_LE(us2);
		
		info = g_new(GMSFAgentLocalizedInfo, 1);
		info->name = read_string(agent->priv->f);
		info->desc = read_string(agent->priv->f);
		info->extra = read_string(agent->priv->f);
		g_hash_table_insert(agent->priv->localized_info, GUINT_TO_POINTER(us2), info);
	}
}

static void gmsf_agent_class_init(GMSFAgentClass *klass) {
	GObjectClass *g_class;

	g_class = G_OBJECT_CLASS(klass);
	
	g_class->finalize = _finalize;
	g_class->get_property = _get_property;
	g_class->set_property = _set_property;
	g_class->constructed = _constructed;

    g_object_class_install_property(g_class, PROP_FILENAME, g_param_spec_string("filename", "nick", "blurb", NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_type_class_add_private(klass, sizeof(GMSFAgentPrivate));
}	

static void gmsf_agent_init(GMSFAgent *agent) {
	agent->priv = G_TYPE_INSTANCE_GET_PRIVATE(agent, GMSF_TYPE_AGENT, GMSFAgentPrivate);
	
	agent->priv->localized_info = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, free_localized_info);
	agent->priv->tray_icon = NULL;
}

GMSFAgent *gmsf_agent_new_from_file(const gchar *file) {
	return g_object_new(GMSF_TYPE_AGENT, "filename", file, NULL);	
}


GMSFAgentFlags gmsf_agent_get_flags(GMSFAgent *agent) {
	return agent->priv->flags;
}

void gmsf_agent_get_animation_set_version(GMSFAgent *agent, gushort *maj, gushort *min) {
	if (maj) *maj = agent->priv->animset_version_major;
	if (min) *min = agent->priv->animset_version_minor;
}

void gmsf_agent_get_version(GMSFAgent *agent, gushort *maj, gushort *min) {
	if (maj) *maj = agent->priv->version_major;
	if (min) *min = agent->priv->version_minor;
}

void gmsf_agent_get_size(GMSFAgent *agent, gushort *width, gushort *height) {
	if (width) *width = agent->priv->width;
	if (height) *height = agent->priv->height;
}

const gchar *gmsf_agent_get_guid(GMSFAgent *agent) {
	return agent->priv->guid;
}

GMSFAgentLocalizedInfo *gmsf_agent_get_localized_info(GMSFAgent *agent, gushort langid) {
	return (GMSFAgentLocalizedInfo *)g_hash_table_lookup(agent->priv->localized_info, GUINT_TO_POINTER(langid));
}

gushort *gmsf_agent_get_localized_info_languages(GMSFAgent *agent, gsize *sz) {
	gushort *ret;
	gpointer *data;
	guint csz, i;
	
	data = g_hash_table_get_keys_as_array(agent->priv->localized_info, &csz);
	if (sz) {
		*sz = csz;
	}
	
	ret = g_new(gushort, csz);
	for (i = 0; i < csz; i++) {
		ret[i] = GPOINTER_TO_UINT(data[i]);
	}
	g_free(data);
	
	return ret;
}

GMSFAgentVoiceInfo *gmsf_agent_get_voice_info(GMSFAgent *agent) {
	if (agent->priv->flags & GMSF_AGENT_FLAG_VOICE_OUTPUT) {
		return &agent->priv->vi;
	}

	return NULL;
}

GMSFAgentBalloonInfo *gmsf_agent_get_balloon_info(GMSFAgent *agent) {
	if (agent->priv->flags & GMSF_AGENT_FLAG_VOICE_OUTPUT) {
		return &agent->priv->bi;
	}
	
	return NULL;
}

GMSFAgentAnimationState *gmsf_agent_get_animation_states(GMSFAgent *agent, gsize *sz) {
	if (sz) {
		*sz = agent->priv->states_sz;
	}
	
	return agent->priv->states;
}

GdkPixbuf *gmsf_agent_get_tray_icon(GMSFAgent *agent) {
	return agent->priv->tray_icon;
}


