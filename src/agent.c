#include <glib.h>
#include <glib/gstdio.h>
#include <glib-object.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <sndfile.h>
#include <gmsf.h>
#include "util.h"

/* TODO: 
 * -Properites
 * -Image regions
 * -Animations */
 
typedef struct {
	GMSFAgentPrivate *priv;
	gsize sz, offset;
} GMSFAgentAudio;

typedef struct {
    const guint8 *data;
    gsize byte_pos;
    guint8 bit_pos; 
} GMSFDecompressorState;

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
	GHashTable *img_locators;
	GHashTable *img_cache;

	gulong aud_info_locator[2];
	gulong audios_sz;
	GMSFAgentAudio *audios;
	SF_VIRTUAL_IO audio_io;
	
	GMSFAgentAnimationState *states;
	gushort states_sz;
};

enum {
	PROP_FILENAME = 1,
};

G_DEFINE_TYPE(GMSFAgent, gmsf_agent, G_TYPE_OBJECT)

static guint32 decompressor_read_bits(GMSFDecompressorState *reader, guint count) {
    guint32 value;
    guint i;
    
    value = 0;
    for (i = 0; i < count; i++) {
        guint8 bit;
        
        bit = (reader->data[reader->byte_pos] >> reader->bit_pos) & 1;
        value |= ((guint32)bit << i);

        reader->bit_pos++;
        if (reader->bit_pos == 8) {
            reader->bit_pos = 0;
            reader->byte_pos++;
        }
    }
    return value;
}

static void decompressor(gpointer in, gpointer out) {
    GMSFDecompressorState reader;
    const guint8 *src;
    guint8 *dst;
    gsize out_pos;

    g_return_if_fail(in != NULL);
    g_return_if_fail(out != NULL);

    src = (const guint8 *)in;
    dst = (guint8 *)out;
    g_return_if_fail(src[0] == 0x00);

    reader.data = src;
    reader.byte_pos = 1;
    reader.bit_pos = 0;
    out_pos = 0;

    while (TRUE) {
        guint32 type_bit;

        type_bit = decompressor_read_bits(&reader, 1);
        if (!type_bit) {
            dst[out_pos++] = (guint8)decompressor_read_bits(&reader, 8);
        } else {
            guint32 count, offset_add, raw_offset, offset, i;
            guint len_ones, offset_bits, seq_ones;
            gsize copy_src;

            count = 2;
            seq_ones = 0;
            while (seq_ones < 3) {
                if (decompressor_read_bits(&reader, 1) == 1) {
                    seq_ones++;
                } else {
                    break;
                }
            }

            offset_bits = 0;
            offset_add = 0;
            switch (seq_ones) {
                case 0:
                    offset_bits = 6;
                    offset_add = 0x0001;
                    break;
                case 1:
                    offset_bits = 9;
                    offset_add = 0x0041;
                    break;
                case 2:
                    offset_bits = 12;
                    offset_add = 0x0241;
                    break;
                case 3:
                    offset_bits = 20;
                    offset_add = 0x1241;
                    break;
            }

            raw_offset = decompressor_read_bits(&reader, offset_bits);
            if (offset_bits == 20) {
                if (raw_offset == 0x000FFFFF) {
                    break;
                }
                count += 1;
            }

            offset = raw_offset + offset_add;
            len_ones = 0;
            while (len_ones < 11) {
                if (decompressor_read_bits(&reader, 1) == 1) {
                    len_ones++;
                } else {
                    break;
                }
            }

            if (len_ones == 11) {
                if (decompressor_read_bits(&reader, 1) == 1) {
                    return;
                }
            }

            if (len_ones > 0) {
                count += (1U << len_ones) - 1;
                count += decompressor_read_bits(&reader, len_ones);
            }

            g_return_if_fail(offset <= out_pos);
            copy_src = out_pos - offset;
            for (i = 0; i < count; i++) {
                dst[out_pos++] = dst[copy_src++];
            }
        }
    }
}

static sf_count_t vio_get_len(void *udata) {
	GMSFAgentAudio *audio;
	
	audio = udata;
	return (sf_count_t)audio->sz;
}

static sf_count_t vio_seek(sf_count_t offset, int whence, void *udata) {
	GMSFAgentAudio *audio;
	sf_count_t rel_pos;

	audio = udata;
	switch (whence) {
		case SEEK_CUR:
			rel_pos = (ftell(audio->priv->f) - (long)audio->offset) + offset;
			break;
		case SEEK_END:
			rel_pos = (sf_count_t)audio->sz + offset;
			break;
		default:
			rel_pos = offset;
			break;
	}

	if (rel_pos < 0) {
		rel_pos = 0;
	} else if (rel_pos > (sf_count_t)audio->sz) {
		rel_pos = (sf_count_t)audio->sz;
	}

	fseek(audio->priv->f, (long)(audio->offset + rel_pos), SEEK_SET);
	return rel_pos;
}

static sf_count_t vio_read(void *ptr, sf_count_t count, void *udata) {
	GMSFAgentAudio *audio;
	sf_count_t rel_pos, remaining;
	
	audio = udata;
	rel_pos = ftell(audio->priv->f) - (long)audio->offset;
	remaining = (sf_count_t)audio->sz - rel_pos;

	if (remaining <= 0) {
		return 0;
	}
	
	if (count > remaining) {
		count = remaining;
	}

	return (sf_count_t)fread(ptr, 1, (size_t)count, audio->priv->f);
}

static sf_count_t vio_tell(void *udata) {
	GMSFAgentAudio *audio;
	
	audio = udata;
	return (sf_count_t)(ftell(audio->priv->f) - (long)audio->offset);
}

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

	fclose(agent->priv->f);
	g_free(agent->priv->filename);
	g_free(agent->priv->guid);
	g_hash_table_destroy(agent->priv->localized_info);
	
	if (agent->priv->flags & GMSF_AGENT_FLAG_VOICE_OUTPUT) {
		g_free(agent->priv->vi.engine_guid);
		g_free(agent->priv->vi.mode_guid);
		if (agent->priv->flags & GMSF_AGENT_FLAG_EXTRA_SPEECH_DATA) {
			g_free(agent->priv->vi.lang_dialect);
			g_free(agent->priv->vi.style);
		}		
	}
	
	if (agent->priv->flags & GMSF_AGENT_FLAG_WORD_BALLOON) {
		g_free(agent->priv->bi.font);
	}
	
	g_free(agent->priv->palette);
		
	if (agent->priv->tray_icon) {
		gdk_pixbuf_unref(agent->priv->tray_icon);
	}
	
	g_hash_table_destroy(agent->priv->img_locators);
	g_hash_table_destroy(agent->priv->img_cache);

	g_free(agent->priv->audios);

	g_free(agent->priv->states);

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

	fseek(agent->priv->f, localized_info_offset, SEEK_SET);
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
	
	fseek(agent->priv->f, agent->priv->aud_info_locator[0], SEEK_SET);
	fread(&ul, sizeof(guint32), 1, agent->priv->f);
	agent->priv->audios_sz = GUINT32_FROM_LE(ul);
	agent->priv->audios = g_new(GMSFAgentAudio, agent->priv->audios_sz);
	for (i = 0; i < agent->priv->audios_sz; i++) {
		fread(locator, sizeof(guint32), 2, agent->priv->f);
		fread(&ul, sizeof(guint32), 1, agent->priv->f);
		
		agent->priv->audios[i].priv = agent->priv;
		agent->priv->audios[i].offset = GUINT32_FROM_LE(locator[0]);
		agent->priv->audios[i].sz = GUINT32_FROM_LE(locator[1]);
	}
	
	fseek(agent->priv->f, agent->priv->img_info_locator[0], SEEK_SET);
	fread(&ul, sizeof(guint32), 1, agent->priv->f);
	ul = GUINT32_FROM_LE(ul);
	for (i = 0; i < ul; i++) {
		guint32 *lctr;
		
		lctr = g_new(guint32, 2);
		fread(lctr, sizeof(guint32), 2, agent->priv->f);
		fread(&ul2, sizeof(guint32), 1, agent->priv->f);
		lctr[0] = GUINT32_FROM_LE(lctr[0]);
		lctr[1] = GUINT32_FROM_LE(lctr[1]);

		g_hash_table_insert(agent->priv->img_locators, GUINT_TO_POINTER(i), lctr);
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
	
	agent->priv->img_locators = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);
	agent->priv->img_cache = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, (GDestroyNotify)gdk_pixbuf_unref);
	
	agent->priv->audio_io.get_filelen = vio_get_len;
	agent->priv->audio_io.seek = vio_seek;
	agent->priv->audio_io.read = vio_read;
	agent->priv->audio_io.tell = vio_tell;
	agent->priv->audio_io.write = NULL;
}

GMSFAgent *gmsf_agent_new_from_file(const gchar *file) {
	return g_object_new(GMSF_TYPE_AGENT, "filename", file, NULL);	
}


GMSFAgentFlags gmsf_agent_get_flags(GMSFAgent *agent) {
	g_return_val_if_fail(agent, GMSF_AGENT_FLAGS_NONE);
	return agent->priv->flags;
}

void gmsf_agent_get_animation_set_version(GMSFAgent *agent, gushort *maj, gushort *min) {
	g_return_if_fail(agent);
	if (maj) *maj = agent->priv->animset_version_major;
	if (min) *min = agent->priv->animset_version_minor;
}

void gmsf_agent_get_version(GMSFAgent *agent, gushort *maj, gushort *min) {
	g_return_if_fail(agent);
	if (maj) *maj = agent->priv->version_major;
	if (min) *min = agent->priv->version_minor;
}

void gmsf_agent_get_size(GMSFAgent *agent, GMSFSize *size) {
	g_return_if_fail(agent);
	if (size) {
		size->width = agent->priv->width;
		size->height = agent->priv->height;
	}
}

const gchar *gmsf_agent_get_guid(GMSFAgent *agent) {
	g_return_val_if_fail(agent, NULL);
	return agent->priv->guid;
}

GMSFAgentLocalizedInfo *gmsf_agent_get_localized_info(GMSFAgent *agent, gushort langid) {
	g_return_val_if_fail(agent, NULL);
	return (GMSFAgentLocalizedInfo *)g_hash_table_lookup(agent->priv->localized_info, GUINT_TO_POINTER(langid));
}

gushort *gmsf_agent_get_localized_info_languages(GMSFAgent *agent, gsize *sz) {
	gushort *ret;
	gpointer *data;
	guint csz, i;
	
	g_return_val_if_fail(agent, NULL);

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
	g_return_val_if_fail(agent, NULL);

	if (agent->priv->flags & GMSF_AGENT_FLAG_VOICE_OUTPUT) {
		return &agent->priv->vi;
	}

	return NULL;
}

GMSFAgentBalloonInfo *gmsf_agent_get_balloon_info(GMSFAgent *agent) {
	g_return_val_if_fail(agent, NULL);

	if (agent->priv->flags & GMSF_AGENT_FLAG_VOICE_OUTPUT) {
		return &agent->priv->bi;
	}
	
	return NULL;
}

GMSFAgentAnimationState *gmsf_agent_get_animation_states(GMSFAgent *agent, gsize *sz) {
	g_return_val_if_fail(agent, NULL);

	if (sz) {
		*sz = agent->priv->states_sz;
	}
	
	return agent->priv->states;
}

GdkPixbuf *gmsf_agent_get_tray_icon(GMSFAgent *agent) {
	g_return_val_if_fail(agent, NULL);
	return agent->priv->tray_icon;
}

gulong gmsf_agent_get_audio_count(GMSFAgent *agent) {
	g_return_val_if_fail(agent, 0);
	return agent->priv->audios_sz;
}

SNDFILE *gmsf_agent_open_audio(GMSFAgent *agent, guint index, SF_INFO *sfinfo) {
	g_return_val_if_fail(agent, NULL);
	fseek(agent->priv->f, agent->priv->audios[index].offset, SEEK_SET);
	return sf_open_virtual(&agent->priv->audio_io, SFM_READ, sfinfo, &agent->priv->audios[index]);
}

guint8 *_raw8_to_bmp(const guint8 *data, guint16 width, guint16 height, const GMSFColor *palette, gulong palette_sz, gsize *out_size) {
    guint32 stride;
    guint32 pixel_bytes;
    guint32 palette_bytes;
    guint32 pixel_offset;
    guint32 file_size;
    guint8 *result;
    guint8 file_header[14];
    guint8 info_header[40];
    gulong i;

    if (out_size) {
        *out_size = 0;
    }

    if (!data || !palette || palette_sz == 0 || width == 0 || height == 0) {
        return NULL;
    }

    stride = ((guint32)width + 3) & 0xFC;
    pixel_bytes = stride * (guint32)height;
    palette_bytes = (guint32)palette_sz * 4;
    pixel_offset = 14 + 40 + palette_bytes;
    file_size = pixel_offset + pixel_bytes;

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

    memset(info_header, 0, sizeof(info_header));
    info_header[0] = 40;
    info_header[4] = (guint8)(width & 0xff);
    info_header[5] = (guint8)((width >> 8) & 0xff);
    info_header[8] = (guint8)(height & 0xff);
    info_header[9] = (guint8)((height >> 8) & 0xff);
    info_header[12] = 1;
    info_header[14] = 8;
    info_header[20] = (guint8)(pixel_bytes & 0xff);
    info_header[21] = (guint8)((pixel_bytes >> 8) & 0xff);
    info_header[22] = (guint8)((pixel_bytes >> 16) & 0xff);
    info_header[23] = (guint8)((pixel_bytes >> 24) & 0xff);
    info_header[32] = (guint8)(palette_sz & 0xff);
    info_header[33] = (guint8)((palette_sz >> 8) & 0xff);
    info_header[34] = (guint8)((palette_sz >> 16) & 0xff);
    info_header[35] = (guint8)((palette_sz >> 24) & 0xff);

    result = g_malloc(file_size);
    memcpy(result, file_header, 14);
    memcpy(result + 14, info_header, 40);

    for (i = 0; i < palette_sz; i++) {
		guint8 *entry;
        GMSFColor c;
        
        c = palette[i];
		entry = result + 54 + (i * 4);
        entry[0] = (guint8)GMSF_COLOR_RED(c);
        entry[1] = (guint8)GMSF_COLOR_GREEN(c);
        entry[2] = (guint8)GMSF_COLOR_BLUE(c);
        entry[3] = 0;
    }

    memcpy(result + pixel_offset, data, pixel_bytes);

    if (out_size) {
        *out_size = file_size;
    }

    return result;
}

GdkPixbuf *gmsf_agent_get_image(GMSFAgent *agent, guint index) {
	GdkPixbuf *ret;

	g_return_val_if_fail(agent, NULL);

	ret = g_hash_table_lookup(agent->priv->img_cache, GUINT_TO_POINTER(index));
	if (!ret) {
		GdkPixbuf *old;
		gpointer buf;
		guint32 *locator;
		guint32 ul;
		guint16 w, h;
		guint8 uc;
		
		locator = g_hash_table_lookup(agent->priv->img_locators, GUINT_TO_POINTER(index));
		if (!locator) {
			return NULL;
		}

		fseek(agent->priv->f, locator[0], SEEK_SET);
		fread(&uc, sizeof(guint8), 1, agent->priv->f);
		fread(&w, sizeof(guint16), 1, agent->priv->f);
		fread(&h, sizeof(guint16), 1, agent->priv->f);
		fread(&uc, sizeof(guint8), 1, agent->priv->f);
		fread(&ul, sizeof(guint32), 1, agent->priv->f);
		w = GUINT16_FROM_LE(w);
		h = GUINT16_FROM_LE(h);
		ul = GUINT32_FROM_LE(ul);

		buf = g_malloc(ul);
		fread(buf, 1, ul, agent->priv->f);
		
		if (uc) {
			GdkPixbufLoader *loader;
			GdkPixbuf *pixbuf;
			guint8 *bytes;
			gpointer dcbuf;
			gsize dcsz;
			gsize bmp_sz;
				
			dcsz = ((w + 3) & 0xFC) * h;
			dcbuf = g_malloc(dcsz);
			decompressor(buf, dcbuf);
			loader = gdk_pixbuf_loader_new();
			bytes = _raw8_to_bmp((const guint8 *)dcbuf, w, h, agent->priv->palette, agent->priv->palette_sz, &bmp_sz);
			gdk_pixbuf_loader_write(loader, (const guchar *)bytes, bmp_sz, NULL);
			gdk_pixbuf_loader_close(loader, NULL);
			ret = gdk_pixbuf_loader_get_pixbuf(loader);
			g_free(bytes);
			g_free(dcbuf);
			gdk_pixbuf_ref(ret);
			g_object_unref(loader);
		} else {
			ret = _load_bitmap(buf, ul);
		}

		old = ret;
		ret = gdk_pixbuf_add_alpha(old, TRUE, GMSF_COLOR_RED(agent->priv->palette[agent->priv->transparent_color_index]), GMSF_COLOR_GREEN(agent->priv->palette[agent->priv->transparent_color_index]), GMSF_COLOR_BLUE(agent->priv->palette[agent->priv->transparent_color_index]));
		gdk_pixbuf_unref(old);
		g_hash_table_insert(agent->priv->img_cache, GUINT_TO_POINTER(index), ret);
		g_free(buf);
	}
	
	return ret;
}

gulong gmsf_agent_get_image_count(GMSFAgent *agent) {
	g_return_val_if_fail(agent, 0);
	return g_hash_table_size(agent->priv->img_locators);
}
