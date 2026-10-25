/*
 * Lyrics from LRCLIB -- the search, the .lrc it saves, and the page the
 * results are picked from -- on the real renderer at the V1's 240x320.
 *
 * The network is substituted: http_get_ex() answers with LRCLIB-shaped JSON
 * written here, so the real lrclib.c parses, ranks and saves what a search
 * would bring back. What is checked on the screen is what only exists with
 * one: that every row of the list and the Apply button stay on the glass,
 * that a row opens the words with their time stamps taken out, and that Apply
 * writes the .lrc beside the track, goes back to the player and has it read
 * the words again.
 */
#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "src/gui/board_profile.h"
#include "src/gui/fonts/fonts.h"
#include "src/gui/nowplaying/lyricsearch.h"
#include "src/gui/shell/icons.h"
#include "src/gui/shell/theme.h"
#include "src/system/core/config.h"
#include "src/system/core/lang.h"
#include "src/system/library/lrclib.h"
#include "src/system/net/http.h"

/* ---- fonts, as in the other page tests ---------------------------------- */

lv_font_t font_ui_14, font_ui_16, font_ui_18, font_ui_20, font_ui_20_bold;
lv_font_t font_ui_22, font_ui_22_bold, font_ui_24, font_ui_24_bold;
lv_font_t font_ui_26, font_ui_28, font_ui_32;
lv_font_t font_ui_36_bold, font_ui_64_bold, font_ui_72;

static void init_fonts(void) {
	lv_font_t *all[] = {
		&font_ui_14, &font_ui_16, &font_ui_18, &font_ui_20, &font_ui_20_bold, &font_ui_22, &font_ui_22_bold,
		&font_ui_24, &font_ui_24_bold, &font_ui_26, &font_ui_28, &font_ui_32, &font_ui_36_bold,
		&font_ui_64_bold, &font_ui_72,
	};
	for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) {
		*all[i] = lv_font_montserrat_14;
	}
}

void fonts_register_change(void (*cb)(void)) { (void)cb; }
int fonts_text_size(void) { return FONTS_TEXT_NORMAL; }

/* ---- the parts of the shell the page leans on, substituted -------------- */

void bootlogo_set(int choice, bool dark) {
	(void)choice;
	(void)dark;
}
bool bootpic_present(void) { return false; }

static char last_toast[128];
void toast_error(const char *text) { snprintf(last_toast, sizeof(last_toast), "error:%s", text ? text : ""); }
void toast_plain(const char *text) { snprintf(last_toast, sizeof(last_toast), "plain:%s", text ? text : ""); }
void toast_success(const char *text) { snprintf(last_toast, sizeof(last_toast), "success:%s", text ? text : ""); }

lv_obj_t *player_screen;
void switch_screen(lv_obj_t *screen) { lv_screen_load(screen); }
void switch_screen_cb(lv_event_t *e) { switch_screen(lv_event_get_user_data(e)); }
void back_btn_cb(lv_event_t *e) {
	(void)e;
	switch_screen(player_screen);
}
void switcher_attach_back_gesture(lv_obj_t *obj) { (void)obj; }
bool switcher_back_drag_active(void) { return false; }
static lv_obj_t *player_return;
void switcher_set_player_return(lv_obj_t *screen) { player_return = screen; }
static bool (*back_guard)(void);
void switcher_set_back_guard(lv_obj_t *screen, bool (*handler)(void)) {
	(void)screen;
	back_guard = handler;
}
void switcher_set_back_guard_peek(lv_obj_t *screen, bool (*peek)(void)) {
	(void)screen;
	(void)peek;
}
void player_sheet_attach_drag(lv_obj_t *obj, bool opening) {
	(void)obj;
	(void)opening;
}
bool player_sheet_drag_active(void) { return false; }
void player_sheet_close(bool animate) { (void)animate; }

static char reloaded[1024];
void player_lyrics_reload(const char *path) { snprintf(reloaded, sizeof(reloaded), "%s", path); }

/* ---- the network, substituted ------------------------------------------- */

static const char *answer;	// the body of the next search, NULL for a 404
static int requests;
static char last_url[4096];

bool http_get_ex(const char *url, const char *extra_headers, bool want_error_body, char **out, size_t *out_len,
				 size_t limit, int timeout_secs, int *status_out) {
	(void)extra_headers;
	(void)want_error_body;
	(void)limit;
	(void)timeout_secs;
	requests++;
	snprintf(last_url, sizeof(last_url), "%s", url);
	if (!answer) {
		*status_out = 404;
		return false;
	}
	*status_out = 200;
	*out = strdup(answer);
	if (out_len) {
		*out_len = strlen(answer);
	}
	return true;
}

const char *http_last_error(void) { return NULL; }

char *http_url_encode(const char *in, char *out, size_t out_size) {
	size_t w = 0;
	for (const unsigned char *p = (const unsigned char *)in; *p && w + 4 < out_size; p++) {
		if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '-' ||
			*p == '_' || *p == '.' || *p == '~') {
			out[w++] = (char)*p;
		} else {
			w += (size_t)snprintf(out + w, out_size - w, "%%%02X", *p);
		}
	}
	out[w] = '\0';
	return out;
}

/* Three records with words and one without: a plain one from the album, a
 * timed one ten seconds off, a timed one within the slack -- which has to come
 * first -- and an instrumental entry, which is not listed at all. */
static const char SEARCH_BODY[] =
	"["
	"{\"id\":1,\"trackName\":\"Song\",\"artistName\":\"Artist\",\"albumName\":\"Album\",\"duration\":200,"
	"\"instrumental\":false,\"plainLyrics\":\"First line\\nSecond line\",\"syncedLyrics\":null},"
	"{\"id\":2,\"trackName\":\"Song (Live)\",\"artistName\":\"Artist\",\"albumName\":\"Live\",\"duration\":211,"
	"\"instrumental\":false,\"plainLyrics\":\"x\",\"syncedLyrics\":\"[00:01.00] Live line\"},"
	"{\"id\":3,\"trackName\":\"Song\",\"artistName\":\"Artist\",\"albumName\":\"Other\",\"duration\":201.4,"
	"\"instrumental\":false,\"plainLyrics\":\"x\","
	"\"syncedLyrics\":\"[ar:Artist]\\n[00:01.00] First line\\n[00:04.50]\\n[00:05.00][00:30.00] Chorus\"},"
	"{\"id\":4,\"trackName\":\"Song\",\"artistName\":\"Artist\",\"albumName\":\"Album\",\"duration\":200,"
	"\"instrumental\":true,\"plainLyrics\":null,\"syncedLyrics\":null}"
	"]";

static void check_search(void) {
	lrclib_record_t *records;
	int count;

	/* No artist in the tags: nothing is asked. */
	requests = 0;
	assert(lrclib_search("Song", "", "Album", 200, &records, &count) == LRCLIB_NO_TAGS);
	assert(requests == 0 && records == NULL && count == 0);

	/* Nothing under the tags, nor as free text. */
	answer = NULL;
	assert(lrclib_search("Song", "Artist", "Album", 200, &records, &count) == LRCLIB_NOT_FOUND);
	assert(requests == 2);
	assert(strstr(last_url, "/search?q=Song%20Artist"));

	/* Only an entry that says the track has no words. */
	answer = "[{\"trackName\":\"Song\",\"artistName\":\"Artist\",\"instrumental\":true}]";
	assert(lrclib_search("Song", "Artist", "", 0, &records, &count) == LRCLIB_INSTRUMENTAL);

	answer = SEARCH_BODY;
	requests = 0;
	assert(lrclib_search("Song", "Artist & Co", "Album", 200, &records, &count) == LRCLIB_OK);
	assert(requests == 1);
	assert(strstr(last_url, "https://lrclib.net/api/search?track_name=Song&artist_name=Artist%20%26%20Co"));
	assert(count == 3);
	assert(records[0].synced && records[0].duration == 201 && strcmp(records[0].album, "Other") == 0);
	assert(!records[1].synced && strcmp(records[1].album, "Album") == 0);
	assert(records[2].synced && records[2].duration == 211);
	lrclib_free(records, count);
}

/* ---- the strings, in every language -------------------------------------- */

static bool is_ini(const char *name) {
	size_t n = strlen(name);
	return n > 4 && strcmp(name + n - 4, ".ini") == 0;
}

static void check_strings(const char *language_dir) {
	static const char *const KEYS[] = {
		"lyrics_download_title", "lyrics_download_note", "lyrics_download", "lyrics_downloading",
		"lyrics_downloaded", "lyrics_not_found", "lyrics_instrumental", "lyrics_no_tags",
		"lyrics_not_saved", "lyrics_download_failed", "lyrics_no_wifi", "lyrics_wifi_off_note",
		"lyrics_enable_wifi", "lyrics_wifi_connecting", "lyrics_wifi_failed", "lyrics_results",
		"lyrics_synced", "lyrics_plain", "lyrics_apply",
	};
	DIR *dir = opendir(language_dir);
	assert(dir);
	int languages = 0;
	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		if (!is_ini(entry->d_name)) {
			continue;
		}
		char name[256];
		snprintf(name, sizeof(name), "%.*s", (int)(strlen(entry->d_name) - 4), entry->d_name);
		assert(lang_set(name));
		for (size_t i = 0; i < sizeof(KEYS) / sizeof(KEYS[0]); i++) {
			const char *text = tr(KEYS[i]);
			if (!text || !text[0] || strcmp(text, KEYS[i]) == 0) {
				fprintf(stderr, "%s: \"%s\" is not translated\n", name, KEYS[i]);
				assert(!"a string is missing from a language file");
			}
		}
		languages++;
	}
	closedir(dir);
	assert(languages == 9);
	assert(lang_set("English"));
}

/* ---- small helpers ------------------------------------------------------- */

static lv_obj_t *find_label(lv_obj_t *obj, const char *text) {
	if (lv_obj_check_type(obj, &lv_label_class) && strcmp(lv_label_get_text(obj), text) == 0) {
		return obj;
	}
	for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) {
		lv_obj_t *found = find_label(lv_obj_get_child(obj, i), text);
		if (found) {
			return found;
		}
	}
	return NULL;
}

static lv_obj_t *button_of_label(lv_obj_t *root, const char *text) {
	lv_obj_t *label = find_label(root, text);
	assert(label);
	lv_obj_t *button = lv_obj_get_parent(label);
	while (button && !lv_obj_check_type(button, &lv_button_class)) {
		button = lv_obj_get_parent(button);
	}
	assert(button);
	return button;
}

/* Nothing drawn may leave the panel sideways; downwards only inside a
 * container that scrolls that way. */
static void check_within_panel_in(lv_obj_t *obj, gui_config_t *cfg, const char *what, bool scrolls) {
	if (lv_obj_is_hidden(obj)) {
		return;
	}
	lv_area_t area;
	lv_obj_get_coords(obj, &area);
	bool overflow_x = area.x1 < 0 || area.x2 >= (int32_t)cfg->screen_width;
	bool overflow_y = area.y1 < 0 || area.y2 >= (int32_t)cfg->screen_height;
	if (overflow_x || (overflow_y && !scrolls)) {
		fprintf(stderr, "%s: an object at %d,%d..%d,%d is outside the %dx%d panel\n", what, area.x1, area.y1, area.x2,
				area.y2, (int)cfg->screen_width, (int)cfg->screen_height);
		assert(!"an object left the panel");
	}
	if (lv_obj_get_scroll_dir(obj) & LV_DIR_VER) {
		scrolls = true;
	}
	for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) {
		check_within_panel_in(lv_obj_get_child(obj, i), cfg, what, scrolls);
	}
}

static void dump_snapshot(lv_obj_t *screen, const char *tag) {
	const char *prefix = getenv("SONIX_PAGE_SNAPSHOTS");
	if (!prefix || !prefix[0]) {
		return;
	}
	lv_obj_update_layout(screen);
	lv_draw_buf_t *shot = lv_snapshot_take(screen, LV_COLOR_FORMAT_RGB888);
	assert(shot);
	char path[512];
	snprintf(path, sizeof(path), "%s-%s.ppm", prefix, tag);
	FILE *file = fopen(path, "wb");
	assert(file);
	fprintf(file, "P6\n%u %u\n255\n", shot->header.w, shot->header.h);
	for (uint32_t y = 0; y < shot->header.h; y++) {
		const uint8_t *row = shot->data + y * shot->header.stride;
		for (uint32_t x = 0; x < shot->header.w; x++) {
			uint8_t rgb[] = {row[x * 3 + 2], row[x * 3 + 1], row[x * 3]};
			assert(fwrite(rgb, sizeof(rgb), 1, file) == 1);
		}
	}
	assert(fclose(file) == 0);
	lv_draw_buf_destroy(shot);
}

static char *read_file(const char *path) {
	FILE *f = fopen(path, "rb");
	if (!f) {
		return NULL;
	}
	static char buf[4096];
	size_t n = fread(buf, 1, sizeof(buf) - 1, f);
	fclose(f);
	buf[n] = '\0';
	return buf;
}

/* ---- the page ------------------------------------------------------------- */

int main(int argc, char **argv) {
	assert(argc == 2);

	char card[] = "/tmp/sonix-lyrics-ui-test-XXXXXX";
	assert(mkdtemp(card));
	char config[512];
	snprintf(config, sizeof(config), "%s/device_config.ini", card);
	config_init(config);
	config_set("ui", "language", "English");
	lang_init(argv[1]);
	check_strings(argv[1]);
	check_search();

	init_fonts();
	lv_init();
	gui_config_t cfg = {bp_screen_w(), bp_screen_h(), bp_padding(), bp_status_bar_h(), card};
	lv_display_create(cfg.screen_width, cfg.screen_height);
	theme_init();

	player_screen = lv_obj_create(NULL);
	lyricsearch_screen = lv_obj_create(NULL);
	lyricsearch_init(&cfg);

	char track[600];
	snprintf(track, sizeof(track), "%s/Artist - Song.flac", card);
	lrclib_record_t *records;
	int count;
	answer = SEARCH_BODY;
	assert(lrclib_search("Song", "Artist", "Album", 200, &records, &count) == LRCLIB_OK);
	lyricsearch_open(track, records, count);
	assert(lv_screen_active() == lyricsearch_screen);
	assert(player_return == lyricsearch_screen);

	/* The list: one row per record, each finger-sized and on the glass, the
	 * timed ones marked as such. */
	lv_obj_update_layout(lyricsearch_screen);
	check_within_panel_in(lyricsearch_screen, &cfg, "Lyrics found", false);
	dump_snapshot(lyricsearch_screen, "v1-lyrics-list");
	assert(find_label(lyricsearch_screen, tr("lyrics_results")));
	lv_obj_t *first = button_of_label(lyricsearch_screen, "Artist - Other");
	assert(lv_obj_get_height(first) >= 44);
	assert(lv_obj_get_width(first) >= (int32_t)cfg.screen_width - 3 * (int32_t)cfg.padding);
	assert(find_label(first, tr("lyrics_synced")));
	assert(find_label(first, "3:21"));
	assert(find_label(button_of_label(lyricsearch_screen, "Artist - Album"), tr("lyrics_plain")));

	/* A row opens its words -- stamps, tags and timed pauses taken out --
	 * with Apply under them; back returns to the list rather than leaving
	 * the page. */
	lv_obj_send_event(first, LV_EVENT_CLICKED, NULL);
	lv_obj_update_layout(lyricsearch_screen);
	check_within_panel_in(lyricsearch_screen, &cfg, "Lyrics preview", false);
	dump_snapshot(lyricsearch_screen, "v1-lyrics-preview");
	assert(find_label(lyricsearch_screen, "First line\nChorus"));
	lv_obj_t *apply = button_of_label(lyricsearch_screen, tr("lyrics_apply"));
	assert(!lv_obj_is_hidden(apply));
	assert(back_guard && back_guard());
	assert(lv_obj_is_hidden(lv_obj_get_parent(apply)));
	assert(!back_guard());

	/* Apply: the timed words saved as "<track>.lrc", back in the player, and
	 * the player told to read them. */
	lv_obj_send_event(first, LV_EVENT_CLICKED, NULL);
	lv_obj_send_event(apply, LV_EVENT_CLICKED, NULL);
	char lrc[700];
	snprintf(lrc, sizeof(lrc), "%s/Artist - Song.lrc", card);
	const char *saved = read_file(lrc);
	assert(saved);
	assert(strcmp(saved, "[ar:Artist]\n[00:01.00] First line\n[00:04.50]\n[00:05.00][00:30.00] Chorus\n") == 0);
	assert(strcmp(reloaded, track) == 0);
	assert(strcmp(last_toast, "success:lyrics_downloaded") == 0);
	assert(lv_screen_active() == player_screen);
	char part[720];
	snprintf(part, sizeof(part), "%s.part", lrc);
	assert(access(part, F_OK) != 0);

	unlink(lrc);
	unlink(config);
	rmdir(card);
	lv_deinit();
	printf("PASS lyrics search (%s)\n", bp_is_tempotec_v1() ? "240x320" : "480x720");
	return 0;
}
