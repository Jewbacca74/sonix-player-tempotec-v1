// Board profile for the TempoTec V1.
//
// This fork builds for the TempoTec Variations V1 and for nothing else. The
// profile used to be a runtime choice -- BOARD=tempotec_v1 here, BOARD=hiby_r1
// or BOARD=hiby_r3proii there, a 480-pixel layout behind every compact/regular
// pair -- and it is not one any more: one board, one panel, one set of sizes.

#include "src/gui/board_profile.h"

#include "src/gui/fonts/fonts.h"

bool bp_is_tempotec_v1(void) {
	// The question is answered by what was compiled, not by the environment:
	// there is no other board this binary could be running on, so no variable
	// is read and no variable can turn the V1 layout off.
	return true;
}

int bp_screen_w(void) { return 240; }
int bp_screen_h(void) { return 320; }
int bp_padding(void) { return 6; }
int bp_status_bar_h(void) { return 24; }
int bp_tile_radius(void) { return 10; }
int bp_screen_corner_radius(void) { return 16; }
int bp_header_button_size(void) { return 36; }
int bp_header_button_gap(void) { return 4; }

int bp_pick(int compact, int regular) {
	(void)regular; // the 480-pixel sizes the pairs were written against
	return compact;
}

const lv_font_t *bp_title_font(void) { return &font_ui_16; }
const lv_font_t *bp_artist_font(void) { return &font_ui_14; }
const lv_font_t *bp_time_font(void) { return &font_ui_14; }
const lv_font_t *bp_tile_label_font(void) { return &font_ui_14; }
const lv_font_t *bp_queue_label_font(void) { return &font_ui_14; }
const lv_font_t *bp_format_label_font(void) { return &font_ui_14; }
