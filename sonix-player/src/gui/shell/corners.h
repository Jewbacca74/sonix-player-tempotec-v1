#ifndef CORNERS_H
#define CORNERS_H

#include <stdbool.h>

#include "src/gui/shell/gui.h"

// Rounded screen corners: four black quarter-circle masks, one in each corner
// of the display, on the system layer so they sit above every page, sheet,
// dialog and the screensaver alike. They take no touches.
//
// A setting on the Appearance page ("ui"/"rounded_corners", off by default).
// The arc is sized per board: 16 px on the TempoTec V1's 240×320 panel, which
// is the 2 mm of glass the 2" IPS actually rounds by, and 28 px on the 480-wide
// HiBy panels.

void corners_init(gui_config_t *cfg);

bool corners_enabled(void);
void corners_set_enabled(bool enabled);

#endif
