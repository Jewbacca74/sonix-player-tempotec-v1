#include "calcmodel.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h> // strtod
#include <string.h>

#define ERROR_TEXT "Error"
// Digits that fit beside a sign and a decimal point. A number being typed
// stops here rather than running off the display.
#define MAX_DIGITS 10

// A double as the display can hold it.
//
// The widest form is tried first, so a sum that comes out whole stays whole:
// 16 * 32 is "512" and a million times a thousand is "1000000000", not
// "5.12e+02" or "1e+09" -- the exponent form is right for a number that has no
// other room, not for an ordinary one. %g also drops the trailing zeros, and
// twelve significant digits of a double are enough that 0.1 + 0.2 comes out
// "0.3" rather than 0.30000000000000004.
//
// A result too long for the display falls back to fewer significant digits,
// and at the very end to the error state -- the only honest answer left: the
// number is real, but there is no room for it here.
static bool value_to_text(double v, char *out, size_t size) {
	if (isnan(v) || isinf(v)) {
		return false;
	}
	if (v == 0.0) {
		v = 0.0; // -0 is a display artefact, not a number the user typed
	}

	static const char *const FORMATS[] = {"%.12g", "%.9g", "%.5g", "%.2g"};
	for (size_t i = 0; i < sizeof(FORMATS) / sizeof(FORMATS[0]); i++) {
		// The length snprintf *would* have written, not the length of what is
		// in the buffer: a number too long for the display arrives here cut
		// off, and "1.52415788e+" is a number the user never asked for.
		int written = snprintf(out, size, FORMATS[i], v);
		if (written > 0 && written < (int)size) {
			return true;
		}
	}
	out[0] = '\0';
	return false;
}

static void set_error(calc_model_t *m) {
	m->error = true;
	m->pending = 0;
	m->typing = false;
	m->value = 0;
	m->accum = 0;
	snprintf(m->shown, sizeof(m->shown), "%s", ERROR_TEXT);
}

static void show_value(calc_model_t *m, double v) {
	if (!value_to_text(v, m->shown, sizeof(m->shown))) {
		set_error(m);
		return;
	}
	m->value = v;
	m->typing = false;
}

static bool digits_in(const char *text) {
	int count = 0;
	for (const char *p = text; *p; p++) {
		if (*p >= '0' && *p <= '9') {
			count++;
		}
	}
	return count < MAX_DIGITS;
}

// The pending operation, applied. A division by zero has no answer and puts
// the model in its error state; every key after that starts again from
// nothing, which is what a calculator that cannot be reset by anything else
// has to do.
static double apply(calc_model_t *m, double a, double b, char op) {
	switch (op) {
	case '+':
		return a + b;
	case '-':
		return a - b;
	case '*':
		return a * b;
	case '/':
		if (b == 0) {
			set_error(m);
			return 0;
		}
		return a / b;
	default:
		return b;
	}
}

// Reads the number on screen (or the loop above's fallback) into value.
static void sync_value(calc_model_t *m) {
	if (m->typing) {
		m->value = strtod(m->shown, NULL);
	}
}

void calc_model_init(calc_model_t *m) {
	memset(m, 0, sizeof(*m));
	snprintf(m->shown, sizeof(m->shown), "0");
}

void calc_key_digit(calc_model_t *m, char digit) {
	if (digit < '0' || digit > '9') {
		return;
	}
	if (m->error) {
		calc_model_init(m);
	}
	if (!m->typing) {
		// A digit after a result, or after an operator, starts a new number.
		// A leading zero is replaced rather than extended.
		snprintf(m->shown, sizeof(m->shown), "%c", digit);
		m->typing = true;
		sync_value(m);
		return;
	}
	if (!digits_in(m->shown)) {
		return;
	}
	size_t len = strlen(m->shown);
	if (len == 1 && m->shown[0] == '0') {
		m->shown[0] = digit;
	} else if (len + 1 < sizeof(m->shown)) {
		m->shown[len] = digit;
		m->shown[len + 1] = '\0';
	}
	sync_value(m);
}

void calc_key_decimal(calc_model_t *m) {
	if (m->error) {
		calc_model_init(m);
	}
	if (!m->typing) {
		snprintf(m->shown, sizeof(m->shown), "0.");
		m->typing = true;
		sync_value(m);
		return;
	}
	if (strchr(m->shown, '.') || strlen(m->shown) + 1 >= sizeof(m->shown)) {
		return;
	}
	size_t len = strlen(m->shown);
	m->shown[len] = '.';
	m->shown[len + 1] = '\0';
}

void calc_key_backspace(calc_model_t *m) {
	if (m->error) {
		calc_model_init(m);
		return;
	}
	if (!m->typing) {
		// Nothing is being typed: the whole entry goes, as one press does on
		// a calculator that has no second clear key.
		snprintf(m->shown, sizeof(m->shown), "0");
		m->value = 0;
		return;
	}
	size_t len = strlen(m->shown);
	if (len > 0) {
		m->shown[len - 1] = '\0';
	}
	if (m->shown[0] == '\0' || strcmp(m->shown, "-") == 0) {
		snprintf(m->shown, sizeof(m->shown), "0");
		m->typing = false;
		m->value = 0;
		return;
	}
	sync_value(m);
}

void calc_key_sign(calc_model_t *m) {
	if (m->error) {
		return;
	}
	if (m->typing) {
		if (m->shown[0] == '-') {
			memmove(m->shown, m->shown + 1, strlen(m->shown));
		} else if (strlen(m->shown) + 1 < sizeof(m->shown)) {
			memmove(m->shown + 1, m->shown, strlen(m->shown) + 1);
			m->shown[0] = '-';
		}
		sync_value(m);
		return;
	}
	show_value(m, -m->value);
}

void calc_key_percent(calc_model_t *m) {
	if (m->error) {
		return;
	}
	sync_value(m);
	show_value(m, m->value / 100.0);
}

void calc_key_operator(calc_model_t *m, char op) {
	if (op != '+' && op != '-' && op != '*' && op != '/') {
		return;
	}
	if (m->error) {
		return; // the display has to be cleared first: there is no left operand
	}
	if (m->typing) {
		if (m->pending) {
			// A chain: finish the operation that was waiting.
			double result = apply(m, m->accum, m->value, m->pending);
			if (m->error) {
				return;
			}
			show_value(m, result);
		}
		m->accum = m->value;
	} else if (!m->pending) {
		sync_value(m);
		m->accum = m->value;
	}
	// Pressed twice, the second operator replaces the first: the accumulator
	// is already right.
	m->pending = op;
	m->typing = false;
}

void calc_key_equals(calc_model_t *m) {
	if (m->error || !m->pending) {
		return;
	}
	double result = apply(m, m->accum, m->value, m->pending);
	if (m->error) {
		return;
	}
	m->pending = 0;
	show_value(m, result);
}

void calc_key_clear(calc_model_t *m) { calc_model_init(m); }

const char *calc_display(const calc_model_t *m) { return m->shown; }

bool calc_is_error(const calc_model_t *m) { return m->error; }
