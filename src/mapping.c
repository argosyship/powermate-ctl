#include "mapping.h"

#include <string.h>

int is_powermate_id(int vendor, int product)
{
	return (vendor == POWERMATE_VID_GRIFFIN &&
		(product == POWERMATE_PID_CLASSIC ||
		 product == POWERMATE_PID_SOUNDKNOB)) ||
	       (vendor == CONTOUR_VID && product == CONTOUR_PID_JOG);
}

void mapper_init(ScrollMapper *m, int step_size, int invert, int click_middle,
		 int hold_for_horizontal)
{
	memset(m, 0, sizeof(*m));
	m->step_size = step_size;
	m->invert = invert;
	m->click_middle = click_middle;
	m->hold_for_horizontal = hold_for_horizontal;
}

void mapper_reset(ScrollMapper *m)
{
	m->v_accum = 0;
	m->h_accum = 0;
	m->pressed = 0;
	m->rotated_while_pressed = 0;
}

static WheelOutput wheel_zero(void)
{
	WheelOutput out;
	memset(&out, 0, sizeof(out));
	return out;
}

static void accumulate(int *accum, int delta, int *hi_res, int *wheel)
{
	*accum += delta;
	/* Integer division truncates toward zero. */
	*wheel = *accum / HI_RES_PER_NOTCH;
	*accum -= *wheel * HI_RES_PER_NOTCH;
	*hi_res = delta;
}

WheelOutput mapper_on_dial(ScrollMapper *m, int value)
{
	WheelOutput out = wheel_zero();
	int delta;
	int hi;
	int wheel;

	if (value == 0)
		return out;

	delta = (m->invert ? 1 : -1) * value * m->step_size;
	if (m->pressed)
		m->rotated_while_pressed = 1;

	if (m->pressed && m->hold_for_horizontal) {
		accumulate(&m->h_accum, delta, &hi, &wheel);
		out.h_hi_res = hi;
		out.h_wheel = wheel;
		return out;
	}

	accumulate(&m->v_accum, delta, &hi, &wheel);
	out.hi_res = hi;
	out.wheel = wheel;
	return out;
}

WheelOutput mapper_on_button(ScrollMapper *m, int pressed)
{
	WheelOutput out = wheel_zero();
	int was_rotated;

	if (pressed) {
		m->pressed = 1;
		m->rotated_while_pressed = 0;
		return out;
	}

	was_rotated = m->rotated_while_pressed;
	m->pressed = 0;
	m->rotated_while_pressed = 0;
	if (m->click_middle && !was_rotated)
		out.has_middle = 1;
	return out;
}

int wheel_has_scroll(const WheelOutput *out)
{
	return out->hi_res || out->wheel || out->h_hi_res || out->h_wheel;
}

int led_command(int brightness)
{
	if (brightness < 0)
		brightness = 0;
	if (brightness > 255)
		brightness = 255;
	/* speed=255, table=0, pulse_asleep=0, pulse_awake=0 */
	return brightness | (255 << 8);
}
