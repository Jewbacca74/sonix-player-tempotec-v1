// board_profile.h -- the dimensions of the TempoTec V1's 240x320 panel.
//
// One board. This fork targets the TempoTec Variations V1 and nothing else, so
// these helpers no longer describe a choice between two layouts: they are the
// V1's sizes, in one place, for the pages and the host simulator alike.

#ifndef SRC_GUI_BOARD_PROFILE_H_
#define SRC_GUI_BOARD_PROFILE_H_

#include <stdbool.h>

#include "lvgl/lvgl.h"

// Always true. Kept as a question because the pages that ask it read better
// than a comment would, and because the answer is now what was compiled
// rather than what the environment says: BOARD is not consulted.
bool bp_is_tempotec_v1(void);

int bp_screen_w(void);
int bp_screen_h(void);
int bp_padding(void);
int bp_status_bar_h(void);
int bp_tile_radius(void);

// Black quarter-circle masks on the system layer, matching the glass. 16 px on
// the V1 (about 2 mm of the 2" 240×320 IPS).
int bp_screen_corner_radius(void);

// Common compact-shell geometry. Callers which already have gui_config_t can
// key off screen_width; these helpers are for code built before the display
// exists (notably fonts_init()).
int bp_header_button_size(void);
int bp_header_button_gap(void);

// One geometry number per page element. The pairs were written for a 240-pixel
// V1 and a 480-pixel HiBy panel; only the compact half is reachable now, and
// the second argument is kept so the call sites still read as pairs of pixel
// sizes rather than as a bare number with a mysterious provenance.
int bp_pick(int compact, int regular);

// Fonts used by the most crowded player/list labels. fonts.c maps the named UI
// fonts to compact raster sizes for this panel, so pages which use font_ui_24
// directly do not keep 24 physical pixels on a 240-pixel display.
const lv_font_t *bp_title_font(void);
const lv_font_t *bp_artist_font(void);
const lv_font_t *bp_time_font(void);
const lv_font_t *bp_tile_label_font(void);
const lv_font_t *bp_queue_label_font(void);
const lv_font_t *bp_format_label_font(void);

#endif // SRC_GUI_BOARD_PROFILE_H_
