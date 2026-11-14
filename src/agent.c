#include <glib.h>
#include <glib/gstdio.h>
#include <glib-object.h>
#include <gmsf.h>

typedef struct {
	gchar *name;
	gchar *desc;
	gchar *extra;
} GMSFAgentLocalizedInfo;

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

void dump_bits(unsigned val)
{
int i;
  unsigned mask = 1 << (32 - 1);
  char output;

  for(i = 0; i < 32; i++) {
    output = (val & mask) ? '1' : '0';
    printf("%c", output);
    mask = mask >> 1;

    if ((i + 1) % 8 == 0) {
      printf(" ");
    }
  }

  printf("\n");
}

static void _constructed(GObject *object) {
	GMSFAgent *agent;
	guint16 uspair[2];
	guint16 us;
	guint32 locator[2];
	guint32 magic;
	guint32 localized_info_offset;
	guint32 ul, ul2;
	guint8 uc;
	
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
		gchar *engine_guid;
		gchar *mode_guid;
		
		engine_guid = read_guid(agent->priv->f);
		mode_guid = read_guid(agent->priv->f);
		fread(&ul, sizeof(guint32), 1, agent->priv->f);
		ul = GUINT32_FROM_LE(ul);
		fread(&us, sizeof(guint16), 1, agent->priv->f);
		us = GUINT16_FROM_LE(us);
		fread(&uc, sizeof(guint8), 1, agent->priv->f);

		printf("Engine: %s\nMode: %s\nSpeed: %u\nPitch: %u\nExtra: %u\n", engine_guid, mode_guid, ul, us, uc);
	}
	
	/* todo, read from localized_info_offset */
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

