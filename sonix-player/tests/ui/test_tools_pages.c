/*
 * The two pages the More menu gained -- the Calculator and the Notes shelf --
 * on the real renderer, at both panel sizes.
 *
 * The arithmetic (test_calculator.c) and the folder rules (test_notes.c) are
 * checked in plain C elsewhere; what is checked here is the part that only
 * exists with a screen: that both destinations are on the More page, that the
 * keys and rows are finger-sized and land inside a 240x320 V1 panel, that a
 * key or a row actually does what it is drawn promising, and that leaving the
 * editor with writing unsaved still ends with the writing on the card.
 */
#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "src/gui/board_profile.h"
#include "src/gui/calculator/calculator.h"
#include "src/gui/fonts/fonts.h"
#include "src/gui/notes/noteeditor.h"
#include "src/gui/notes/notes.h"
#include "src/gui/shell/gridpage.h"
#include "src/system/playback/device_state.h"
#include "src/gui/shell/icons.h"
#include "src/gui/shell/morepage.h"
#include "src/gui/shell/theme.h"
#include "src/system/core/config.h"
#include "src/system/core/lang.h"
#include "src/system/notes/notesstore.h"
#include "src/system/text/textfile.h"

/* ---- the parts of the shell these pages lean on, substituted ------------- */

lv_obj_t *music_screen, *streaming_screen, *wireless_screen;
lv_obj_t *audiobooks_screen, *settings_screen, *player_screen;
lv_obj_t *ebookpage_screen, *browser_screen;
lv_obj_t *dacpage_screen, *gearboypage_screen;

lv_font_t font_ui_14, font_ui_16, font_ui_18, font_ui_20, font_ui_20_bold;
lv_font_t font_ui_22, font_ui_22_bold, font_ui_24, font_ui_24_bold;
lv_font_t font_ui_26, font_ui_28, font_ui_32;
static void (*font_callbacks[16])(void);
static int font_callback_count;
static int text_size = FONTS_TEXT_NORMAL;

static void init_fonts(void) {
	lv_font_t *fonts[] = {
		&font_ui_14, &font_ui_16, &font_ui_18, &font_ui_20, &font_ui_20_bold,
		&font_ui_22, &font_ui_22_bold, &font_ui_24, &font_ui_24_bold,
		&font_ui_26, &font_ui_28, &font_ui_32,
	};
	for (size_t i = 0; i < sizeof(fonts) / sizeof(fonts[0]); i++) {
		*fonts[i] = lv_font_montserrat_14;
	}
}

void fonts_register_change(void (*cb)(void)) {
	assert(font_callback_count < 16);
	font_callbacks[font_callback_count++] = cb;
}
int fonts_text_size(void) { return text_size; }
bool fonts_set_text_size(int size) {
	text_size = size;
	for (int i = 0; i < font_callback_count; i++) {
		font_callbacks[i]();
	}
	return true;
}
const char *fonts_choice(void) { return "default"; }
bool fonts_set_face(const char *id) { return strcmp(id, "default") == 0; }
int fonts_choices(const char **ids, const char **labels, int max) {
	assert(max > 0);
	ids[0] = "default";
	labels[0] = "Test font";
	return 1;
}

void bootlogo_set(int choice, bool dark) { (void)choice; (void)dark; }
void device_state_get(device_state_t *out) { memset(out, 0, sizeof(*out)); }
bool gui_card_available(void) { return true; }
void gui_notify_no_card(void) { assert(!"the card is there in this test"); }
void gui_notify_popup(const char *text) {
	fprintf(stderr, "popup: %s\n", text ? text : "");
	assert(!"unexpected popup");
}
void toast_error(const char *text) {
	fprintf(stderr, "error: %s\n", text ? text : "");
	assert(!"unexpected error toast");
}
void toast_plain(const char *text) { (void)text; }
void toast_success(const char *text) { (void)text; }
void topbar_set_battery_percent(bool shown) { (void)shown; }
void topbar_set_clock_position(int pos) { (void)pos; }
void switch_screen(lv_obj_t *screen) { lv_screen_load(screen); }
void switch_screen_cb(lv_event_t *e) { switch_screen(lv_event_get_user_data(e)); }
void ebookpage_open(void) { switch_screen(ebookpage_screen); }
void filespage_open(void) { switch_screen(browser_screen); }
void flappybird_open(void) { /* not reached from this test */ }
void switcher_attach_back_gesture(lv_obj_t *obj) { (void)obj; }
bool switcher_back_drag_active(void) { return false; }
void switcher_set_back_guard(lv_obj_t *screen, bool (*handler)(void)) { (void)screen; (void)handler; }
void switcher_set_back_guard_peek(lv_obj_t *screen, bool (*peek)(void)) { (void)screen; (void)peek; }
void player_sheet_attach_drag(lv_obj_t *obj, bool opening) { (void)obj; (void)opening; }
bool player_sheet_drag_active(void) { return false; }
void coverflow_attach_edge(lv_obj_t *obj, gui_config_t *cfg) { (void)obj; (void)cfg; }
bool coverflow_drag_active(void) { return false; }

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

static lv_obj_t *find_image_button(lv_obj_t *obj, const lv_image_dsc_t *glyph);

static lv_obj_t *find_class(lv_obj_t *obj, const lv_obj_class_t *type) {
	if (lv_obj_check_type(obj, type)) {
		return obj;
	}
	for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) {
		lv_obj_t *found = find_class(lv_obj_get_child(obj, i), type);
		if (found) {
			return found;
		}
	}
	return NULL;
}

/* Whether a page holds an entry named `label` -- the tile or row whose
 * caption is that word, whole or cut short to fit the tile it is drawn on.
 * A narrowed caption carries LVGL's ellipsis, so the dotted tail is not part
 * of the comparison; the visible head still has to be the word itself. */
static bool has_entry(lv_obj_t *container, const char *label) {
	size_t full = strlen(label);
	for (uint32_t i = 0; i < lv_obj_get_child_count(container); i++) {
		lv_obj_t *caption = find_class(lv_obj_get_child(container, i), &lv_label_class);
		if (!caption) {
			continue;
		}
		const char *text = lv_label_get_text(caption);
		size_t cut = strlen(text);
		while (cut > 0 && text[cut - 1] == '.') {
			cut--;
		}
		if (cut == 0 || cut > full || strncmp(text, label, cut) != 0) {
			continue;
		}
		// A caption may be cut short, but not to a stub that could be the
		// start of any word: the whole label, or five characters of it.
		if (cut == full || cut >= 5) {
			return true;
		}
	}
	return false;
}

/* Presses what a finger would press to reach `text`: the button carrying it. */
static void press_key(lv_obj_t *root, const char *text) {
	lv_obj_t *label = find_label(root, text);
	assert(label);
	lv_obj_t *key = lv_obj_get_parent(label);
	while (key && !lv_obj_check_type(key, &lv_button_class)) {
		key = lv_obj_get_parent(key);
	}
	assert(key);
	lv_obj_send_event(key, LV_EVENT_CLICKED, NULL);
}

/* Nothing drawn, anywhere on the page, may leave the panel.
 *
 * Sideways is never allowed. Downwards is, inside a container that scrolls
 * that way and no further: the More page's list and the note editor's text
 * area both hold more than the panel can show, and an object past the foot of
 * one of those is an object the user can scroll to. Everything outside such a
 * container -- the keys of the calculator, the tiles of a grid, the corner
 * controls -- has to be on the glass where it is drawn. */
static void check_within_panel_in(lv_obj_t *obj, gui_config_t *cfg, const char *what, bool scrolls) {
	if (lv_obj_is_hidden(obj)) {
		return;
	}
	lv_area_t area;
	lv_obj_get_coords(obj, &area);
	bool overflow_x = area.x1 < 0 || area.x2 >= (int32_t)cfg->screen_width;
	bool overflow_y = area.y1 < 0 || area.y2 >= (int32_t)cfg->screen_height;
	if (overflow_x || (overflow_y && !scrolls)) {
		fprintf(stderr, "%s: an object at %d,%d..%d,%d is outside the %dx%d panel\n", what, area.x1, area.y1,
				area.x2, area.y2, (int)cfg->screen_width, (int)cfg->screen_height);
		assert(!"an object left the panel");
	}
	// A container that scrolls takes the rest of its subtree with it.
	if (lv_obj_get_scroll_dir(obj) & LV_DIR_VER) {
		scrolls = true;
	}
	for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) {
		check_within_panel_in(lv_obj_get_child(obj, i), cfg, what, scrolls);
	}
}

static void check_within_panel(lv_obj_t *obj, gui_config_t *cfg, const char *what) {
	check_within_panel_in(obj, cfg, what, false);
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

/* ---- reading back what a page writes ------------------------------------- */

static char *read_note(const char *dir, const char *name) {
	char path[1200];
	assert(notes_path(path, sizeof(path), dir, name));
	return notes_read(path, NULL);
}

/* ---- the strings, in every language -------------------------------------- */

static bool is_ini(const char *name) {
	size_t n = strlen(name);
	return n > 4 && strcmp(name + n - 4, ".ini") == 0;
}

/* Every language file has to carry the two pages' strings -- the More menu's
 * own two names, the calculator's error state, and the notes shelf's rows and
 * notices. tr() falls back to the key it is given, so a string missing from a
 * file does not fail; it shows the tag itself, which is how a page ends up
 * labelled "notes_new" on a Japanese player and nobody notices. */
static void check_strings(const char *language_dir) {
	static const char *const KEYS[] = {
		"calculator", "calculator_error", "notes", "notes_new", "notes_empty",
		"notes_more", "notes_placeholder", "notes_saved", "notes_cannot_save",
		"notes_cannot_open", "notes_too_long",
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
			// Long enough to be read on the page, short enough for the
			// notifications' own buffers: several pages build a popup in a
			// 160-byte array, and a translation longer than that would be
			// cut off there rather than here.
			assert(strlen(text) < 160);
		}
		languages++;
	}
	closedir(dir);
	assert(languages == 11);
	assert(lang_set("English"));
}

/* ---- the More page ------------------------------------------------------- */

/* The More page has two shapes: the tiles every board shares, and the V1's
 * single-column list, used by the themes built on function glyphs. Whichever
 * is on screen is the one that has to be right.
 *
 * They are told apart by their layout -- the tiles are a wrapping row, the
 * list a column -- because their entries look alike: both are buttons with a
 * caption, so an entry cannot say which shape it is part of. */
static lv_obj_t *visible_shape(lv_obj_t *page, bool *is_grid) {
	lv_obj_t *tiles = NULL;
	lv_obj_t *rows = NULL;
	for (uint32_t i = 0; i < lv_obj_get_child_count(page); i++) {
		lv_obj_t *child = lv_obj_get_child(page, i);
		if (lv_obj_get_child_count(child) < 2) {
			continue; // the title, or a page's own floating control
		}
		if (lv_obj_get_style_flex_flow(child, LV_PART_MAIN) == LV_FLEX_FLOW_ROW_WRAP) {
			tiles = child;
		} else {
			rows = child;
		}
	}
	// The list is a V1 shape; the boards with the wide panel draw the tiles
	// only. Either way there is exactly one shape on screen.
	assert(tiles || rows);
	if (tiles && !lv_obj_is_hidden(tiles)) {
		assert(!rows || lv_obj_is_hidden(rows));
		*is_grid = true;
		return tiles;
	}
	assert(rows && !lv_obj_is_hidden(rows));
	*is_grid = false;
	return rows;
}

/* The theme a snapshot was taken under, for its file name: the More page is
 * laid out as tiles or as the V1 list depending on the theme, so a run that
 * keeps one snapshot per shape would keep the last theme that used it. */
static const char *theme_tag(void) {
	switch (theme_get_kind()) {
	case THEME_KIND_LIGHT:
		return "light";
	case THEME_KIND_TOKYO_NIGHT:
		return "tokyo";
	case THEME_KIND_AMBER_ON_BLACK:
		return "amber";
	case THEME_KIND_OLED:
		return "oled";
	default:
		return "dark";
	}
}

static void check_more_page(gui_config_t *cfg, const char *tag) {
	lv_screen_load(morepage_screen);
	lv_obj_update_layout(morepage_screen);
	check_within_panel(morepage_screen, cfg, "More");
	dump_snapshot(morepage_screen, tag);

	bool tiles = false;
	lv_obj_t *shown = visible_shape(morepage_screen, &tiles);
	lv_area_t area;
	// Both new destinations are on the shape that is actually drawn, and so is
	// everything the page offered before them: the page gained two entries, it
	// did not lose any.
	assert(has_entry(shown, tr("calculator")));
	assert(has_entry(shown, tr("notes")));
	assert(has_entry(shown, tr("dac")));
	assert(has_entry(shown, tr("books")));
	assert(has_entry(shown, tr("file_explorer")));
	assert(has_entry(shown, tr("flappy_bird")));

	// A tile of a page that does not scroll has to be on the glass, every one
	// of them: an entry laid out past the foot of a page that cannot be
	// scrolled is an entry nobody can reach. (The list does scroll, so its
	// rows may run past the foot; there, what matters is that a row is a
	// finger target.)
	int entries = 0;
	for (uint32_t i = 0; i < lv_obj_get_child_count(shown); i++) {
		lv_obj_t *entry = lv_obj_get_child(shown, i);
		if (lv_obj_is_hidden(entry)) {
			continue;
		}
		entries++;
		lv_obj_get_coords(entry, &area);
		assert(lv_obj_get_width(entry) <= (int32_t)cfg->screen_width);
		assert(area.x1 >= 0 && area.x2 < (int32_t)cfg->screen_width);
		if (tiles) {
			assert(area.y1 >= 0 && area.y2 < (int32_t)cfg->screen_height);
			// Small enough to fit seven on the V1's panel, never smaller
			// than a tile the label font can still be read on.
			assert(lv_obj_get_width(entry) >= bp_pick(70, 90));
			assert(lv_obj_get_height(entry) >= bp_pick(72, 90));
		} else {
			// A row is a finger target, and as wide as the list it is in.
			// Its foot is allowed past the panel: the list scrolls, which is
			// how the rows below the fold are reached at all.
			assert(lv_obj_get_height(entry) >= bp_pick(44, 56));
			assert(lv_obj_get_width(entry) >= (int32_t)cfg->screen_width - 3 * (int32_t)cfg->padding);
			if (entries == 1) {
				assert(area.y1 >= 0 && area.y1 < (int32_t)cfg->screen_height);
			}
		}
	}
	assert(entries >= 5);
}

/* ---- the Calculator page ------------------------------------------------- */

/* The card the sum is drawn in: one wide label alone in its own card, unlike
 * a key (a button, whether it wears a caption or a glyph) or the keypad (which
 * holds the twenty of them). */
static lv_obj_t *calc_display_label(lv_obj_t *page) {
	for (uint32_t i = 0; i < lv_obj_get_child_count(page); i++) {
		lv_obj_t *child = lv_obj_get_child(page, i);
		if (lv_obj_check_type(child, &lv_button_class) || lv_obj_get_child_count(child) != 1) {
			continue;
		}
		lv_obj_t *only = lv_obj_get_child(child, 0);
		if (lv_obj_check_type(only, &lv_label_class)) {
			return only;
		}
	}
	return NULL;
}

static void check_calculator_page(gui_config_t *cfg, const char *tag) {
	calculator_open();
	assert(lv_screen_active() == calculator_screen);
	lv_obj_update_layout(calculator_screen);
	check_within_panel(calculator_screen, cfg, "Calculator");
	dump_snapshot(calculator_screen, tag);

	lv_obj_t *display = calc_display_label(calculator_screen);
	assert(display);

	// Twenty keys, every one of them a finger target: the V1's own corner
	// buttons are 36 px, so a key is not allowed to be smaller than that.
	lv_obj_t *keypad = NULL;
	for (uint32_t i = 0; i < lv_obj_get_child_count(calculator_screen); i++) {
		lv_obj_t *child = lv_obj_get_child(calculator_screen, i);
		if (lv_obj_get_child_count(child) == 20) {
			keypad = child;
		}
	}
	assert(keypad);
	for (uint32_t i = 0; i < lv_obj_get_child_count(keypad); i++) {
		lv_obj_t *key = lv_obj_get_child(keypad, i);
		assert(lv_obj_get_width(key) >= bp_pick(34, 60));
		assert(lv_obj_get_height(key) >= bp_pick(28, 60));
	}

	// The keys that carry a glyph rather than a letter are there too, and the
	// four operations are spelled the way the rest of the interface spells
	// them: a multiplication sign, not an asterisk.
	assert(find_label(keypad, "AC"));
	assert(find_label(keypad, "="));
	assert(find_label(keypad, "\xc2\xb1"));
	assert(find_label(keypad, "%"));
	assert(find_label(keypad, "\xc3\xb7"));
	assert(find_label(keypad, "\xc3\x97"));

	// A sum typed on the glass. The display follows every key, which is the
	// whole of what the page does with the model.
	press_key(keypad, "AC");
	assert(strcmp(lv_label_get_text(display), "0") == 0);
	press_key(keypad, "1");
	press_key(keypad, "2");
	assert(strcmp(lv_label_get_text(display), "12") == 0);
	press_key(keypad, "+");
	press_key(keypad, "7");
	assert(strcmp(lv_label_get_text(display), "7") == 0);
	press_key(keypad, "=");
	assert(strcmp(lv_label_get_text(display), "19") == 0);

	// The error state is drawn in the language's own words, not the model's.
	press_key(keypad, "AC");
	press_key(keypad, "9");
	press_key(keypad, "\xc3\xb7");
	press_key(keypad, "0");
	press_key(keypad, "=");
	assert(strcmp(lv_label_get_text(display), tr("calculator_error")) == 0);
	press_key(keypad, "AC");
	assert(strcmp(lv_label_get_text(display), "0") == 0);

	// The display label stays inside its card with the widest number it can
	// hold -- a result too long for the card is the one thing that would put
	// ink outside the panel.
	press_key(keypad, "9");
	for (int i = 0; i < 11; i++) {
		press_key(keypad, "9");
	}
	assert(lv_obj_get_self_width(display) <= lv_obj_get_width(lv_obj_get_parent(display)));

	// Leaving and coming back shows the sum where it was left: the page is
	// built once and the model outlives the visit.
	press_key(keypad, "AC");
	press_key(keypad, "7");
	press_key(keypad, "+");
	press_key(keypad, "5");
	lv_screen_load(morepage_screen);
	calculator_open();
	assert(strcmp(lv_label_get_text(display), "5") == 0);
	press_key(keypad, "=");
	assert(strcmp(lv_label_get_text(display), "12") == 0);
	press_key(keypad, "AC");
}

/* ---- the Notes page ------------------------------------------------------ */

static void check_notes_page(gui_config_t *cfg, const char *dir, const char *tag, const char *tag_editor,
							 const char *tag_after) {
	// Through the page's own door, as the More menu opens it: that is what
	// reads the card and then shows the listing.
	notes_open();
	assert(lv_screen_active() == notes_screen);
	lv_obj_update_layout(notes_screen);
	check_within_panel(notes_screen, cfg, "Notes");
	dump_snapshot(notes_screen, tag);

	// The way to write something, then the notes the card holds, by the names
	// they are shown under -- without the .txt they are stored under.
	assert(find_label(notes_screen, tr("notes_new")));
	lv_obj_t *row = find_label(notes_screen, "shopping");
	assert(row);
	assert(find_label(notes_screen, "recipe"));
	// A picture in the folder is not offered: it is not a note.
	assert(!find_label(notes_screen, "cover"));

	// A row is a finger target on the V1 as well.
	lv_obj_t *row_btn = lv_obj_get_parent(row);
	while (row_btn && !lv_obj_check_type(row_btn, &lv_button_class)) {
		row_btn = lv_obj_get_parent(row_btn);
	}
	assert(row_btn);
	assert(lv_obj_get_height(row_btn) >= bp_pick(40, 56));

	// Tapping it opens that note.
	lv_obj_send_event(row_btn, LV_EVENT_CLICKED, NULL);
	assert(lv_screen_active() == noteeditor_screen);
	lv_obj_update_layout(noteeditor_screen);
	check_within_panel(noteeditor_screen, cfg, "Note editor");
	dump_snapshot(noteeditor_screen, tag_editor);

	lv_obj_t *field = find_class(noteeditor_screen, &lv_textarea_class);
	assert(field);
	// The file holds a final newline -- textfile_write() puts one on every note
	// -- and the editor shows the file as it is.
	assert(strcmp(lv_textarea_get_text(field), "milk and bread\n") == 0);
	// The title is the note's name without the extension.
	assert(find_label(noteeditor_screen, "shopping"));

	// Typing, then leaving the page: the writing reaches the card even though
	// the check button was never pressed. Nothing here is the only copy.
	lv_textarea_set_text(field, "milk, bread and eggs");
	char *text = read_note(dir, "shopping.txt");
	assert(text && strcmp(text, "milk and bread\n") == 0);
	free(text);

	notes_open();
	text = read_note(dir, "shopping.txt");
	assert(text && strcmp(text, "milk, bread and eggs\n") == 0);
	free(text);
	// And the row still shows the note's name, not its extension.
	assert(find_label(notes_screen, "shopping"));

	// A new note: the naming dialog, the accept key, and the note it leaves
	// behind -- written before anything is typed into it, so it is in the list
	// the moment it exists.
	notes_open();
	lv_obj_t *new_row = lv_obj_get_parent(find_label(notes_screen, tr("notes_new")));
	while (new_row && !lv_obj_check_type(new_row, &lv_button_class)) {
		new_row = lv_obj_get_parent(new_row);
	}
	assert(new_row);
	lv_obj_send_event(new_row, LV_EVENT_CLICKED, NULL);

	// The dialog is up, with the name it suggests already in the field.
	lv_obj_t *name_field = find_class(notes_screen, &lv_textarea_class);
	assert(name_field);
	assert(!lv_obj_is_hidden(name_field));
	const char *suggested = lv_textarea_get_text(name_field);
	assert(suggested && suggested[0]);
	assert(notes_name_ok(suggested));

	// A name of the test's own, so the file it makes is known here.
	lv_textarea_set_text(name_field, "test-note");
	press_key(notes_screen, tr("create"));

	// The editor is open on the note that was just made, and the file is on
	// the card already -- empty, and listed.
	assert(lv_screen_active() == noteeditor_screen);
	lv_obj_t *field2 = find_class(noteeditor_screen, &lv_textarea_class);
	assert(field2 && strcmp(lv_textarea_get_text(field2), "") == 0);
	assert(find_label(noteeditor_screen, "test-note"));
	text = read_note(dir, "test-note.txt");
	assert(text && text[0] == '\0');
	free(text);

	// Typing something and pressing the corner check writes it. The check is
	// drawn rather than written, so it is found by its glyph.
	lv_textarea_set_text(field2, "bought at nine");
	lv_obj_t *save = find_image_button(noteeditor_screen, &icon_check);
	assert(save);
	lv_obj_send_event(save, LV_EVENT_CLICKED, NULL);
	text = read_note(dir, "test-note.txt");
	assert(text && strcmp(text, "bought at nine\n") == 0);
	free(text);
	assert(lv_screen_active() == noteeditor_screen);

	// Back to the list: the note made a moment ago is on it.
	notes_open();
	lv_obj_update_layout(notes_screen);
	check_within_panel(notes_screen, cfg, "Notes (with the new note)");
	dump_snapshot(notes_screen, tag_after);
	assert(find_label(notes_screen, "test-note"));
	assert(find_label(notes_screen, tr("notes_new")));
}

/* The button carrying a glyph: a corner control has an image on it, not a
 * caption, so it cannot be found by the text it shows. */
static lv_obj_t *find_image_button(lv_obj_t *obj, const lv_image_dsc_t *glyph) {
	if (lv_obj_check_type(obj, &lv_image_class) && lv_image_get_src(obj) == glyph) {
		lv_obj_t *parent = lv_obj_get_parent(obj);
		if (parent && lv_obj_check_type(parent, &lv_button_class)) {
			return parent;
		}
	}
	for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) {
		lv_obj_t *found = find_image_button(lv_obj_get_child(obj, i), glyph);
		if (found) {
			return found;
		}
	}
	return NULL;
}

int main(int argc, char **argv) {
	assert(argc == 2);

	/* The card, as a folder the test owns and throws away. */
	char card[] = "/tmp/sonix-tools-test-XXXXXX";
	assert(mkdtemp(card));
	char config[512];
	snprintf(config, sizeof(config), "%s/device_config.ini", card);
	config_init(config);
	config_set("ui", "language", "English");
	lang_init(argv[1]);
	check_strings(argv[1]);

	init_fonts();
	lv_init();
	gui_config_t cfg = {bp_screen_w(), bp_screen_h(), bp_padding(), bp_status_bar_h(), card};
	lv_display_create(cfg.screen_width, cfg.screen_height);
	theme_init();

	music_screen = lv_obj_create(NULL);
	streaming_screen = lv_obj_create(NULL);
	wireless_screen = lv_obj_create(NULL);
	audiobooks_screen = lv_obj_create(NULL);
	settings_screen = lv_obj_create(NULL);
	player_screen = lv_obj_create(NULL);
	ebookpage_screen = lv_obj_create(NULL);
	browser_screen = lv_obj_create(NULL);
	dacpage_screen = lv_obj_create(NULL);
	gearboypage_screen = lv_obj_create(NULL);
	morepage_screen = lv_obj_create(NULL);
	calculator_screen = lv_obj_create(NULL);
	notes_screen = lv_obj_create(NULL);
	noteeditor_screen = lv_obj_create(NULL);

	/* The notes the card already has, put there the way a computer would
	 * leave them: one of each extension the page knows, and a picture. */
	char dir[1024];
	assert(notes_dir(dir, sizeof(dir), card));
	assert(mkdir(dir, 0755) == 0);
	assert(notes_write(dir, "shopping", "milk and bread"));
	assert(notes_write(dir, "recipe.md", "# soup"));
	char path[1400];
	snprintf(path, sizeof(path), "%s/cover.jpg", dir);
	FILE *other = fopen(path, "wb");
	assert(other);
	assert(fputs("not a note", other) >= 0);
	assert(fclose(other) == 0);

	/* Built in the order gui_init() builds them: the editor before the shelf
	 * that opens it, both before the menu that points at them. */
	calculator_init(&cfg);
	noteeditor_init(&cfg);
	notes_init(&cfg);
	morepage_init(&cfg);

	/* Both shapes of the More page, each under the themes that use it. Dark
	 * and Light draw the tiles on every board; the V1's function-glyph themes
	 * draw the single-column list. */
	for (int kind = 0; kind < THEME_KIND_COUNT; kind++) {
		theme_set_kind((theme_kind_t)kind);
		const char *shape = (bp_is_tempotec_v1() && theme_uses_function_glyphs()) ? "rows" : "tiles";
		char tag[64];
		snprintf(tag, sizeof(tag), "%s-more-%s-%s", bp_is_tempotec_v1() ? "v1" : "hiby", shape, theme_tag());
		check_more_page(&cfg, tag);
	}
	theme_set_kind(THEME_KIND_DARK);

	check_calculator_page(&cfg, bp_is_tempotec_v1() ? "v1-calculator" : "hiby-calculator");
	check_notes_page(&cfg, dir, bp_is_tempotec_v1() ? "v1-notes" : "hiby-notes",
					 bp_is_tempotec_v1() ? "v1-note-editor" : "hiby-note-editor",
					 bp_is_tempotec_v1() ? "v1-notes-after" : "hiby-notes-after");

	lv_deinit();
	printf("PASS calculator and notes pages (%s)\n", bp_is_tempotec_v1() ? "240x320" : "480x720");
	return 0;
}
