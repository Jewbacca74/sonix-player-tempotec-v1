// Headless checks of the dates tags carry: the shapes they come in, the
// precision they keep, and the moment they resolve to for sorting.
//
// A date tag is written by whoever wrote the file, and every tagger has its
// own idea of the shape: a bare year, a year and a month, an ISO date, the
// same with slashes, the compact "20260312", any of them carrying a time and
// a zone after it. The podcast lists read the date to order episodes and to
// write "12 March 2026" under each one, and a date misread here shows as a
// row in the wrong place under the wrong date. Plain C, no LVGL.

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "src/system/library/tagdate.h"

static void check(const char *text, int year, int month, int day) {
	tagdate_t d;
	assert(tagdate_parse(text, &d));
	assert(d.year == year);
	assert(d.month == month);
	assert(d.day == day);
	printf("  %-28s -> %04d-%02d-%02d\n", text, year, month > 0 ? month : 1, day > 0 ? day : 1);
}

static void check_rejected(const char *text) {
	tagdate_t d;
	assert(!tagdate_parse(text, &d));
	assert(d.year == 0 && d.month == 0 && d.day == 0);
	printf("  %-28s -> (no date)\n", text);
}

int main(void) {
	printf("tag dates\n");

	// What is missing stays missing: a tag that says only a year is not
	// silently the first of January.
	check("2026", 2026, 0, 0);
	check("2026-03", 2026, 3, 0);
	check("2026-03-12", 2026, 3, 12);

	// The same date in the shapes the taggers write.
	check("2026-3-2", 2026, 3, 2);
	check("2026/03/12", 2026, 3, 12);
	check("20260312", 2026, 3, 12);
	check("  2026-03-12  ", 2026, 3, 12);

	// A time, a zone, and the far end of an ID3v2.4 range follow the date
	// without being read into it.
	check("2026-03-12T08:00:00", 2026, 3, 12);
	check("2026-03-12T08:00:00Z", 2026, 3, 12);
	check("2026-03-12T08:00:00+02:00", 2026, 3, 12);
	check("2026-03-12 08:00", 2026, 3, 12);
	check("2026-03-12/2026-03-13", 2026, 3, 12);
	check("20260312T080000", 2026, 3, 12);

	// Parts that name no real month or day drop to whatever the tag did say.
	check("2026-13", 2026, 0, 0);
	check("2026-00-12", 2026, 0, 0);
	check("2026-02-99", 2026, 2, 0);
	check("20261345", 2026, 0, 0);
	check("2026-3", 2026, 3, 0);
	// And what follows the date is ignored, so a missing separator is not two
	// dates running together but a month and a stray mark.
	check("2026-0312", 2026, 3, 0);

	// Texts with no date in them, or dates in shapes nothing writes.
	check_rejected("");
	check_rejected("    ");
	check_rejected("hello");
	check_rejected("08:00");
	check_rejected("12/03/2026"); // a day-first date: no such reading here
	check_rejected("0000");
	check_rejected("00000312");
	check_rejected("2026031"); // the compact shape is eight digits, whole
	check_rejected("202603121");
	check_rejected("26-03-12");

	// A pointer may come back empty-handed at any time.
	assert(!tagdate_parse(NULL, NULL));
	tagdate_t d;
	assert(!tagdate_parse(NULL, &d));
	assert(!tagdate_parse("2026-03-12", NULL));

	// The fuller date wins, whatever order the frames arrive in; a tie keeps
	// what the caller has (tagdate_better is then false).
	tagdate_t year = {.year = 2026};
	tagdate_t full = {.year = 2026, .month = 3, .day = 12};
	tagdate_t month = {.year = 2026, .month = 3};
	tagdate_t none = {0};
	assert(tagdate_better(&full, &year));
	assert(!tagdate_better(&year, &full));
	assert(tagdate_better(&month, &year));
	assert(tagdate_better(&full, &month));
	assert(!tagdate_better(&year, &year));
	assert(!tagdate_better(&full, &full));
	assert(tagdate_better(&year, &none));
	assert(!tagdate_better(&none, &year));
	assert(!tagdate_better(&none, &none));
	assert(!tagdate_better(NULL, &year));
	assert(tagdate_better(&year, NULL));
	printf("  better-than ranking\n");

	// The moment for sorting: the parts the tag left out settle on the first
	// of January, and of the month, and the date comes back out unchanged.
	struct tm back;
	time_t when = tagdate_to_time(&full);
	assert(when != 0);
	assert(localtime_r(&when, &back));
	assert(back.tm_year + 1900 == 2026 && back.tm_mon == 2 && back.tm_mday == 12);

	when = tagdate_to_time(&month);
	assert(when != 0);
	assert(localtime_r(&when, &back));
	assert(back.tm_year + 1900 == 2026 && back.tm_mon == 2 && back.tm_mday == 1);

	when = tagdate_to_time(&year);
	assert(when != 0);
	assert(localtime_r(&when, &back));
	assert(back.tm_year + 1900 == 2026 && back.tm_mon == 0 && back.tm_mday == 1);

	// And the order is the calendar's: an earlier date sorts before a later
	// one wherever the two fall.
	tagdate_t before = {.year = 2025, .month = 12, .day = 31};
	assert(tagdate_to_time(&before) < tagdate_to_time(&full));
	assert(tagdate_to_time(&year) < tagdate_to_time(&month));
	assert(tagdate_to_time(&month) < tagdate_to_time(&full));

	assert(tagdate_to_time(&none) == 0);
	assert(tagdate_to_time(NULL) == 0);
	printf("  moments for sorting\n");

	puts("PASS tag dates");
	return 0;
}
