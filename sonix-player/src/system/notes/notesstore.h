#ifndef SYSTEM_NOTES_NOTESSTORE_H
#define SYSTEM_NOTES_NOTESSTORE_H

#include <stdbool.h>
#include <stddef.h>

// The notes on the card: one folder of text files, read, written and listed.
//
// Plain C and no LVGL, so the folder rules and the arithmetic of "the next
// free name" can be tested without a screen (tests/test_notes.c). The page
// above this only draws what it returns.

// Where the notes live: `<card>/Notes`. A folder of the user's own, beside
// Audiobooks, Ebook, Games and PEQ, rather than anything the player creates.
#define NOTES_FOLDER "Notes"

// The longest note, in bytes, that the editor will read back. A note typed on
// a device keyboard is a few hundred bytes; this is the ceiling of what a file
// dropped into the folder can be before the page says it is too long.
#define NOTES_MAX_BYTES (64 * 1024)

// What the editor will hold, in characters. A character is at most four bytes
// of UTF-8, so this is the byte ceiling above without the editor having to
// count bytes on every key.
#define NOTES_MAX_CHARS (NOTES_MAX_BYTES / 4)

// Room for a name on a 240 px list. Longer names are still listed -- they are
// cut to fit, which is all a row could ever show of them.
#define NOTES_NAME_MAX 96
#define NOTES_MAX_ENTRIES 200

typedef struct {
	char name[NOTES_NAME_MAX + 1];
} note_entry_t;

// `<card_root>/Notes`, and `<dir>/<name>`. False when the result would not fit.
bool notes_dir(char *out, size_t size, const char *card_root);
bool notes_path(char *out, size_t size, const char *dir, const char *name);

// Whether a name is a note: a text file, not hidden. The extensions are the
// ones the file manager already opens as text, plus Markdown -- a note written
// elsewhere as .md is a note here too. Everything else in the folder -- a
// cover picture, a leftover .tmp from a write that was interrupted -- is left
// alone rather than offered to the editor to overwrite.
bool notes_is_note_name(const char *name);

// The folder's notes, by name, A-Z and case-insensitively, with a run of
// digits read as a number: note-2 comes before note-10. Up to `max` entries
// are written; the count is returned, or 0 when the folder is not there.
int notes_list(const char *dir, note_entry_t *out, int max);

// The note's text, decoded to UTF-8 (see src/system/text/textfile.h), or NULL
// when the file cannot be read or is too long. `*truncated` distinguishes the
// two: the text is still returned up to NOTES_MAX_BYTES.
char *notes_read(const char *path, bool *truncated);

// Whether the editor can hold `text` -- NOTES_MAX_CHARS characters, counting
// UTF-8 sequences rather than bytes. A note longer than that is opened to be
// read and not to be written: the writing would stop at the limit and the next
// save would cut the rest of the file off.
bool notes_fits_editor(const char *text);

// The file a name is written as. "list" becomes "list.txt"; a name that
// already ends in an extension the page knows is left alone. The dialog asks
// this before it writes, so that the note it opens afterwards is the note it
// has just made.
bool notes_file_name(const char *name, char *out, size_t size);

// `text` written into `dir` as `name`, as UTF-8. The folder is made if it is
// not there, which is what lets the first note be written on a card that has
// never had one.
bool notes_write(const char *dir, const char *name, const char *text);

// Whether a name can be written at all: something other than blank, with no
// path separator and no control character in it. The name of the file, without
// the .txt the writer adds if the user did not.
bool notes_name_ok(const char *name);

// A name as a list or a title should show it: the note's extension taken off,
// since every row of the page is a text file and the .txt is noise on a 240 px
// line. The name on the card is untouched.
void notes_display_name(const char *name, char *out, size_t size);

// A name for a new note that is free: the date, or the date and a number. What
// the new-note dialog opens with, so the fastest way to write something down
// does not begin with typing a name.
void notes_default_name(const char *dir, char *out, size_t size);

#endif /* SYSTEM_NOTES_NOTESSTORE_H */
