#ifndef AAMLAYOUT_H
#define AAMLAYOUT_H

#include <stdbool.h>

// The geometry of Album Art Max, on its own.
//
// Three rows and a picture: the shell's status bar, the header row under it
// (format, shuffle mode, menu, star), the sleeve, and the strip at the foot
// with the title and the place in the queue. Nothing in here knows about LVGL,
// fonts or widgets -- the caller passes in the heights it has measured, which
// is what lets the arrangement be worked out before anything has been laid out
// (the artwork is asked for at the size this gives, see reload_cover) and what
// lets the arithmetic be tested without a display.

// Leftie's rail, bar and head included: what the sleeve gives up in width when
// the scrub bar stands up along its left edge.
#define AAM_RAIL_W 12

// The sleeve never gets smaller than this, whatever the rows around it take:
// below it there is nothing left to look at, and the picture is the point.
#define AAM_MIN_ART_H 64

typedef struct {
	int status_h; // the shell's status bar, where the page begins
	int header_h; // the row of chrome under it
	int art_x, art_y, art_w, art_h;
	int strip_h; // the title and the queue position, at the foot
} aam_box_t;

// `screen_*` is the player's own size in pixels. `status_h`, `header_h` and
// `strip_h` are measured by the caller -- the board profile, the button size
// and the line height of the face in force. `leftie` stands the scrub bar up
// the left edge, which takes AAM_RAIL_W off the sleeve's width.
aam_box_t aam_layout_box(int screen_w, int screen_h, int status_h, int header_h, int strip_h, bool leftie);

#endif /* AAMLAYOUT_H */
