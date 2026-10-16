#ifndef GUI_CALCULATOR_CALCULATOR_H
#define GUI_CALCULATOR_CALCULATOR_H

#include "src/gui/shell/gui.h"

#include "lvgl/lvgl.h"

// The calculator, from the More page.
//
// A display and twenty keys, drawn from the board profile's own geometry so
// the same page fills a 240x320 V1 and a 480x720 player: the keys are the
// space left under the display, divided into the four columns and five rows
// every pocket calculator has.
//
// The arithmetic is in calcmodel.c, and the model outlives the page: leaving
// the calculator and coming back finds the sum where it was left.

extern lv_obj_t *calculator_screen;

void calculator_init(gui_config_t *cfg);

// Puts the calculator on screen. An action rather than a screen to switch to,
// for the reason the file manager's is: the display is repainted from the
// model as the page comes back.
void calculator_open(void);

#endif /* GUI_CALCULATOR_CALCULATOR_H */
