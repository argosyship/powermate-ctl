#ifndef POWERMATE_MAPPING_H
#define POWERMATE_MAPPING_H

#define HI_RES_PER_NOTCH 120

#define POWERMATE_VID_GRIFFIN 0x077d
#define POWERMATE_PID_CLASSIC 0x0410
#define POWERMATE_PID_SOUNDKNOB 0x04aa
#define CONTOUR_VID 0x05f3
#define CONTOUR_PID_JOG 0x0240

typedef struct {
	int hi_res;
	int wheel;
	int h_hi_res;
	int h_wheel;
	int has_middle;
} WheelOutput;

typedef struct {
	int step_size;
	int invert;
	int click_middle;
	int hold_for_horizontal;
	int v_accum;
	int h_accum;
	int pressed;
	int rotated_while_pressed;
} ScrollMapper;

int is_powermate_id(int vendor, int product);

void mapper_init(ScrollMapper *m, int step_size, int invert, int click_middle,
		 int hold_for_horizontal);
void mapper_reset(ScrollMapper *m);
WheelOutput mapper_on_dial(ScrollMapper *m, int value);
WheelOutput mapper_on_button(ScrollMapper *m, int pressed);

int wheel_has_scroll(const WheelOutput *out);
int led_command(int brightness);

#endif
