#ifndef CLOCK_H
#define CLOCK_H

#include <stdbool.h>
#include <time.h>

// Wall clock handling.
//
// The player cannot assume the system time means anything. The device may have
// come up with the clock at the epoch, so it can be set manually or corrected
// automatically over Wi-Fi. The player records a trustworthy time and writes
// it through to the RTC so the next boot starts from the right place.

// True once a time has been set (now or in an earlier session). Until then the
// status bar shows no clock rather than a wrong one.
bool clock_is_set(void);

// Reads the saved state and, if the system clock came up obviously wrong,
// restores the last time the player saw. Call once at startup, after the
// config has been loaded.
void clock_init(void);

// Applies a user-chosen local date and time: sets the system clock, writes it
// through to the RTC so it survives a power cycle, and records in the config
// that the clock is now trustworthy. Returns false if the system clock could
// not be set (which needs root).
bool clock_set(int year, int month, int day, int hour, int minute);

// Writes the current time into the config so a device with no working RTC
// starts somewhere sensible instead of at the epoch. Cheap and idempotent;
// call it on a slow timer.
void clock_remember(void);

// ---------------------------------------------------------------------------
// The time zone
//
// Chosen manually in Settings > Date and time > Time zone, or inferred
// automatically over Wi-Fi from the internet connection. The manual list uses
// fixed offsets with a separate daylight-saving choice; automatic mode uses
// the IANA zone rules when available.
// ---------------------------------------------------------------------------

bool clock_utc_offset_known(void);
int clock_utc_offset_minutes(void);

// Daylight saving time, kept as its own thing.
//
// The zones in the list are fixed offsets and must stay that way: Rome is +1,
// full stop, as on any map. DST is not another zone, it is one hour on top of
// the chosen one, and it has to be switched on and off twice a year without
// hunting for one's city on a different row of the list.
//
// So the two are kept apart: the list says where, this switch says when. What
// reaches the system is their sum.
bool clock_dst_enabled(void);
void clock_set_dst(bool on);

// Written by the time zone list, and can be forced from the configuration:
//   [clock]
//   utc_offset_minutes = 120
void clock_set_utc_offset_minutes(int minutes);

// Whether automatic date, time and time-zone updates are enabled. Enabled by
// default; turning it off leaves the manual controls available on Date and time.
bool clock_automatic_enabled(void);
void clock_set_automatic(bool on);

// Asks the network for the current time and, when automatic mode is enabled,
// the time zone inferred from the public IP address. Runs off the UI thread;
// Wi-Fi polls it while connected, and the worker rate-limits successful syncs
// to half an hour while retrying failures sooner. A successful time answer is
// written through to the RTC.
void clock_network_sync(void);
bool clock_network_sync_busy(void);
bool clock_network_sync_succeeded(void);

// Puts the system clock back where it belongs; returns true if it had to move
// it.
//
// Why this exists: CLOCK_REALTIME does not advance while the SoC is suspended
// to RAM, and this device suspends whenever it is left alone. Every suspend
// therefore loses wall-clock time -- "three minutes behind after twenty" is a
// handful of them adding up, not a crystal drifting. Two independent ways back
// to the truth, tried in order:
//
//   1. the RTC, which runs on its own power island and is written through by
//      clock_set(); and
//   2. CLOCK_BOOTTIME, which (unlike CLOCK_MONOTONIC) counts the time spent
//      suspended, so the elapsed real time can be reconstructed from the last
//      known-good pair even with no usable RTC.
//
// Call it after every resume, at every screen wake, and on a slow timer.
bool clock_resync(void);

// Writes the current time through to the RTC, the way the stock player does on
// its way out (decompile: "hwclock -w --utc" in its shutdown path). Call it
// before powering off or rebooting -- without it everything the clock learned
// since it was last set is lost at power-off.
void clock_shutdown(void);

// ---------------------------------------------------------------------------
// How the time is written
//
// One setting for the whole device: the status bar, the screensaver and the
// rollers on the Date and time page. On by default -- 24-hour is the format the
// rest of the interface is written in, and whoever wants AM/PM says so.
// ---------------------------------------------------------------------------

bool clock_use_24h(void);
void clock_set_use_24h(bool on); // stored in the configuration

// "14:05" or "2:05 PM", per the setting. `hour` is always the real hour, 0 to
// 23; the conversion happens here.
void clock_format_hm(char *out, size_t out_size, int hour, int minute);

#endif /* CLOCK_H */
