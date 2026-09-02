#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void settings_init(Settings *s)
{
	memset(s, 0, sizeof(*s));
	s->step_size = 24;
	s->invert = 0;
	s->grab = 1;
	s->led_brightness = 96;
	s->click_middle = 1;
	s->hold_for_horizontal = 1;
	s->print_events = 0;
	s->device[0] = '\0';
}

void settings_default_path(char *buf, int buflen)
{
	const char *xdg = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");

	if (xdg && xdg[0])
		snprintf(buf, (size_t)buflen, "%s/powermate-scroll/config.toml", xdg);
	else if (home && home[0])
		snprintf(buf, (size_t)buflen, "%s/.config/powermate-scroll/config.toml",
			 home);
	else
		snprintf(buf, (size_t)buflen, "/tmp/powermate-scroll.toml");
}

static char *trim(char *s)
{
	char *end;

	while (*s == ' ' || *s == '\t')
		s++;
	end = s + strlen(s);
	while (end > s &&
	       (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' ||
		end[-1] == '\n')) {
		*--end = '\0';
	}
	return s;
}

static void strip_comment(char *s)
{
	int in_quote = 0;
	char *p;

	for (p = s; *p; p++) {
		if (*p == '"')
			in_quote = !in_quote;
		else if (*p == '#' && !in_quote) {
			*p = '\0';
			return;
		}
	}
}

static int parse_bool(const char *v, int *out)
{
	if (strcmp(v, "true") == 0) {
		*out = 1;
		return 0;
	}
	if (strcmp(v, "false") == 0) {
		*out = 0;
		return 0;
	}
	return -1;
}

static int parse_string(const char *v, char *out, int outlen)
{
	size_t n;

	if (v[0] == '"') {
		const char *end = strrchr(v + 1, '"');
		if (!end)
			return -1;
		n = (size_t)(end - (v + 1));
		if (n >= (size_t)outlen)
			return -1;
		memcpy(out, v + 1, n);
		out[n] = '\0';
		return 0;
	}
	if (strlen(v) >= (size_t)outlen)
		return -1;
	memcpy(out, v, strlen(v) + 1);
	return 0;
}

int settings_load_toml(Settings *s, const char *path, char *err, int errlen)
{
	FILE *fp;
	char line[512];
	int lineno = 0;

	fp = fopen(path, "r");
	if (!fp)
		return 0;

	while (fgets(line, sizeof(line), fp)) {
		char *eq;
		char *key;
		char *val;
		char str[SETTINGS_DEVICE_MAX];

		lineno++;
		strip_comment(line);
		key = trim(line);
		if (!key[0])
			continue;
		eq = strchr(key, '=');
		if (!eq)
			continue;
		*eq = '\0';
		key = trim(key);
		val = trim(eq + 1);

		if (strcmp(key, "step_size") == 0) {
			s->step_size = atoi(val);
		} else if (strcmp(key, "invert") == 0) {
			if (parse_bool(val, &s->invert) != 0) {
				snprintf(err, (size_t)errlen,
					 "%s:%d: invert must be true or false", path,
					 lineno);
				fclose(fp);
				return -1;
			}
		} else if (strcmp(key, "grab") == 0) {
			if (parse_bool(val, &s->grab) != 0) {
				snprintf(err, (size_t)errlen,
					 "%s:%d: grab must be true or false", path,
					 lineno);
				fclose(fp);
				return -1;
			}
		} else if (strcmp(key, "led_brightness") == 0) {
			s->led_brightness = atoi(val);
		} else if (strcmp(key, "click") == 0) {
			if (parse_string(val, str, sizeof(str)) != 0) {
				snprintf(err, (size_t)errlen, "%s:%d: bad click value",
					 path, lineno);
				fclose(fp);
				return -1;
			}
			if (strcmp(str, "middle") == 0)
				s->click_middle = 1;
			else if (strcmp(str, "none") == 0)
				s->click_middle = 0;
			else {
				snprintf(err, (size_t)errlen,
					 "click must be 'middle' or 'none'");
				fclose(fp);
				return -1;
			}
		} else if (strcmp(key, "hold_for_horizontal") == 0) {
			if (parse_bool(val, &s->hold_for_horizontal) != 0) {
				snprintf(err, (size_t)errlen,
					 "%s:%d: hold_for_horizontal must be true or false",
					 path, lineno);
				fclose(fp);
				return -1;
			}
		} else if (strcmp(key, "device") == 0) {
			if (parse_string(val, s->device, SETTINGS_DEVICE_MAX) != 0) {
				snprintf(err, (size_t)errlen, "%s:%d: bad device path",
					 path, lineno);
				fclose(fp);
				return -1;
			}
		}
	}

	fclose(fp);

	if (s->step_size <= 0) {
		snprintf(err, (size_t)errlen, "step_size must be a positive integer");
		return -1;
	}
	if (s->led_brightness < 0 || s->led_brightness > 255) {
		snprintf(err, (size_t)errlen, "led_brightness must be between 0 and 255");
		return -1;
	}
	return 0;
}
