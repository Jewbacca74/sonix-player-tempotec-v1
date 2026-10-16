#ifndef GUI_NOTES_NOTES_H
#define GUI_NOTES_NOTES_H

#include "src/gui/shell/gui.h"

#include "lvgl/lvgl.h"

// The Notes page, from the More menu.
//
// It is the `Notes` folder of the card, listed: every text file in it, by
// name, with a row above them that makes a new one. Tapping a note opens it in
// the editor (noteeditor.c).
//
// The folder is read every time the page is entered, so a note written on a
// computer shows up on the player without a restart, and a note deleted there
// is gone from here too.

extern lv_obj_t *notes_screen;

void notes_init(gui_config_t *cfg);

// Reads the card and opens the page. An action rather than a screen to switch
// to, for the reason the file manager's is: what is on the card is read on the
// way in.
void notes_open(void);

#endif /* GUI_NOTES_NOTES_H */
