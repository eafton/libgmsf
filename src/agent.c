#include <glib.h>
#include <glib/gstdio.h>
#include <glib-object.h>
#include <gmsf.h>

typedef struct {
	gchar *name;
	gchar *desc;
	gchar *extra;
} GMSFAgentLocalizedInfo;

typedef struct {
	gchar *name;
	gchar **anims;
} GMSFAgentAnimationState;

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
	gchar *engine_guid;
	gchar *mode_guid;
	gulong speed;
	gushort pitch;
	gushort lang;
	gchar *lang_dialect;
	gushort gender;
	gushort age;
	gchar *style;
	
	/* BALLOONINFO */
	guchar text_lines;
	guchar chars_per_line;
	GMSFColor fg;
	GMSFColor bg;
	GMSFColor border;
	gchar *font;
	glong font_height;
	glong weight;
	
	GMSFColor *palette;
	gulong palette_sz;
	
	gulong tray_icon_offset;
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

static gchar *langid_to_locale(guint16 langid) {
	/* TODO */
	return g_strdup_printf("%u", langid);
}

static void _finalize(GObject *object) {
	GMSFAgent *agent;
	
	agent = GMSF_AGENT(object);

	g_free(agent->priv->filename);
	
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
		agent->priv->engine_guid = read_guid(agent->priv->f);
		agent->priv->mode_guid = read_guid(agent->priv->f);
		fread(&ul, sizeof(guint32), 1, agent->priv->f);
		agent->priv->speed = GUINT32_FROM_LE(ul);
		fread(&us, sizeof(guint16), 1, agent->priv->f);
		agent->priv->pitch = GUINT16_FROM_LE(us);
		fread(&uc, sizeof(guint8), 1, agent->priv->f);

		if (uc) {
			fread(&us, sizeof(guint16), 1, agent->priv->f);
			agent->priv->lang = GUINT16_FROM_LE(us);
			
			agent->priv->lang_dialect = read_string(agent->priv->f);

			fread(&us, sizeof(guint16), 1, agent->priv->f);
			agent->priv->gender = GUINT16_FROM_LE(us);

			fread(&us, sizeof(guint16), 1, agent->priv->f);
			agent->priv->age = GUINT16_FROM_LE(us);

			agent->priv->style = read_string(agent->priv->f);
		}
	}
	
	if (agent->priv->flags & GMSF_AGENT_FLAG_WORD_BALLOON) {
		fread(&agent->priv->text_lines, sizeof(guint8), 1, agent->priv->f);
		fread(&agent->priv->chars_per_line, sizeof(guint8), 1, agent->priv->f);
		
		fread(ucquad, sizeof(guint8), 4, agent->priv->f);
		agent->priv->fg = GMSF_COLOR_PACK(ucquad[0], ucquad[1], ucquad[2], 255);
		fread(ucquad, sizeof(guint8), 4, agent->priv->f);
		agent->priv->bg = GMSF_COLOR_PACK(ucquad[0], ucquad[1], ucquad[2], 255);
		fread(ucquad, sizeof(guint8), 4, agent->priv->f);
		agent->priv->border = GMSF_COLOR_PACK(ucquad[0], ucquad[1], ucquad[2], 255);
				
		agent->priv->font = read_string(agent->priv->f);
		fread(&l, sizeof(gint32), 1, agent->priv->f);
		agent->priv->font_height = GINT32_FROM_LE(l);
		fread(&l, sizeof(gint32), 1, agent->priv->f);
		agent->priv->weight = GINT32_FROM_LE(l);
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
		agent->priv->flags |= GMSF_AGENT_FLAG_TRAY_ICON;
		agent->priv->tray_icon_offset = ftell(agent->priv->f);
		fread(&ul, sizeof(guint32), 1, agent->priv->f);
		fseek(agent->priv->f, GUINT32_FROM_LE(ul), SEEK_CUR);
		fread(&ul, sizeof(guint32), 1, agent->priv->f);
		fseek(agent->priv->f, GUINT32_FROM_LE(ul), SEEK_CUR);
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
		g_hash_table_insert(agent->priv->localized_info, langid_to_locale(us2), info);
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
	
	agent->priv->localized_info = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, free_localized_info);
}

GMSFAgent *gmsf_agent_new_from_file(const gchar *file) {
	return g_object_new(GMSF_TYPE_AGENT, "filename", file, NULL);	
}

