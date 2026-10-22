/*
 * The rows of Album Art Max, on their own.
 *
 * The arrangement is three rows and a picture: the status bar, the header row,
 * the sleeve, the strip. Nothing here is LVGL -- aam_layout_box() takes the
 * heights the caller measured and answers with the box -- so what is checked
 * is the arithmetic that decides how much of a 240x320 panel the artwork gets,
 * in both of its shapes.
 *
 * The numbers are the V1's: a 24 px status bar, a 40 px header row (the taller
 * of AAM_HEADER_H and the board's 36 px corner button) and a strip one 15 px
 * line of title plus its padding (2 + 15 + 2 + 2 = 21).
 */
#include <stdio.h>
#include <stdlib.h>

#include "src/gui/nowplaying/aamlayout.h"

static int failures;

#define CHECK(cond)                                                                                                    \
	do {                                                                                                               \
		if (!(cond)) {                                                                                                 \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                            \
			failures++;                                                                                                \
		}                                                                                                              \
	} while (0)

#define V1_W 240
#define V1_H 320
#define V1_STATUS 24
#define V1_HEADER 40
#define V1_STRIP 21

static void test_the_panel_is_shared_out(void) {
	aam_box_t b = aam_layout_box(V1_W, V1_H, V1_STATUS, V1_HEADER, V1_STRIP, false);

	// The page starts under the status bar and the header row sits on it.
	CHECK(b.status_h == V1_STATUS);
	CHECK(b.header_h == V1_HEADER);
	CHECK(b.art_y == V1_STATUS + V1_HEADER);

	// The sleeve is the whole width and everything the strip leaves: the rows
	// and the picture add up to the panel exactly, with nothing lost between
	// them and nothing overlapping.
	CHECK(b.art_x == 0);
	CHECK(b.art_w == V1_W);
	CHECK(b.art_h == V1_H - V1_STATUS - V1_HEADER - V1_STRIP);
	CHECK(b.art_y + b.art_h + b.strip_h == V1_H);

	// Which, on the V1, is the 235 x 235 the concept draws: a square by luck
	// of the numbers rather than by design, and large enough to be the point of
	// the arrangement.
	CHECK(b.art_w == 240);
	CHECK(b.art_h == 235);
}

static void test_leftie_takes_its_width_off_the_sleeve(void) {
	aam_box_t flat = aam_layout_box(V1_W, V1_H, V1_STATUS, V1_HEADER, V1_STRIP, false);
	aam_box_t left = aam_layout_box(V1_W, V1_H, V1_STATUS, V1_HEADER, V1_STRIP, true);

	// The rail stands where the picture used to begin, so the picture starts
	// after it and is exactly that much narrower. Nothing else moves: the
	// rows above and below are the same height and the foot is in the same
	// place, so the strip does not drift when the bar changes shape.
	CHECK(left.art_x == AAM_RAIL_W);
	CHECK(left.art_w == flat.art_w - AAM_RAIL_W);
	CHECK(left.art_y == flat.art_y);
	CHECK(left.art_h == flat.art_h);
	CHECK(left.strip_h == flat.strip_h);
	CHECK(left.art_x + left.art_w == V1_W);

	// A few pixels, which is the whole claim the shape makes: the picture is
	// not the thing being traded away here.
	CHECK(AAM_RAIL_W * 100 / V1_W <= 5);
}

static void test_the_sleeve_has_a_floor(void) {
	// A panel shorter than its own rows: the picture keeps AAM_MIN_ART_H and
	// the overrun is clipped, rather than the caller being handed a negative
	// height to pass on to a widget.
	aam_box_t b = aam_layout_box(V1_W, 100, V1_STATUS, V1_HEADER, V1_STRIP, false);
	CHECK(b.art_h == AAM_MIN_ART_H);

	// Degenerate rows are held at zero rather than dragging the picture up
	// over the status bar.
	aam_box_t z = aam_layout_box(V1_W, V1_H, -5, -5, -5, false);
	CHECK(z.status_h == 0);
	CHECK(z.header_h == 0);
	CHECK(z.strip_h == 0);
	CHECK(z.art_y == 0);
	CHECK(z.art_h == V1_H);

	// And a panel too narrow for Leftie's rail still answers with a picture.
	aam_box_t n = aam_layout_box(4, V1_H, V1_STATUS, V1_HEADER, V1_STRIP, true);
	CHECK(n.art_w >= 1);
}

static void test_the_taller_header_wins(void) {
	// AAM_HEADER_H is a floor and not a fixed height: a board whose corner
	// buttons are taller than 40 px hands over its own measurement, and the
	// artwork moves down by exactly that much rather than being drawn over.
	aam_box_t a = aam_layout_box(V1_W, V1_H, V1_STATUS, 40, V1_STRIP, false);
	aam_box_t b = aam_layout_box(V1_W, V1_H, V1_STATUS, 56, V1_STRIP, false);
	CHECK(b.art_y == a.art_y + 16);
	CHECK(b.art_h == a.art_h - 16);
	CHECK(b.art_y + b.art_h + b.strip_h == V1_H);
}

int main(void) {
	test_the_panel_is_shared_out();
	test_leftie_takes_its_width_off_the_sleeve();
	test_the_sleeve_has_a_floor();
	test_the_taller_header_wins();

	if (failures) {
		fprintf(stderr, "aam layout: %d check(s) failed\n", failures);
		return EXIT_FAILURE;
	}
	printf("PASS aam layout\n");
	return EXIT_SUCCESS;
}
