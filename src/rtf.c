#include <fcntl.h>
#include <glib-object.h>
#include <gmsf.h>

struct _GMSFRichTextPrivate {
	GScanner *scanner;
	GRegex *hex_regex;
	gint to_close;
};

typedef enum {
    RTF_DEST_TEXT,
    RTF_DEST_SKIP
} GMSFRTFDestination;

typedef struct {
    GMSFRTFDestination dest;
} GMSFRTFState;

G_DEFINE_TYPE(GMSFRichText, gmsf_rich_text, G_TYPE_OBJECT)

static void demo(GMSFRichText *rtf) {
    GScanner *s;
	GQueue *state_stack;
	GMSFRTFState *current_state;
	
    s = rtf->priv->scanner;
	state_stack = g_queue_new();
    current_state = g_new0(GMSFRTFState, 1);
    current_state->dest = RTF_DEST_TEXT;
    g_queue_push_head(state_stack, current_state);

    while (!g_scanner_eof(s)) {
        GTokenType token;
        
        token = g_scanner_get_next_token(s);
        if (token == '{') {
			GMSFRTFState *new_state;
			
			new_state = g_new0(GMSFRTFState, 1);
			new_state->dest = current_state->dest; 
			g_queue_push_head(state_stack, new_state);
			current_state = new_state;
        } else if (token == '}') {
			if (g_queue_get_length(state_stack) > 1) { 
				GMSFRTFState *old_state;
				
				old_state = g_queue_pop_head(state_stack);
				g_free(old_state);
				current_state = g_queue_peek_head(state_stack);
			}
		} else if (token == '\\') {
			if (g_scanner_peek_next_token(s) == G_TOKEN_IDENTIFIER) {
				gchar *control_word;
				glong parameter;
				gboolean has_param;
				
				g_scanner_get_next_token(s);
				control_word = g_strdup(s->value.v_identifier);
				parameter = 0;
				has_param = FALSE;
				
				if (g_scanner_peek_next_token(s) == '-') {
					g_scanner_get_next_token(s);
					if (g_scanner_peek_next_token(s) == G_TOKEN_INT) {
						g_scanner_get_next_token(s);
						parameter = -(glong)s->value.v_int64;
						has_param = TRUE;
					}
				} else if (g_scanner_peek_next_token(s) == G_TOKEN_INT) {
					g_scanner_get_next_token(s);
					parameter = (glong)s->value.v_int64;
					has_param = TRUE;
				}

				if (g_scanner_peek_next_token(s) == ' ') {
					g_scanner_get_next_token(s);
				}

				if (has_param) {
					printf("CONTROL: %s %d\n", control_word, parameter);
				} else {
					printf("CONTROL: %s no param\n", control_word);	
				}
				
				if (!g_strcmp0(control_word, "fonttbl") || !g_strcmp0(control_word, "colortbl") || !g_strcmp0(control_word, "expandedcolortbl")) {
					current_state->dest = RTF_DEST_SKIP;
				}
				
				g_free(control_word);
			} else if (g_scanner_peek_next_token(s) == '*') {
				g_scanner_get_next_token(s);
				current_state->dest = RTF_DEST_SKIP;
			} else if (g_scanner_peek_next_token(s) == '\'') {
				g_scanner_get_next_token(s);
				
				if (g_scanner_peek_next_token(s) == G_TOKEN_IDENTIFIER || g_scanner_peek_next_token(s) == G_TOKEN_INT) {
					GTokenType raw_hex_token;
					gchar hex_buf[3];
					
					raw_hex_token = g_scanner_get_next_token(s);
					hex_buf[0] = 0;

					if (raw_hex_token == G_TOKEN_IDENTIFIER) {
						strncpy(hex_buf, s->value.v_identifier, 2);
					} else if (raw_hex_token == G_TOKEN_INT) {
						snprintf(hex_buf, sizeof(hex_buf), "%02x", (guint)s->value.v_int);
					}

					if (g_regex_match(rtf->priv->hex_regex, hex_buf, 0, NULL)) {
						if (current_state->dest == RTF_DEST_TEXT) {
							guint raw_byte;
							
							raw_byte = 0;
							sscanf(hex_buf, "%x", &raw_byte);
							printf("%c", (gchar)raw_byte);
						}
					}
				}
			} else if (g_scanner_peek_next_token(s) == '\\' || g_scanner_peek_next_token(s) == '{' || g_scanner_peek_next_token(s) == '}') {
				GTokenType literal_token;
				
				literal_token = g_scanner_get_next_token(s);
				if (current_state->dest == RTF_DEST_TEXT) {
					printf("%c", (gchar)literal_token);
				}
			}
		} else if (token == ' ') {
			if (current_state->dest == RTF_DEST_TEXT) {
				printf(" ");
			}
		} else if (token == G_TOKEN_IDENTIFIER) {
			if (current_state->dest == RTF_DEST_TEXT) {
				puts(s->value.v_identifier);
			}
		} else if (token == G_TOKEN_INT) {
			if (current_state->dest == RTF_DEST_TEXT) {
				printf("%d\n", s->value.v_int);
			}
		} else {
			if (current_state->dest == RTF_DEST_TEXT) {
				printf("%c\n", (gchar)token);
			}
		}
	}
	
    while (!g_queue_is_empty(state_stack)) {
        g_free(g_queue_pop_head(state_stack));
    }
    g_queue_free(state_stack);
}

static void _finalize(GObject *object) {
	GMSFRichText *rtf;

	rtf = GMSF_RICH_TEXT(object);
	if (rtf->priv->to_close > -1) {
		g_close(rtf->priv->to_close);
	}
	g_regex_unref(rtf->priv->hex_regex);
	g_scanner_destroy(rtf->priv->scanner);
    G_OBJECT_CLASS(gmsf_rich_text_parent_class)->finalize(object);
}

static void gmsf_rich_text_class_init(GMSFRichTextClass *klass) {
	GObjectClass *g_class;

	g_class = G_OBJECT_CLASS(klass);
	g_class->finalize = _finalize;
	g_type_class_add_private(klass, sizeof(GMSFRichTextPrivate));
}	

static void gmsf_rich_text_init(GMSFRichText *rtf) {
	GScannerConfig config;
	
	rtf->priv = G_TYPE_INSTANCE_GET_PRIVATE(rtf, GMSF_TYPE_RICH_TEXT, GMSFRichTextPrivate);
	rtf->priv->to_close = -1;
	
	config.cset_identifier_first = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	config.cset_identifier_nth = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	config.cset_skip_characters = "\t\r\n"; 
	config.skip_comment_multi = FALSE; 
	config.skip_comment_single = FALSE; 
	config.scan_string_sq = FALSE; 
	config.scan_string_dq = FALSE; 
	config.scan_hex = FALSE; 
	config.scan_identifier = TRUE;
	config.scan_symbols = TRUE;
	config.numbers_2_int = TRUE;
	config.char_2_token = TRUE;
	rtf->priv->scanner = g_scanner_new(&config);
	
	rtf->priv->hex_regex = g_regex_new("^[0-9a-fA-F]{2}$", G_REGEX_OPTIMIZE, 0, NULL);
}

GMSFRichText *gmsf_rich_text_new_from_fd(gint fd) {
	GMSFRichText *rtf;
	
	rtf = GMSF_RICH_TEXT(g_object_new(GMSF_TYPE_RICH_TEXT, NULL));
	g_scanner_input_file(rtf->priv->scanner, fd);
	demo(rtf);
	return rtf;
}

GMSFRichText *gmsf_rich_text_new_from_file(const gchar *file) {
	GMSFRichText *rtf;
	
	rtf = GMSF_RICH_TEXT(g_object_new(GMSF_TYPE_RICH_TEXT, NULL));
	rtf->priv->to_close = g_open(file, O_RDONLY);
	g_scanner_input_file(rtf->priv->scanner, rtf->priv->to_close);
	demo(rtf);
	return rtf;
}

GMSFRichText *gmsf_rich_text_new_from_data(gpointer data, gsize sz) {
	GMSFRichText *rtf;
	
	rtf = GMSF_RICH_TEXT(g_object_new(GMSF_TYPE_RICH_TEXT, NULL));
	g_scanner_input_text(rtf->priv->scanner, data, sz);
	demo(rtf);
	return rtf;
}

