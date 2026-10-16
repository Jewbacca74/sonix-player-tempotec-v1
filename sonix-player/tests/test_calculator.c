// Headless checks of the calculator's arithmetic: the twenty keys of the
// Calculator page driven through the model, with no screen in the way.
//
// The model is the whole of the calculator that can be wrong quietly -- a
// display showing the wrong number of digits, a chain of operations that
// computes the wrong sum -- so it is kept in plain C (src/gui/calculator/
// calcmodel.c) and tested here. The page above it only draws what these
// functions return.

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/gui/calculator/calcmodel.h"

static calc_model_t model;

static void press(const char *keys) {
	for (const char *k = keys; *k; k++) {
		if (*k >= '0' && *k <= '9') {
			calc_key_digit(&model, *k);
		} else if (*k == '.') {
			calc_key_decimal(&model);
		} else if (*k == '+' || *k == '-' || *k == '*' || *k == '/') {
			calc_key_operator(&model, *k);
		} else if (*k == '=') {
			calc_key_equals(&model);
		} else if (*k == '%') {
			calc_key_percent(&model);
		} else if (*k == 's') {
			calc_key_sign(&model);
		} else if (*k == '<') {
			calc_key_backspace(&model);
		} else if (*k == 'C') {
			calc_key_clear(&model);
		} else {
			assert(!"unknown key in the test script");
		}
	}
}

static void expect(const char *shown) {
	if (strcmp(calc_display(&model), shown) != 0) {
		fprintf(stderr, "expected \"%s\", display shows \"%s\"\n", shown, calc_display(&model));
		assert(!"wrong display");
	}
	assert(!calc_is_error(&model));
	assert(strlen(calc_display(&model)) < CALC_DISPLAY_MAX);
}

static void test_typing(void) {
	calc_model_init(&model);
	expect("0");

	press("7");
	expect("7");
	press("8");
	expect("78");

	// Only ten digits: the eleventh is dropped rather than shown cut off.
	press("123456789");
	expect("7812345678");
	press("0");
	expect("7812345678");

	// The decimal point cannot be entered twice: the second one is dropped
	// while the digits around it are not.
	press("C");
	press(".5");
	expect("0.5");
	press(".5");
	expect("0.55");
	press("C");
	press("1.5.5");
	expect("1.55");

	// Backspace walks back to zero and stops there.
	press("C");
	press("42");
	press("<");
	expect("4");
	press("<");
	expect("0");
	press("<");
	expect("0");

	// A decimal point on its own reads as zero point.
	press("C");
	press(".");
	expect("0.");
}

static void test_operations(void) {
	calc_model_init(&model);
	press("12+7");
	// The display shows what is being typed, not the sum being worked out.
	expect("7");
	press("=");
	expect("19");

	// A digit after a result starts a new sum rather than extending the old
	// one: 19 + 3 is not 193.
	press("3");
	expect("3");

	// An operator after a result carries the result forward.
	press("C");
	press("2+3=");
	expect("5");
	press("*4=");
	expect("20");

	// A pocket calculator's arithmetic: each operator is applied to what is on
	// screen as it is pressed, so 2+3*4 is twenty. There is no room on the
	// glass for the parentheses the other answer would need.
	press("C");
	press("2+3*4=");
	expect("20");

	// Left to right when the operators are equal: 9-2-3 is four.
	press("C");
	press("9-2-3=");
	expect("4");

	// A second operator in a row replaces the pending one.
	press("C");
	press("6+*2=");
	expect("12");

	// Pressing = with nothing typed after the operator doubles what is on
	// screen, which is what the same keys do on a pocket calculator.
	press("C");
	press("6+");
	press("=");
	expect("12");

	// Negative operands.
	press("C");
	press("10-25=");
	expect("-15");

	// Dividing by zero is the one operation with no answer.
	press("C");
	press("9/0=");
	assert(calc_is_error(&model));
	assert(strcmp(calc_display(&model), "Error") == 0);
	// The operators and the sign keys do nothing while the error stands --
	// there is no left operand to work from -- and C, or any digit, starts
	// again.
	press("+");
	assert(calc_is_error(&model));
	press("C");
	expect("0");
	press("9/0=");
	assert(calc_is_error(&model));
	press("7");
	expect("7");
}

static void test_sign_percent(void) {
	calc_model_init(&model);

	// The sign flips what is on screen, typed or computed.
	press("5s");
	expect("-5");
	press("s");
	expect("5");
	press("+3=");
	expect("8");
	// The sign of a result flips the result itself.
	press("s");
	expect("-8");

	// Percent is the simple rule: what is on screen, divided by a hundred.
	press("C");
	press("250%");
	expect("2.5");
	press("C");
	press("50%");
	expect("0.5");
	press("C");
	press("8%");
	expect("0.08");
}

static void test_display_width(void) {
	calc_model_init(&model);

	// A third is shown to the width of the display and no further.
	press("1/3=");
	expect("0.333333333");

	// A sum a calculator has to round is rounded, not cut: two thirds of a
	// hundred and sixty-eight thousandths.
	press("C");
	press("2/3=");
	expect("0.666666667");

	// A result too wide even rounded falls back to fewer significant digits,
	// and then to the exponent form rather than a display full of digits.
	press("C");
	press("123456789*123456789=");
	expect("1.5242e+16");

	press("C");
	press("9999999999*9999999999=");
	expect("1e+20");
	assert(strlen(calc_display(&model)) < CALC_DISPLAY_MAX);

	// Division that does not fit still lands inside the display.
	press("C");
	press("1/7=");
	expect("0.142857143");
	assert(strlen(calc_display(&model)) == 11);

	// Whole results are whole: no trailing zeros, no decimal point.
	press("C");
	press("1000000*1000=");
	expect("1000000000");
}

static void test_clear_and_reuse(void) {
	calc_model_init(&model);
	press("99*99=+1=");
	expect("9802");

	// Clear takes the pending operation with it.
	press("C");
	expect("0");
	press("7");
	expect("7");

	// Clearing twice is not an error, and the model survives being made
	// again -- which is what the page does on the way in.
	press("C");
	press("+");
	press("C");
	expect("0");
	calc_model_init(&model);
	expect("0");
	press("5+5=");
	expect("10");
}

int main(void) {
	test_typing();
	test_operations();
	test_sign_percent();
	test_display_width();
	test_clear_and_reuse();
	printf("PASS calculator arithmetic\n");
	return 0;
}
