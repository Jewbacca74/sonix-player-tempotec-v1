#include "aamlayout.h"

// The rows, top to bottom, and the picture between them.
//
// Everything that can be negative is held at something drawable rather than
// left to fall through: a panel smaller than the rows it is asked for is a
// mistake in the board profile, not a reason to hand a widget a negative size.
aam_box_t aam_layout_box(int screen_w, int screen_h, int status_h, int header_h, int strip_h, bool leftie) {
	aam_box_t b;
	if (status_h < 0) {
		status_h = 0;
	}
	if (header_h < 0) {
		header_h = 0;
	}
	if (strip_h < 0) {
		strip_h = 0;
	}

	b.status_h = status_h;
	b.header_h = header_h;
	b.strip_h = strip_h;
	b.art_x = leftie ? AAM_RAIL_W : 0;
	b.art_y = status_h + header_h;
	b.art_w = screen_w - b.art_x;
	if (b.art_w < 1) {
		b.art_w = 1;
	}
	// The picture takes what the two rows above it and the strip below leave.
	// A sleeve that would come back smaller than AAM_MIN_ART_H keeps that much
	// anyway: what it overruns is clipped by the panel, which reads as a full
	// picture rather than as a letterbox with nothing in it.
	b.art_h = screen_h - b.art_y - strip_h;
	if (b.art_h < AAM_MIN_ART_H) {
		b.art_h = AAM_MIN_ART_H;
	}
	return b;
}

// The three marks of the header row. The slot's centre has to sit half a glyph
// plus the end padding in from the edge for the *glyph* to keep that padding,
// and the slot is wider than the glyph, so the slot starts nearer the edge than
// the padding suggests.
void aam_head_slots(int screen_w, int out[3]) {
	(void)screen_w; // the row is measured from its right end, whatever its width
	int half = AAM_HEAD_SLOT / 2;
	for (int i = 0; i < 3; i++) {
		out[i] = AAM_HEAD_END_PAD + AAM_HEAD_GLYPH / 2 - half + i * AAM_HEAD_PITCH;
	}
}

int aam_head_marks_left(int screen_w) {
	int slots[3];
	aam_head_slots(screen_w, slots);
	return screen_w - (slots[2] + AAM_HEAD_SLOT);
}
