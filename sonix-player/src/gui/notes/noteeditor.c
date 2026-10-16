#include "noteeditor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lvgl/lvgl.h"

#include "src/gui/board_profile.h"
#include "src/gui/fonts/fonts.h"
#include "src/gui/shell/icons.h"
#include "src/gui/shell/keyboard.h"
#include "src/gui/shell/settingsrow.h"
#include "src/gui/shell/switcher.h"
#include "src/gui/shell/theme.h"
#include "src/gui/shell/toast.h"
#include "src/system/core/lang.h"
#include "src/system/notes/notesstore.h"

lv_obj_t *noteeditor_screen;

// How long the writing has to have stopped before the page writes the note by
// itself. Long enough that a pause between words does not write, short enough
// that a power-off in the middle of a sentence costs one.
#define AUTOSAVE_MS 2000

static lv_obj_t *title_label;
static lv_obj_t *field;
static lv_timer_t *autosave_timer;

static char dir[1024];
static char name[NOTES_NAME_MAX + 1];
static bool dirty;        // typed into since the last write
static char idle_text[64]; // the end of the text as of the last tick

static void write_note(void) {
	if (!dirty) {
		return;
	}
	if (!notes_write(dir, name, lv_textarea_get_text(field))) {
		// Still dirty: the next pause, the check key or leaving the page will
		// try again rather than the text being quietly dropped.
		return;
	}
	dirty = false;
}

static void save_cb(lv_event_t *e) {
	(void)e;
	if (dirty && notes_write(dir, name, lv_textarea_get_text(field))) {
		dirty = false;
		toast_plain(tr("notes_saved"));
		return;
	}
	toast_plain(tr(dirty ? "notes_cannot_save" : "notes_saved"));
}

// A new line, from the keyboard's accent key. A note is a list of lines as
// often as it is a paragraph, and this is the only page that has to be able to
// start one.
static void newline_cb(lv_event_t *e) {
	lv_obj_t *target = lv_event_get_user_data(e);
	if (target) {
		lv_textarea_add_text(target, "\n");
	}
}

static void value_changed_cb(lv_event_t *e) {
	(void)e;
	dirty = true;
}

// A few characters off the end of the text, which is enough to tell one tick's
// text from the next one's without holding a second copy of the note.
static void text_tail(const char *text, char *out, size_t size) {
	size_t len = strlen(text);
	if (len > size - 1) {
		len = size - 1;
	}
	memcpy(out, text + strlen(text) - len, len);
	out[len] = '\0';
}

static void autosave_cb(lv_timer_t *timer) {
	(void)timer;
	if (!dirty) {
		return;
	}
	// Only once the typing has stopped: the timer runs all the while, and
	// writing on every tick would put a card write between every word.
	char tail[64];
	text_tail(lv_textarea_get_text(field), tail, sizeof(tail));
	if (strcmp(tail, idle_text) == 0) {
		write_note();
		return;
	}
	memcpy(idle_text, tail, sizeof(idle_text));
}

static void unloaded_cb(lv_event_t *e) {
	(void)e;
	write_note();
}

// The title row says which note this is, the way the text viewer says which
// file it is showing.
static void title_set(const char *note_name) {
	char shown[NOTES_NAME_MAX + 1];
	notes_display_name(note_name, shown, sizeof(shown));
	lv_label_set_text(title_label, shown);
}

static bool open_with(const char *note_dir, const char *note_name, const char *text) {
	if (!note_dir || !note_dir[0] || !note_name || !note_name[0]) {
		return false;
	}
	snprintf(dir, sizeof(dir), "%s", note_dir);
	snprintf(name, sizeof(name), "%s", note_name);
	title_set(name);

	lv_textarea_set_text(field, text ? text : "");
	// The caret starts at the top of a freshly opened note rather than at the
	// end of it: what is being done is usually reading it again.
	lv_obj_scroll_to_y(field, 0, LV_ANIM_OFF);
	dirty = false;
	idle_text[0] = '\0';
	lv_timer_resume(autosave_timer);
	lv_timer_reset(autosave_timer);

	switch_screen(noteeditor_screen);
	return true;
}

bool noteeditor_open(const char *note_dir, const char *note_name) {
	bool truncated = false;
	char path[1200];
	if (!notes_path(path, sizeof(path), note_dir, note_name)) {
		return false;
	}

	char *text = notes_read(path, &truncated);
	if (!text) {
		gui_notify_popup(tr("notes_cannot_open"));
		return false;
	}
	if (truncated) {
		// Longer than the editor holds. The file manager's text viewer shows
		// the beginning of it; writing here would cut the rest of it off.
		free(text);
		gui_notify_popup(tr("notes_too_long"));
		return false;
	}

	bool ok = open_with(note_dir, note_name, text);
	free(text);
	return ok;
}

bool noteeditor_new(const char *note_dir, const char *note_name) {
	return open_with(note_dir, note_name, "");
}

void noteeditor_init(gui_config_t *cfg) {
	lv_obj_add_style(noteeditor_screen, &theme_style_screen, 0);
	title_label = settingsrow_title(noteeditor_screen, cfg, "");
	settingsrow_title_corner_slots(title_label, cfg, 1);

	// The keyboard takes the bottom of the page and the checking key the
	// corner above it; the text gets everything left.
	const int keyboard_h = cfg->screen_width < 320 ? 144 : 316;
	const int content_top = settingsrow_content_top(cfg);
	const int field_h = cfg->screen_height - content_top - keyboard_h - cfg->padding;

	field = lv_textarea_create(noteeditor_screen);
	lv_textarea_set_one_line(field, false);
	lv_textarea_set_max_length(field, NOTES_MAX_CHARS);
	lv_textarea_set_placeholder_text(field, tr("notes_placeholder"));
	lv_obj_set_size(field, cfg->screen_width - 2 * cfg->padding, field_h);
	lv_obj_align(field, LV_ALIGN_TOP_LEFT, cfg->padding, content_top);
	lv_obj_add_style(field, &theme_style_card, 0);
	lv_obj_set_style_radius(field, bp_pick(10, 16), 0);
	lv_obj_set_style_border_width(field, 0, 0);
	lv_obj_set_style_shadow_width(field, 0, 0);
	lv_obj_set_style_pad_all(field, bp_pick(8, 14), 0);
	lv_obj_set_style_text_font(field, &font_ui_22, 0);
	lv_obj_set_scrollbar_mode(field, LV_SCROLLBAR_MODE_AUTO);
	keyboard_style_caret(field);
	lv_obj_add_event_cb(field, value_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

	lv_obj_t *save = lv_btn_create(noteeditor_screen);
	settingsrow_place_corner_button(save, cfg, 0);
	lv_obj_set_style_bg_opa(save, LV_OPA_TRANSP, 0);
	lv_obj_set_style_border_width(save, 0, 0);
	lv_obj_set_style_shadow_width(save, 0, 0);
	lv_obj_set_style_pad_all(save, 0, 0);
	lv_obj_add_event_cb(save, save_cb, LV_EVENT_CLICKED, NULL);

	lv_obj_t *save_glyph = lv_image_create(save);
	lv_image_set_src(save_glyph, &icon_check);
	lv_obj_add_style(save_glyph, &theme_style_icon, 0);
	settingsrow_scale_corner_icon(save_glyph, cfg);
	lv_obj_center(save_glyph);

	// The accent key, where a keyboard has its return key, starts a line.
	keyboard_create(noteeditor_screen, cfg->screen_width, keyboard_h, field, &icon_enter, NULL, newline_cb, field);

	lv_obj_add_event_cb(noteeditor_screen, unloaded_cb, LV_EVENT_SCREEN_UNLOADED, NULL);

	// Paused until a note is opened: the page is built once, at startup, and a
	// timer that ran from then on would be one more thing every page pays for.
	autosave_timer = lv_timer_create(autosave_cb, AUTOSAVE_MS, NULL);
	lv_timer_pause(autosave_timer);
}
