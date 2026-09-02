#define _POSIX_C_SOURCE 200809L

#include "../src/config.h"
#include "../src/mapping.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failures;

#define EXPECT_EQ(a, b)                                                                \
	do {                                                                           \
		long _a = (long)(a);                                                   \
		long _b = (long)(b);                                                   \
		if (_a != _b) {                                                        \
			fprintf(stderr, "%s:%d: got %ld, expected %ld\n", __FILE__,    \
				__LINE__, _a, _b);                                     \
			failures++;                                                    \
		}                                                                      \
	} while (0)

static void test_mapping(void)
{
	ScrollMapper m;
	WheelOutput out;
	int i;
	int total_hi = 0;
	int notches = 0;

	mapper_init(&m, 24, 0, 1, 1);
	out = mapper_on_dial(&m, 1);
	EXPECT_EQ(out.hi_res, -24);
	EXPECT_EQ(out.wheel, 0);

	mapper_init(&m, 24, 0, 1, 1);
	out = mapper_on_dial(&m, -1);
	EXPECT_EQ(out.hi_res, 24);

	mapper_init(&m, 10, 1, 1, 1);
	EXPECT_EQ(mapper_on_dial(&m, 1).hi_res, 10);

	mapper_init(&m, 40, 0, 1, 1);
	EXPECT_EQ(mapper_on_dial(&m, 1).wheel, 0);
	EXPECT_EQ(mapper_on_dial(&m, 1).wheel, 0);
	out = mapper_on_dial(&m, 1);
	EXPECT_EQ(out.hi_res, -40);
	EXPECT_EQ(out.wheel, -1);

	mapper_init(&m, 24, 0, 1, 1);
	out = mapper_on_dial(&m, 5);
	EXPECT_EQ(out.hi_res, -120);
	EXPECT_EQ(out.wheel, -1);

	mapper_init(&m, 24, 0, 1, 1);
	for (i = 0; i < 94; i++) {
		out = mapper_on_dial(&m, 1);
		total_hi += out.hi_res;
		notches += out.wheel;
	}
	EXPECT_EQ(total_hi, -94 * 24);
	EXPECT_EQ(notches, total_hi / HI_RES_PER_NOTCH);
	EXPECT_EQ(notches, -18);

	mapper_init(&m, 16, 0, 1, 1);
	mapper_on_button(&m, 1);
	out = mapper_on_dial(&m, 1);
	EXPECT_EQ(out.hi_res, 0);
	EXPECT_EQ(out.h_hi_res, -16);

	mapper_init(&m, 24, 0, 1, 1);
	EXPECT_EQ(mapper_on_button(&m, 1).has_middle, 0);
	out = mapper_on_button(&m, 0);
	EXPECT_EQ(out.has_middle, 1);

	mapper_init(&m, 24, 0, 1, 1);
	mapper_on_button(&m, 1);
	mapper_on_dial(&m, 1);
	EXPECT_EQ(mapper_on_button(&m, 0).has_middle, 0);

	mapper_init(&m, 24, 0, 1, 0);
	mapper_on_button(&m, 1);
	out = mapper_on_dial(&m, 1);
	EXPECT_EQ(out.hi_res, -24);
	EXPECT_EQ(mapper_on_button(&m, 0).has_middle, 0);

	mapper_init(&m, 24, 0, 0, 1);
	mapper_on_button(&m, 1);
	EXPECT_EQ(mapper_on_button(&m, 0).has_middle, 0);

	mapper_init(&m, 24, 0, 1, 1);
	out = mapper_on_dial(&m, 0);
	EXPECT_EQ(wheel_has_scroll(&out), 0);
}

static void test_led(void)
{
	EXPECT_EQ(led_command(96) & 0xff, 96);
	EXPECT_EQ(led_command(999) & 0xff, 255);
	EXPECT_EQ(led_command(-1) & 0xff, 0);
}

static void test_config(void)
{
	char path[] = "/tmp/powermate-scroll-test-XXXXXX";
	char path2[] = "/tmp/powermate-scroll-test-XXXXXX";
	int fd;
	FILE *fp;
	Settings s;
	char err[256];

	fd = mkstemp(path);
	if (fd < 0) {
		perror("mkstemp");
		failures++;
		return;
	}
	fp = fdopen(fd, "w");
	fputs("step_size = 8\nclick = \"none\"\ninvert = true\n", fp);
	fclose(fp);

	settings_init(&s);
	err[0] = '\0';
	if (settings_load_toml(&s, path, err, sizeof(err)) != 0) {
		fprintf(stderr, "config load failed: %s\n", err);
		failures++;
	} else {
		EXPECT_EQ(s.step_size, 8);
		EXPECT_EQ(s.click_middle, 0);
		EXPECT_EQ(s.invert, 1);
	}
	unlink(path);

	fd = mkstemp(path2);
	if (fd < 0) {
		perror("mkstemp");
		failures++;
		return;
	}
	fp = fdopen(fd, "w");
	fputs("click = \"left\"\n", fp);
	fclose(fp);
	settings_init(&s);
	EXPECT_EQ(settings_load_toml(&s, path2, err, sizeof(err)) != 0, 1);
	unlink(path2);
}

int main(void)
{
	test_mapping();
	test_led();
	test_config();
	if (failures) {
		fprintf(stderr, "%d failure(s)\n", failures);
		return 1;
	}
	puts("ok");
	return 0;
}
