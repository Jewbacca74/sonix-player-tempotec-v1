#ifndef TAGDATE_H
#define TAGDATE_H

#include <stdbool.h>
#include <time.h>

// The date a tag carries, at whatever precision it carries it.
//
// A date tag is a moment of any precision: "2026", "2026-03", "2026-03-12",
// "2026-03-12T08:00:00". The parts the tag left out are zero here rather than
// guessed at -- a podcast episode dated only to its year must not be shown as
// "1 January 2026" -- and a caller that needs one moment to sort by asks
// tagdate_to_time(), which settles the missing parts on purpose and says so.
typedef struct {
	int year;  // 0 when the text held no date at all
	int month; // 1 to 12; 0 when the tag named only a year
	int day;   // 1 to 31; 0 when the tag named only a year or a month
} tagdate_t;

// Reads the date out of a tag's text, whichever shape it comes in: ISO with
// dashes ("2026-03-12"), the same with slashes ("2026/03/12"), the compact
// "20260312", and any of them followed by a time ("...T08:00:00", "... 08:00",
// with a zone or not). What is missing stays missing: "2026-13" is a year,
// "2026-02-99" is a year and a month, and a text with no date in it is false.
//
// What follows the date is ignored rather than refused: ID3v2.4 writes ranges
// ("2026-03-12/2026-03-13") and stamps with zones, and the date wanted is the
// first part of either.
bool tagdate_parse(const char *text, tagdate_t *out);

// True when `a` names more of the date than `b`. A file can carry the same
// date twice at different precision -- TYER's bare year and TDRC's full
// moment in one ID3 tag -- and the frames arrive in whatever order the tagger
// wrote them. The fuller date wins; on a tie the caller keeps what it has.
bool tagdate_better(const tagdate_t *a, const tagdate_t *b);

// The date as one moment, for sorting and for the date formatters: the parts
// the tag left out settle on the first of January, and of the month. 0 when
// there is no year to build on. Note what this is not: a timestamp the tag
// promised (a calendar date carries none), and not safe past 2037 on the
// device, whose time_t is 32-bit like the rest of its file times.
time_t tagdate_to_time(const tagdate_t *d);

#endif /* TAGDATE_H */
