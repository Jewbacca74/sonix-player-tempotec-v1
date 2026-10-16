#include "notesstore.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "src/system/text/textfile.h"

// The extensions that are notes, which is the set the file manager already
// opens as text plus Markdown.
static const char *const NOTE_EXTENSIONS[] = {"txt", "md", "lrc", "nfo", "ini", NULL};

static bool name_ends_with_name(const char *name, size_t len) {
	for (int i = 0; NOTE_EXTENSIONS[i]; i++) {
		size_t ext_len = strlen(NOTE_EXTENSIONS[i]);
		if (len == ext_len) {
			continue; // a file called ".txt" is hidden, not a note
		}
		if (len > ext_len + 1 && name[len - ext_len - 1] == '.' &&
			strcasecmp(name + len - ext_len, NOTE_EXTENSIONS[i]) == 0) {
			return true;
		}
	}
	return false;
}

bool notes_is_note_name(const char *name) {
	if (!name || !name[0] || name[0] == '.') {
		return false;
	}
	size_t len = strlen(name);
	if (len > NOTES_NAME_MAX) {
		return false; // more than the row or the store can hold
	}
	return name_ends_with_name(name, len);
}

bool notes_dir(char *out, size_t size, const char *card_root) {
	if (!out || !size || !card_root || !card_root[0]) {
		return false;
	}
	int n = snprintf(out, size, "%s/%s", card_root, NOTES_FOLDER);
	return n > 0 && (size_t)n < size;
}

bool notes_path(char *out, size_t size, const char *dir, const char *name) {
	if (!out || !size || !dir || !dir[0] || !name || !name[0]) {
		return false;
	}
	int n = snprintf(out, size, "%s/%s", dir, name);
	return n > 0 && (size_t)n < size;
}

// ---------------------------------------------------------------------------
// the listing
// ---------------------------------------------------------------------------

// A-Z, case-insensitively, with a run of digits compared as the number it is:
// a folder of dated notes reads in the order they were written rather than
// note-1, note-10, note-2.
static int compare_names(const char *a, const char *b) {
	while (*a && *b) {
		if (isdigit((unsigned char)*a) && isdigit((unsigned char)*b)) {
			char *end_a = NULL;
			char *end_b = NULL;
			unsigned long long num_a = strtoull(a, &end_a, 10);
			unsigned long long num_b = strtoull(b, &end_b, 10);
			if (num_a != num_b) {
				return num_a < num_b ? -1 : 1;
			}
			// Equal numbers written differently ("07" against "7") keep going:
			// the rest of the name decides, as it does for a letter.
			a = end_a;
			b = end_b;
			continue;
		}
		int la = tolower((unsigned char)*a);
		int lb = tolower((unsigned char)*b);
		if (la != lb) {
			return la < lb ? -1 : 1;
		}
		a++;
		b++;
	}
	return *a ? 1 : *b ? -1 : 0;
}

static int compare_entries(const void *left, const void *right) {
	return compare_names(((const note_entry_t *)left)->name, ((const note_entry_t *)right)->name);
}

int notes_list(const char *dir, note_entry_t *out, int max) {
	if (!dir || !out || max <= 0) {
		return 0;
	}
	DIR *handle = opendir(dir);
	if (!handle) {
		return 0;
	}

	int count = 0;
	struct dirent *entry;
	while (count < max && (entry = readdir(handle)) != NULL) {
		if (!notes_is_note_name(entry->d_name)) {
			continue;
		}

		// A directory called "list.txt" is not a note, and offering it to the
		// editor would be offering to write over it.
		char path[1024];
		if (!notes_path(path, sizeof(path), dir, entry->d_name)) {
			continue;
		}
		struct stat st;
		if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
			continue;
		}

		// notes_is_note_name() refused anything longer than a row can hold, so
		// this copy fits. It is written as a length rather than as a %s
		// because a %s reads to the compiler as an unbounded one, and a
		// name off a card is a name somebody else chose.
		size_t name_len = strlen(entry->d_name);
		if (name_len > NOTES_NAME_MAX) {
			continue;
		}
		memcpy(out[count].name, entry->d_name, name_len + 1);
		count++;
	}
	closedir(handle);

	qsort(out, (size_t)count, sizeof(out[0]), compare_entries);
	return count;
}

// ---------------------------------------------------------------------------
// reading and writing one
// ---------------------------------------------------------------------------

char *notes_read(const char *path, bool *truncated) {
	if (truncated) {
		*truncated = false;
	}
	unsigned char *raw = NULL;
	size_t bytes = 0;
	bool more = false;
	if (!textfile_read(path, NOTES_MAX_BYTES, &raw, &bytes, &more)) {
		return NULL;
	}
	char *text = textfile_decode(raw, bytes);
	free(raw);
	if (truncated) {
		*truncated = more;
	}
	return text;
}

static bool make_dir(const char *path) {
	struct stat st;
	if (mkdir(path, 0777) == 0) {
		return true;
	}
	return errno == EEXIST && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

// The file a name is written as. "list" becomes "list.txt", "list.md" stays
// the Markdown file it says it is, and a name that carries a dot but no
// extension the page knows -- a date typed as "2026.10.16", a name copied
// from somewhere else -- keeps it and gets ".txt" after it, rather than being
// refused for a reason the user cannot see.
bool notes_file_name(const char *name, char *out, size_t size) {
	if (!name || !out || !size) {
		return false;
	}
	size_t len = strlen(name);
	int n = name_ends_with_name(name, len) ? snprintf(out, size, "%s", name) : snprintf(out, size, "%s.txt", name);
	return n > 0 && (size_t)n < size;
}

bool notes_write(const char *dir, const char *name, const char *text) {
	if (!dir || !dir[0] || !notes_name_ok(name)) {
		return false;
	}
	if (!make_dir(dir)) {
		return false;
	}

	char file[NOTES_NAME_MAX + 8];
	if (!notes_file_name(name, file, sizeof(file))) {
		return false;
	}
	if (!notes_is_note_name(file)) {
		return false;
	}

	char path[1024];
	if (!notes_path(path, sizeof(path), dir, file)) {
		return false;
	}
	return textfile_write(path, text ? text : "");
}

void notes_display_name(const char *name, char *out, size_t size) {
	if (!out || !size) {
		return;
	}
	snprintf(out, size, "%s", name ? name : "");
	if (!name) {
		return;
	}
	size_t len = strlen(name);
	if (!name_ends_with_name(name, len)) {
		return;
	}
	char *dot = strrchr(out, '.');
	if (dot && dot != out) {
		*dot = '\0';
	}
}

bool notes_name_ok(const char *name) {
	if (!name || name[0] == '.') {
		// A leading dot is refused on purpose: the file is written as
		// "<name>.txt", and a hidden file is one the notes list would never
		// show back to the person who wrote it.
		return false;
	}
	size_t len = 0;
	for (const char *p = name; *p; p++, len++) {
		if (*p == '/' || *p == '\\' || (unsigned char)*p < 0x20) {
			return false;
		}
	}
	// Blank, or nothing but spaces: a file whose name a list cannot show.
	for (const char *p = name; *p; p++) {
		if (!isspace((unsigned char)*p)) {
			return len <= NOTES_NAME_MAX;
		}
	}
	return false;
}

void notes_default_name(const char *dir, char *out, size_t size) {
	if (!out || !size) {
		return;
	}
	char stamp[32] = "";
	time_t now = time(NULL);
	struct tm local;
	if (localtime_r(&now, &local)) {
		strftime(stamp, sizeof(stamp), "%Y-%m-%d", &local);
	}

	// The date, then the date and a number: a second note written the same day
	// does not land on top of the first.
	for (int extra = 0; extra < 100; extra++) {
		if (stamp[0] == '\0') {
			snprintf(out, size, extra == 0 ? "note" : "note-%d", extra + 1);
		} else if (extra == 0) {
			snprintf(out, size, "%s", stamp);
		} else {
			snprintf(out, size, "%s-%d", stamp, extra + 1);
		}

		// Asked about the file the name would be written as, not the bare
		// name the dialog shows: a note is stored as "<name>.txt", so
		// "2026-10-16" is taken exactly when 2026-10-16.txt is on the card.
		char file[NOTES_NAME_MAX + 8];
		char path[1024];
		struct stat st;
		if (!dir || !notes_file_name(out, file, sizeof(file)) ||
			!notes_path(path, sizeof(path), dir, file) || stat(path, &st) != 0) {
			return; // free, or the folder cannot be asked -- either way the name stands
		}
	}

	// A hundred notes under one name: the caller's dialog still gets something
	// it can write.
	snprintf(out, size, "%s", stamp[0] ? stamp : "note");
}
