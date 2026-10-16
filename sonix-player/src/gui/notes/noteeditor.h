#ifndef GUI_NOTES_NOTEEDITOR_H
#define GUI_NOTES_NOTEEDITOR_H

#include "src/gui/shell/gui.h"

#include "lvgl/lvgl.h"

// One note, opened for writing.
//
// The page is a text area with the shared keyboard under it, the note's name
// in the title row and a check in the corner that saves it. The keyboard's
// accent key -- the wide one at the bottom right, where a keyboard has its
// return key -- writes a new line, because this is the one page in the player
// where a line break is part of what is being typed.
//
// Nothing here is the only copy: the note is written when the check is
// pressed, when the page is left, and a couple of seconds after the writing
// stops. Leaving the page with the chevron does not ask whether to keep the
// text, because the answer is always yes.

extern lv_obj_t *noteeditor_screen;

void noteeditor_init(gui_config_t *cfg);

// Opens a note that is on the card. False -- with a notice, and staying where
// it is -- when the file cannot be read or is too long to edit here.
bool noteeditor_open(const char *dir, const char *name);

// Opens a name that is not on the card yet: the page starts empty and the file
// is written the first time it is saved.
bool noteeditor_new(const char *dir, const char *name);

#endif /* GUI_NOTES_NOTEEDITOR_H */
