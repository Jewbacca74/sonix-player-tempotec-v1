#include "tagdate.h"

#include <string.h>

// How many of the three parts the tag spelled out. The basis of
// tagdate_better(), and of nothing else: the display formats itself from the
// fields that are there.
static int precision(const tagdate_t *d) {
	if (d->day > 0) {
		return 3;
	}
	if (d->month > 0) {
		return 2;
	}
	if (d->year > 0) {
		return 1;
	}
	return 0;
}

// Takes a year, month and day as the tag spelled them, drops the parts that do
// not name a real part of a date (month 13, day 0) and keeps the rest: the
// date a tag gives is still better than the one it does not, and a file dated
// "2026-13" is dated to its year however wrong the month is.
static void put_parts(tagdate_t *d, int year, int month, int day) {
	d->year = year >= 1 && year <= 9999 ? year : 0;
	d->month = d->year && month >= 1 && month <= 12 ? month : 0;
	d->day = d->month && day >= 1 && day <= 31 ? day : 0;
}

static int digit_at(const char *s, int count) {
	int value = 0;
	for (int i = 0; i < count; i++) {
		if (s[i] < '0' || s[i] > '9') {
			return -1;
		}
		value = value * 10 + (s[i] - '0');
	}
	return value;
}

// The ISO shape, from just after the four year digits: a separator and a
// month, then another separator and a day, each stopping at whatever follows
// (a time, a zone, the far end of an ID3v2.4 range, or the end of the text).
static void parse_iso_rest(tagdate_t *d, const char *p, int year) {
	int month = 0;
	int day = 0;
	if (*p == '-' || *p == '/') {
		// One or two digits for each part: "2026-3-2" is written as often as
		// "2026-03-02", and both are the same date.
		p++;
		int m = digit_at(p, 2);
		int take = 2;
		if (m < 0) {
			m = digit_at(p, 1);
			take = 1;
		}
		if (m >= 0) {
			month = m;
			p += take;
			if (*p == '-' || *p == '/') {
				p++;
				day = digit_at(p, 2);
				if (day < 0) {
					day = digit_at(p, 1);
				}
			}
		}
		// "2026-" or "2026-x": a year and a stray mark, nothing more.
	}
	put_parts(d, year, month, day);
}

bool tagdate_parse(const char *text, tagdate_t *out) {
	if (!text || !out) {
		return false;
	}
	memset(out, 0, sizeof(*out));
	while (*text == ' ' || *text == '\t') {
		text++;
	}

	// Digits first, and how many of them, before anything is cut apart: the
	// two shapes differ only in whether separators interrupt them.
	int digits = 0;
	while (text[digits] >= '0' && text[digits] <= '9') {
		digits++;
	}

	if (digits == 8) {
		// The compact shape, "20260312". Only whole -- "202603121" is not a
		// date -- and never the opening of the ISO one, whose separators stop
		// the count at four.
		int year = digit_at(text, 4);
		int month = digit_at(text + 4, 2);
		int day = digit_at(text + 6, 2);
		put_parts(out, year, month, day);
		return out->year > 0;
	}

	if (digits == 4) {
		int year = digit_at(text, 4);
		parse_iso_rest(out, text + 4, year);
		return out->year > 0;
	}

	return false;
}

bool tagdate_better(const tagdate_t *a, const tagdate_t *b) {
	return precision(a ? a : &(tagdate_t){0}) > precision(b ? b : &(tagdate_t){0});
}

time_t tagdate_to_time(const tagdate_t *d) {
	if (!d || d->year <= 0) {
		return 0;
	}
	struct tm tm_date;
	memset(&tm_date, 0, sizeof(tm_date));
	tm_date.tm_year = d->year - 1900;
	tm_date.tm_mon = d->month > 0 ? d->month - 1 : 0;
	tm_date.tm_mday = d->day > 0 ? d->day : 1;
	tm_date.tm_isdst = -1;
	time_t when = mktime(&tm_date);
	return when == (time_t)-1 ? 0 : when;
}
