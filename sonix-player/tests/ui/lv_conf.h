/* The production widgets and layout, without any display/audio/font libraries. */
#include "../../lv_conf.h"

#undef LV_USE_FREETYPE
#define LV_USE_FREETYPE 0
#undef LV_USE_SDL
#define LV_USE_SDL 0
#undef LV_USE_LINUX_FBDEV
#define LV_USE_LINUX_FBDEV 0
#undef LV_USE_EVDEV
#define LV_USE_EVDEV 0

/* Supply deterministic font metrics instead of opening device font files. */
#undef LV_FONT_MONTSERRAT_14
#define LV_FONT_MONTSERRAT_14 1
