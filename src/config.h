#ifndef POWERMATE_CONFIG_H
#define POWERMATE_CONFIG_H

#define SETTINGS_DEVICE_MAX 256

typedef struct {
	int step_size;
	int invert;
	int grab;
	int led_brightness;
	int click_middle;
	int hold_for_horizontal;
	int print_events;
	char device[SETTINGS_DEVICE_MAX];
} Settings;

void settings_init(Settings *s);
int settings_load_toml(Settings *s, const char *path, char *err, int errlen);
void settings_default_path(char *buf, int buflen);

#endif
