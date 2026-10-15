#ifndef EBOOKREADER_H
#define EBOOKREADER_H

#include "src/gui/shell/gui.h"

#include "lvgl/lvgl.h"

extern lv_obj_t *ebookreader_screen;

void ebookreader_init(gui_config_t *cfg);

// Opens `path` and shows it at wherever it was left off. The book stays open
// for as long as this screen is up and is closed -- arenas, ZIP and fonts --
// the moment it is left.
void ebookreader_open(const char *path);

// The same, but landing at a place somebody named instead of where the reading
// was left: what the bookmarks page opens a book with.
void ebookreader_open_at(const char *path, uint32_t spine, uint32_t block, uint32_t offset);

// Offers a hardware/media transport action to the open reader. Previous and
// Next turn a page (or move one screen in vertical mode); Play/Pause is left to
// the audio player, so somebody can still pause background music while reading.
// Returns true when the reader owns the action and it must not reach playback.
bool ebookreader_handle_key(gui_key_t key);

#endif /* EBOOKREADER_H */
