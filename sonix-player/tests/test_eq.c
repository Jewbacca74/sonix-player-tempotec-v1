// test_eq.c -- the graphic EQ, PEQ and MSEB on PCM, on the host.
//
// Radio stations do not go through audio.c's decode loop: they decode on their
// own thread and write through audio_external_write(). The chain they share
// with a track from the card is eq_process() on stereo 16-bit frames, called
// from radio_push_pcm(). This suite drives that same function, so a station
// and a file cannot drift apart: if EQ, PEQ or MSEB stop changing samples
// here, they have stopped on the radio too.

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "src/system/audio/eq.h"
#include "src/system/core/config.h"

static int failures;

#define CHECK(cond)                                                                             \
	do {                                                                                        \
		if (!(cond)) {                                                                          \
			failures++;                                                                     \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                 \
		}                                                                                       \
	} while (0)

#define FRAMES 1024
#define RATE 44100
#define AMP 8000

static void fill_sine(short *pcm, int frames, int rate, int freq, int amp) {
	for (int i = 0; i < frames; i++) {
		double s = sin(2.0 * 3.14159265358979323846 * (double)freq * (double)i / (double)rate);
		short v = (short)(s * amp);
		pcm[2 * i] = v;
		pcm[2 * i + 1] = v;
	}
}

static void all_off(void) {
	eq_set_enabled(false);
	peq_set_enabled(false);
	mseb_set_enabled(false);
	eq_reset();
	peq_reset();
	mseb_reset();
}

static void test_off_is_a_no_op(void) {
	all_off();
	short original[FRAMES * 2];
	short buf[FRAMES * 2];
	fill_sine(original, FRAMES, RATE, 440, AMP);
	memcpy(buf, original, sizeof(buf));
	eq_process(buf, FRAMES, 2, RATE);
	CHECK(memcmp(buf, original, sizeof(buf)) == 0);
}

static void test_graphic_eq_changes_samples(void) {
	all_off();
	eq_set_enabled(true);
	eq_set_band(5, 12); // 1 kHz, the most a slider travels

	short original[FRAMES * 2];
	short buf[FRAMES * 2];
	fill_sine(original, FRAMES, RATE, 1000, AMP);
	memcpy(buf, original, sizeof(buf));
	eq_process(buf, FRAMES, 2, RATE);
	CHECK(memcmp(buf, original, sizeof(buf)) != 0);

	all_off();
	memcpy(buf, original, sizeof(buf));
	eq_process(buf, FRAMES, 2, RATE);
	CHECK(memcmp(buf, original, sizeof(buf)) == 0);
}

static void test_peq_changes_samples(void) {
	all_off();
	peq_band_t band;
	peq_get_band(0, &band);
	band.on = true;
	band.type = PEQ_TYPE_PEAK;
	band.freq = 1000;
	band.gain_tenths = 120;
	band.q_cent = 100;
	peq_set_band(0, &band);
	peq_set_enabled(true);

	short original[FRAMES * 2];
	short buf[FRAMES * 2];
	fill_sine(original, FRAMES, RATE, 1000, AMP);
	memcpy(buf, original, sizeof(buf));
	eq_process(buf, FRAMES, 2, RATE);
	CHECK(memcmp(buf, original, sizeof(buf)) != 0);

	all_off();
	memcpy(buf, original, sizeof(buf));
	eq_process(buf, FRAMES, 2, RATE);
	CHECK(memcmp(buf, original, sizeof(buf)) == 0);
}

static void test_mseb_changes_samples(void) {
	all_off();
	mseb_set_enabled(true);
	mseb_set_value(0, 80); // temperature, well off flat

	short original[FRAMES * 2];
	short buf[FRAMES * 2];
	fill_sine(original, FRAMES, RATE, 440, AMP);
	memcpy(buf, original, sizeof(buf));
	eq_process(buf, FRAMES, 2, RATE);
	CHECK(memcmp(buf, original, sizeof(buf)) != 0);

	all_off();
	memcpy(buf, original, sizeof(buf));
	eq_process(buf, FRAMES, 2, RATE);
	CHECK(memcmp(buf, original, sizeof(buf)) == 0);
}

// Radio stations arrive at 44.1 or 48 kHz, stereo, 16-bit -- the arguments
// radio_push_pcm() hands eq_process(). Both rates have to run.
static void test_radio_rates(void) {
	all_off();
	eq_set_enabled(true);
	eq_set_band(4, 8);

	static const int rates[] = {44100, 48000};
	for (unsigned i = 0; i < sizeof(rates) / sizeof(rates[0]); i++) {
		short original[FRAMES * 2];
		short buf[FRAMES * 2];
		fill_sine(original, FRAMES, rates[i], 500, AMP);
		memcpy(buf, original, sizeof(buf));
		eq_process(buf, FRAMES, 2, rates[i]);
		CHECK(memcmp(buf, original, sizeof(buf)) != 0);
	}
	all_off();
}

int main(void) {
	char dir[] = "/tmp/sonix-eq-test-XXXXXX";
	if (!mkdtemp(dir)) {
		perror("mkdtemp");
		return 1;
	}
	char path[256];
	snprintf(path, sizeof(path), "%s/device_config.ini", dir);
	// A missing file is not an error: the path is still remembered and the
	// defaults are used until the first save.
	config_init(path);
	eq_init();

	test_off_is_a_no_op();
	test_graphic_eq_changes_samples();
	test_peq_changes_samples();
	test_mseb_changes_samples();
	test_radio_rates();

	if (failures) {
		fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}
	printf("eq: graphic EQ, PEQ and MSEB all run on stereo PCM, including radio rates\n");
	return 0;
}
