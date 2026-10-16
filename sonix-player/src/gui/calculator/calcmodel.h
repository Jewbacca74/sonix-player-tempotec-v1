#ifndef GUI_CALCULATOR_CALCMODEL_H
#define GUI_CALCULATOR_CALCMODEL_H

#include <stdbool.h>

// The arithmetic behind the Calculator page.
//
// Plain C and no LVGL, for the reason the equaliser's own coefficients are
// kept out of the page: the part that can be wrong is then testable on its own
// (tests/test_calculator.c) and the page holds nothing but widgets. The model
// owns the display string; the page only draws it.

// Twelve characters, which is what fits across the V1's display at the size it
// is drawn in: a sign, a decimal point and ten digits, or the exponent form a
// very large result falls back to.
#define CALC_DISPLAY_MAX 13

typedef struct {
	char shown[CALC_DISPLAY_MAX];
	double value;  // what the display stands for
	double accum;  // the left operand of the pending operation
	char pending;  // '+', '-', '*', '/', or 0 for none
	bool typing;   // the display is a number being typed, not a result
	bool error;    // the last operation had no answer (a division by zero)
} calc_model_t;

void calc_model_init(calc_model_t *m);

// The keys. Digits are '0'-'9'; the operators are '+', '-', '*' and '/'.
void calc_key_digit(calc_model_t *m, char digit);
void calc_key_decimal(calc_model_t *m);
void calc_key_operator(calc_model_t *m, char op);
void calc_key_equals(calc_model_t *m);
void calc_key_backspace(calc_model_t *m);

// The sign of what is on screen. On a number still being typed it is a minus
// in front of it; on a result it negates the result itself, so "2 + 3 =" then
// "+/-" gives -5.
void calc_key_sign(calc_model_t *m);

// Divides what is on screen by a hundred. The simple rule, said out loud: a
// phone that answers "200 + 10 %" with 220 is doing something else with the
// same keys, and this screen has no room to explain that.
void calc_key_percent(calc_model_t *m);

// AC: everything, the pending operation included.
void calc_key_clear(calc_model_t *m);

// What to draw, or "0" on a model that has just been made.
const char *calc_display(const calc_model_t *m);

// Whether the display is the error state, which the page draws in the
// translation table's own words rather than the model's.
bool calc_is_error(const calc_model_t *m);

#endif /* GUI_CALCULATOR_CALCMODEL_H */
