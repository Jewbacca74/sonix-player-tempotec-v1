/* Real LVGL, menus, appearance controls, themes, translations and persistence.
 * Only fonts and unrelated playback/navigation/board services are substituted. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "src/gui/board_profile.h"
#include "src/gui/fonts/fonts.h"
#include "src/gui/library/audiobooks.h"
#include "src/gui/library/music.h"
#include "src/gui/nowplaying/coverflow.h"
#include "src/gui/nowplaying/player.h"
#include "src/gui/settings/appearance.h"
#include "src/gui/settings/settings.h"
#include "src/gui/shell/gridpage.h"
#include "src/gui/shell/icons.h"
#include "src/gui/shell/main_menu.h"
#include "src/gui/shell/morepage.h"
#include "src/gui/shell/switcher.h"
#include "src/gui/shell/theme.h"
#include "src/gui/shell/toast.h"
#include "src/gui/shell/topbar.h"
#include "src/gui/streaming/streaming.h"
#include "src/gui/wireless/wireless.h"
#include "src/system/core/config.h"
#include "src/system/core/lang.h"
#include "src/system/device/bootlogo.h"
#include "src/system/playback/device_state.h"

lv_obj_t *music_screen, *streaming_screen, *wireless_screen;
lv_obj_t *audiobooks_screen, *morepage_screen, *settings_screen, *player_screen;

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
	/* Deliberately enlarge the footer enough to force the grid to reflow. */
	font_ui_14.line_height = size == FONTS_TEXT_LARGE ? 24 : 14;
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
void gui_notify_no_card(void) { assert(!"unexpected missing card"); }
void gui_notify_popup(const char *text) { (void)text; assert(!"unexpected popup"); }
void toast_error(const char *text) { (void)text; assert(!"unexpected error"); }
void topbar_set_battery_percent(bool shown) { (void)shown; }
void topbar_set_clock_position(int pos) { (void)pos; }
void switch_screen(lv_obj_t *screen) { lv_screen_load(screen); }
void switch_screen_cb(lv_event_t *e) { switch_screen(lv_event_get_user_data(e)); }
void switcher_attach_back_gesture(lv_obj_t *obj) { (void)obj; }
bool switcher_back_drag_active(void) { return false; }
void player_sheet_attach_drag(lv_obj_t *obj, bool opening) { (void)obj; (void)opening; }
bool player_sheet_drag_active(void) { return false; }
void coverflow_attach_edge(lv_obj_t *obj, gui_config_t *cfg) { (void)obj; (void)cfg; }
bool coverflow_drag_active(void) { return false; }

static lv_obj_t *child_of_type(lv_obj_t *obj, const lv_obj_class_t *type) {
	for (uint32_t i = 0; i < lv_obj_get_child_count(obj); i++) {
		lv_obj_t *child = lv_obj_get_child(obj, i);
		if (lv_obj_check_type(child, type) && !lv_obj_is_floating(child)) {
			return child;
		}
	}
	assert(!"missing child");
	return NULL;
}

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

static lv_obj_t *menu_toggle;
static void set_icons_only(bool enabled) {
	lv_obj_set_state(menu_toggle, LV_STATE_CHECKED, enabled);
	lv_obj_send_event(menu_toggle, LV_EVENT_VALUE_CHANGED, NULL);
	assert(main_menu_icons_only() == enabled);
}

static void check_grid(lv_obj_t *grid, bool icons_only) {
	lv_obj_update_layout(grid);
	for (uint32_t i = 0; i < lv_obj_get_child_count(grid); i++) {
		lv_obj_t *tile = lv_obj_get_child(grid, i);
		lv_obj_t *label = child_of_type(tile, &lv_label_class);
		assert(lv_obj_is_hidden(label) == icons_only);
		assert(lv_label_get_text(label)[0]);
		if (!icons_only) {
			continue;
		}
		lv_obj_t *icon = child_of_type(tile, &lv_image_class);
		int long_side = LV_MAX(lv_obj_get_width(icon), lv_obj_get_height(icon));
		int default_size = theme_uses_function_glyphs() ? bp_pick(22, 46) : bp_pick(46, 128);
		assert(long_side > default_size);
		lv_area_t t, image;
		lv_obj_get_coords(tile, &t);
		lv_obj_get_coords(icon, &image);
		assert(abs((t.x1 + t.x2) - (image.x1 + image.x2)) <= 2);
		assert(abs((t.y1 + t.y2) - (image.y1 + image.y2)) <= 2);
		assert(image.x1 >= t.x1 && image.x2 <= t.x2);
		assert(image.y1 >= t.y1 && image.y2 <= t.y2);
		/* A floating halo must follow the centered glyph after every reflow. */
		for (uint32_t j = 0; j < lv_obj_get_child_count(tile); j++) {
			lv_obj_t *halo = lv_obj_get_child(tile, j);
			if (!lv_obj_is_floating(halo) || lv_obj_is_hidden(halo)) {
				continue;
			}
			lv_area_t h;
			lv_obj_get_coords(halo, &h);
			assert(abs((h.x1 + h.x2) - (image.x1 + image.x2)) <= 2);
			assert(abs((h.y1 + h.y2) - (image.y1 + image.y2)) <= 2);
			assert(h.x1 >= t.x1 && h.x2 <= t.x2 && h.y1 >= t.y1 && h.y2 <= t.y2);
		}
	}
}

typedef struct {
	int tile_w, tile_h, icon_w, icon_h, icon_x, icon_y;
	uint32_t scale;
	const void *source;
} tile_metrics_t;
static tile_metrics_t tile_metrics(lv_obj_t *tile) {
	lv_obj_update_layout(tile);
	lv_obj_t *icon = child_of_type(tile, &lv_image_class);
	return (tile_metrics_t){lv_obj_get_width(tile), lv_obj_get_height(tile),
		lv_obj_get_width(icon), lv_obj_get_height(icon), lv_obj_get_x(icon), lv_obj_get_y(icon),
		lv_image_get_scale(icon), lv_image_get_src(icon)};
}
static void check_metrics(lv_obj_t *tile, tile_metrics_t expected) {
	tile_metrics_t now = tile_metrics(tile);
	assert(now.tile_w == expected.tile_w && now.tile_h == expected.tile_h);
	assert(now.icon_w == expected.icon_w && now.icon_h == expected.icon_h);
	assert(now.icon_x == expected.icon_x && now.icon_y == expected.icon_y);
	assert(now.scale == expected.scale && now.source == expected.source);
}

int main(int argc, char **argv) {
	assert(argc == 2 || argc == 3);
	bool saved_icons_only = argc == 3;
	char dir[] = "/tmp/sonix-menu-test-XXXXXX";
	assert(mkdtemp(dir));
	char config[256];
	snprintf(config, sizeof(config), "%s/device_config.ini", dir);
	config_init(config);
	assert(!main_menu_icons_only()); // no key: preserve the existing menu
	if (saved_icons_only) {
		config_set_bool("ui", "main_menu_icons_only", true);
		assert(config_save());
		assert(config_init(config));
	}
	config_set("ui", "language", "English");
	lang_init(argv[1]);
	assert(strcmp(tr("appearance_main_menu_icons_only"), "Main menu icons only") == 0);
	init_fonts();
	lv_init();
	gui_config_t cfg = {bp_screen_w(), bp_screen_h(), bp_padding(), bp_status_bar_h(), "/tmp"};
	lv_display_create(cfg.screen_width, cfg.screen_height);
	theme_init();
	music_screen = lv_obj_create(NULL);
	streaming_screen = lv_obj_create(NULL);
	wireless_screen = lv_obj_create(NULL);
	audiobooks_screen = lv_obj_create(NULL);
	morepage_screen = lv_obj_create(NULL);
	settings_screen = lv_obj_create(NULL);
	player_screen = lv_obj_create(NULL);
	main_menu_screen = lv_obj_create(NULL);
	main_menu_init(&cfg);
	lv_screen_load(main_menu_screen);
	lv_obj_t *grid = lv_obj_get_child(main_menu_screen, 0);
	check_grid(grid, saved_icons_only);

	appearance_screen = lv_obj_create(NULL);
	appearance_init(&cfg);
	lv_obj_t *toggle_label = find_label(appearance_screen, tr("appearance_main_menu_icons_only"));
	assert(toggle_label);
	menu_toggle = child_of_type(lv_obj_get_parent(toggle_label), &lv_switch_class);
	assert(lv_obj_has_state(menu_toggle, LV_STATE_CHECKED) == saved_icons_only);

	const grid_entry_t entries[] = {
		{"music_all_tracks", &icon_menu_music, &music_screen, NULL},
		{"albums", &icon_album, &music_screen, NULL},
	};
	lv_obj_t *section_grid = gridpage_build(music_screen, &cfg, entries, 2, 2, 3, true);
	gridpage_set_tile_label_hidden(NULL, 0, true);
	gridpage_set_tile_label_hidden(section_grid, -1, true);
	gridpage_set_tile_label_hidden(section_grid, 99, true);

	for (int kind = 0; kind < THEME_KIND_COUNT; kind++) {
		set_icons_only(false);
		theme_set_kind((theme_kind_t)kind);
		tile_metrics_t baseline[6];
		for (int i = 0; i < 6; i++) {
			baseline[i] = tile_metrics(lv_obj_get_child(grid, i));
		}
		tile_metrics_t section_before = tile_metrics(lv_obj_get_child(section_grid, 0));
		bool header_hidden = lv_obj_is_hidden(lv_obj_get_child(main_menu_screen, 1));
		bool footer_hidden = lv_obj_is_hidden(lv_obj_get_child(main_menu_screen, 3));

		set_icons_only(true);
		assert(config_init(config)); // a real save/reload, not just the in-memory flag
		assert(main_menu_icons_only());
		check_grid(grid, true);
		check_grid(section_grid, false);
		check_metrics(lv_obj_get_child(section_grid, 0), section_before);
		assert(lv_obj_is_hidden(lv_obj_get_child(main_menu_screen, 1)) == header_hidden);
		assert(lv_obj_is_hidden(lv_obj_get_child(main_menu_screen, 3)) == footer_hidden);

		theme_set_accent(3);
		theme_set_dynamic_tint(true);
		check_grid(grid, true);
		check_grid(section_grid, false);
		fonts_set_text_size(FONTS_TEXT_LARGE);
		check_grid(grid, true);
		fonts_set_text_size(FONTS_TEXT_NORMAL);
		check_grid(grid, true);

		set_icons_only(false);
		assert(config_init(config));
		assert(!main_menu_icons_only());
		check_grid(grid, false);
		for (int i = 0; i < 6; i++) {
			check_metrics(lv_obj_get_child(grid, i), baseline[i]);
		}
		/* Refresh callbacks must not undo the preference on a theme change. */
		set_icons_only(true);
		theme_set_kind((theme_kind_t)((kind + 1) % THEME_KIND_COUNT));
		check_grid(grid, true);
		check_grid(section_grid, false);
	}

	/* Hidden labels still translate, and all six destinations remain clickable. */
	assert(lang_set("Français"));
	check_grid(grid, true);
	lv_obj_t *music_label = child_of_type(lv_obj_get_child(grid, 0), &lv_label_class);
	assert(strcmp(lv_label_get_text(music_label), tr("music")) == 0);
	assert(lang_set("English"));
	assert(strcmp(lv_label_get_text(music_label), tr("music")) == 0);
	lv_obj_t *destinations[] = {music_screen, streaming_screen, wireless_screen,
		audiobooks_screen, morepage_screen, settings_screen};
	for (int i = 0; i < 6; i++) {
		lv_obj_send_event(lv_obj_get_child(grid, i), LV_EVENT_CLICKED, NULL);
		assert(lv_screen_active() == destinations[i]);
		lv_screen_load(main_menu_screen);
		check_grid(grid, true);
	}

	lv_deinit();
	unlink(config);
	rmdir(dir);
	printf("PASS main-menu icons-only (%s, %s)\n", bp_is_tempotec_v1() ? "240x320" : "480x720",
		saved_icons_only ? "saved setting" : "default setting");
	return 0;
}
