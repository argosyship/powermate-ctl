#define _GNU_SOURCE

#include "config.h"
#include "mapping.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libevdev/libevdev-uinput.h>
#include <libevdev/libevdev.h>
#include <linux/input.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define UINPUT_NAME "PowerMate High-Res Scroll"
#define WAIT_SECONDS 1

static volatile sig_atomic_t g_stop;

static void on_signal(int sig)
{
	(void)sig;
	g_stop = 1;
}

static void log_msg(const char *level, const char *fmt, ...)
{
	struct tm tm;
	time_t now = time(NULL);
	va_list ap;

	localtime_r(&now, &tm);
	fprintf(stderr, "%02d:%02d:%02d %s ", tm.tm_hour, tm.tm_min, tm.tm_sec, level);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
}

static void fd_path(int fd, char *buf, size_t buflen)
{
	char linkbuf[64];
	ssize_t len;

	snprintf(linkbuf, sizeof(linkbuf), "/proc/self/fd/%d", fd);
	len = readlink(linkbuf, buf, buflen - 1);
	if (len < 0) {
		snprintf(buf, buflen, "fd %d", fd);
		return;
	}
	buf[len] = '\0';
}

static int name_has_powermate(const char *name)
{
	const char *p;

	if (!name)
		return 0;
	for (p = name; *p; p++) {
		const char *a = p;
		const char *b = "powermate";
		while (*a && *b &&
		       tolower((unsigned char)*a) == (unsigned char)*b) {
			a++;
			b++;
		}
		if (!*b)
			return 1;
	}
	return 0;
}

static int open_event_fd(const char *path)
{
	int fd = open(path, O_RDWR | O_CLOEXEC);
	if (fd < 0)
		fd = open(path, O_RDONLY | O_CLOEXEC);
	return fd;
}

static int cmpstr(const void *a, const void *b)
{
	return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static int collect_event_paths(char ***out, int *count)
{
	DIR *dir;
	struct dirent *ent;
	char **paths = NULL;
	int n = 0;
	int cap = 0;

	dir = opendir("/dev/input");
	if (!dir) {
		*out = NULL;
		*count = 0;
		return -1;
	}
	while ((ent = readdir(dir)) != NULL) {
		char path[sizeof("/dev/input/") + 256];
		char **next;

		if (strncmp(ent->d_name, "event", 5) != 0)
			continue;
		snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);
		if (n == cap) {
			cap = cap ? cap * 2 : 16;
			next = realloc(paths, (size_t)cap * sizeof(*paths));
			if (!next) {
				closedir(dir);
				return -1;
			}
			paths = next;
		}
		paths[n] = strdup(path);
		if (!paths[n]) {
			closedir(dir);
			return -1;
		}
		n++;
	}
	closedir(dir);
	qsort(paths, (size_t)n, sizeof(*paths), cmpstr);
	*out = paths;
	*count = n;
	return 0;
}

static void free_paths(char **paths, int n)
{
	int i;

	if (!paths)
		return;
	for (i = 0; i < n; i++)
		free(paths[i]);
	free(paths);
}

static int list_input_devices(void)
{
	char **paths = NULL;
	int n = 0;
	int i;
	int found = 0;

	if (collect_event_paths(&paths, &n) != 0 || n == 0) {
		printf("No input devices found in /dev/input.\n");
		free_paths(paths, n);
		return 1;
	}

	for (i = 0; i < n; i++) {
		int fd;
		struct libevdev *dev = NULL;
		int vendor;
		int product;
		const char *name;
		const char *mark = "";

		fd = open_event_fd(paths[i]);
		if (fd < 0) {
			printf("%s: (%s)\n", paths[i], strerror(errno));
			continue;
		}
		if (libevdev_new_from_fd(fd, &dev) != 0) {
			printf("%s: (%s)\n", paths[i], strerror(errno));
			close(fd);
			continue;
		}
		vendor = libevdev_get_id_vendor(dev);
		product = libevdev_get_id_product(dev);
		name = libevdev_get_name(dev);
		if (is_powermate_id(vendor, product)) {
			mark = "  [PowerMate]";
			found = 1;
		}
		printf("%s: '%s' id %04x:%04x%s\n", paths[i], name ? name : "",
		       vendor & 0xffff, product & 0xffff, mark);
		libevdev_free(dev);
		close(fd);
	}
	free_paths(paths, n);

	if (!found) {
		printf("\nNo Griffin PowerMate found. Plug it in and check:\n"
		       "  lsusb | grep -i griffin\n"
		       "  lsmod | grep powermate\n"
		       "  sudo modprobe powermate\n");
		return 1;
	}
	return 0;
}

static int find_powermate(const char *preferred, struct libevdev **out_dev, int *out_fd)
{
	char **paths = NULL;
	int n = 0;
	int i;
	int chosen_fd = -1;
	struct libevdev *chosen = NULL;
	char pathbuf[256];

	if (preferred && preferred[0]) {
		int fd = open_event_fd(preferred);
		struct libevdev *dev = NULL;

		if (fd < 0)
			return -1;
		if (libevdev_new_from_fd(fd, &dev) != 0) {
			close(fd);
			return -1;
		}
		log_msg("INFO", "Using --device %s (%s)", preferred,
			libevdev_get_name(dev));
		*out_dev = dev;
		*out_fd = fd;
		return 0;
	}

	if (collect_event_paths(&paths, &n) != 0)
		return -1;

	for (i = 0; i < n; i++) {
		int fd;
		struct libevdev *dev = NULL;
		int vendor;
		int product;
		const char *name;

		fd = open_event_fd(paths[i]);
		if (fd < 0)
			continue;
		if (libevdev_new_from_fd(fd, &dev) != 0) {
			close(fd);
			continue;
		}
		vendor = libevdev_get_id_vendor(dev);
		product = libevdev_get_id_product(dev);
		name = libevdev_get_name(dev);
		if (is_powermate_id(vendor, product) || name_has_powermate(name)) {
			if (!chosen) {
				chosen = dev;
				chosen_fd = fd;
			} else {
				libevdev_free(dev);
				close(fd);
			}
		} else {
			libevdev_free(dev);
			close(fd);
		}
	}
	free_paths(paths, n);

	if (!chosen)
		return -1;

	fd_path(chosen_fd, pathbuf, sizeof(pathbuf));
	log_msg("INFO", "Found %s at %s", libevdev_get_name(chosen), pathbuf);
	*out_dev = chosen;
	*out_fd = chosen_fd;
	return 0;
}

static int open_uinput(struct libevdev_uinput **out)
{
	struct libevdev *dev;
	int rc;

	dev = libevdev_new();
	if (!dev)
		return -1;
	libevdev_set_name(dev, UINPUT_NAME);
	libevdev_set_id_bustype(dev, BUS_USB);
	libevdev_set_id_vendor(dev, 0x077d);
	libevdev_set_id_product(dev, 0x0411);
	libevdev_set_id_version(dev, 0x0001);
	libevdev_enable_property(dev, INPUT_PROP_POINTER);
	libevdev_enable_event_type(dev, EV_KEY);
	libevdev_enable_event_code(dev, EV_KEY, BTN_LEFT, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_RIGHT, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_MIDDLE, NULL);
	libevdev_enable_event_type(dev, EV_REL);
	libevdev_enable_event_code(dev, EV_REL, REL_X, NULL);
	libevdev_enable_event_code(dev, EV_REL, REL_Y, NULL);
	libevdev_enable_event_code(dev, EV_REL, REL_WHEEL, NULL);
	libevdev_enable_event_code(dev, EV_REL, REL_HWHEEL, NULL);
	libevdev_enable_event_code(dev, EV_REL, REL_WHEEL_HI_RES, NULL);
	libevdev_enable_event_code(dev, EV_REL, REL_HWHEEL_HI_RES, NULL);

	rc = libevdev_uinput_create_from_device(dev, LIBEVDEV_UINPUT_OPEN_MANAGED, out);
	libevdev_free(dev);
	if (rc != 0) {
		fprintf(stderr,
			"Cannot open /dev/uinput. Load the module and install the udev rule:\n"
			"  sudo modprobe uinput\n"
			"  sudo install -Dm644 packaging/99-powermate-scroll.rules "
			"/etc/udev/rules.d/99-powermate-scroll.rules\n"
			"  sudo udevadm control --reload-rules && sudo udevadm trigger\n"
			"Original error: %s\n",
			strerror(-rc));
		return -1;
	}
	return 0;
}

static void set_led(int fd, int brightness)
{
	struct input_event ev;

	memset(&ev, 0, sizeof(ev));
	ev.type = EV_MSC;
	ev.code = MSC_PULSELED;
	ev.value = led_command(brightness);
	if (write(fd, &ev, sizeof(ev)) != (ssize_t)sizeof(ev))
		return;
	memset(&ev, 0, sizeof(ev));
	ev.type = EV_SYN;
	ev.code = SYN_REPORT;
	ev.value = 0;
	(void)write(fd, &ev, sizeof(ev));
}

static void inject(struct libevdev_uinput *ui, const WheelOutput *out)
{
	int need_syn = 0;

	if (out->hi_res) {
		libevdev_uinput_write_event(ui, EV_REL, REL_WHEEL_HI_RES, out->hi_res);
		need_syn = 1;
	}
	if (out->wheel) {
		libevdev_uinput_write_event(ui, EV_REL, REL_WHEEL, out->wheel);
		need_syn = 1;
	}
	if (out->h_hi_res) {
		libevdev_uinput_write_event(ui, EV_REL, REL_HWHEEL_HI_RES, out->h_hi_res);
		need_syn = 1;
	}
	if (out->h_wheel) {
		libevdev_uinput_write_event(ui, EV_REL, REL_HWHEEL, out->h_wheel);
		need_syn = 1;
	}
	if (out->has_middle) {
		libevdev_uinput_write_event(ui, EV_KEY, BTN_MIDDLE, 1);
		libevdev_uinput_write_event(ui, EV_SYN, SYN_REPORT, 0);
		libevdev_uinput_write_event(ui, EV_KEY, BTN_MIDDLE, 0);
		need_syn = 1;
	}
	if (need_syn)
		libevdev_uinput_write_event(ui, EV_SYN, SYN_REPORT, 0);
}

static void describe_output(const WheelOutput *out, char *buf, size_t buflen)
{
	char *p = buf;
	size_t left = buflen;
	int n;

	buf[0] = '\0';
	if (out->hi_res) {
		n = snprintf(p, left, "wheel_hi_res=%d", out->hi_res);
		if (n < 0 || (size_t)n >= left)
			return;
		p += n;
		left -= (size_t)n;
	}
	if (out->wheel) {
		n = snprintf(p, left, "%swheel=%d", buf[0] ? ", " : "", out->wheel);
		if (n < 0 || (size_t)n >= left)
			return;
		p += n;
		left -= (size_t)n;
	}
	if (out->h_hi_res) {
		n = snprintf(p, left, "%shwheel_hi_res=%d", buf[0] ? ", " : "",
			     out->h_hi_res);
		if (n < 0 || (size_t)n >= left)
			return;
		p += n;
		left -= (size_t)n;
	}
	if (out->h_wheel) {
		n = snprintf(p, left, "%shwheel=%d", buf[0] ? ", " : "", out->h_wheel);
		if (n < 0 || (size_t)n >= left)
			return;
		p += n;
		left -= (size_t)n;
	}
	if (out->has_middle)
		snprintf(p, left, "%smiddle-click", buf[0] ? ", " : "");
}

enum {
	SESSION_OK = 0,
	SESSION_MISSING = 1,
	SESSION_DISCONNECT = 2,
	SESSION_UINPUT = 3
};

static int run_session(const Settings *settings)
{
	struct libevdev *dev = NULL;
	int fd = -1;
	struct libevdev_uinput *ui = NULL;
	ScrollMapper mapper;
	int grabbed = 0;
	int rc = SESSION_OK;

	if (find_powermate(settings->device, &dev, &fd) != 0)
		return SESSION_MISSING;

	mapper_init(&mapper, settings->step_size, settings->invert,
		    settings->click_middle, settings->hold_for_horizontal);

	if (!settings->print_events) {
		if (open_uinput(&ui) != 0) {
			rc = SESSION_UINPUT;
			goto done;
		}
	}

	if (settings->grab) {
		if (libevdev_grab(dev, LIBEVDEV_GRAB) != 0)
			log_msg("WARNING", "Could not grab device (continuing anyway): %s",
				strerror(errno));
		else
			grabbed = 1;
	}

	set_led(fd, settings->led_brightness);
	log_msg("INFO",
		"Scrolling: clockwise=down, step_size=%d (%s units per detent), "
		"click=%s, hold+turn=%s",
		settings->step_size, settings->step_size < 120 ? "high-res" : "coarse",
		settings->click_middle ? "middle" : "none",
		settings->hold_for_horizontal ? "horizontal" : "off");
	log_msg("INFO", "Hover the cursor over a browser page and turn the knob.");

	while (!g_stop) {
		struct input_event ev;
		int status;
		WheelOutput out;

		status = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
		if (status == LIBEVDEV_READ_STATUS_SYNC) {
			while (status == LIBEVDEV_READ_STATUS_SYNC)
				status = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC,
							     &ev);
			continue;
		}
		if (status == -EINTR)
			continue;
		if (status == -EAGAIN)
			continue;
		if (status < 0) {
			log_msg("WARNING", "PowerMate disconnected (%s); waiting to reconnect",
				strerror(-status));
			rc = SESSION_DISCONNECT;
			break;
		}

		memset(&out, 0, sizeof(out));
		if (ev.type == EV_REL && ev.code == REL_DIAL)
			out = mapper_on_dial(&mapper, ev.value);
		else if (ev.type == EV_KEY && ev.code == BTN_0)
			out = mapper_on_button(&mapper, ev.value != 0);
		else
			continue;

		if (settings->print_events) {
			if (wheel_has_scroll(&out) || out.has_middle) {
				char desc[128];
				describe_output(&out, desc, sizeof(desc));
				printf("%s\n", desc[0] ? desc : "(no-op)");
				fflush(stdout);
			}
			continue;
		}
		inject(ui, &out);
	}

done:
	set_led(fd, 0);
	if (grabbed)
		libevdev_grab(dev, LIBEVDEV_UNGRAB);
	if (ui)
		libevdev_uinput_destroy(ui);
	if (dev)
		libevdev_free(dev);
	if (fd >= 0)
		close(fd);
	return rc;
}

static int serve(const Settings *settings)
{
	while (!g_stop) {
		int rc = run_session(settings);
		if (rc == SESSION_OK || g_stop)
			break;
		if (rc == SESSION_UINPUT)
			return 1;
		if (rc == SESSION_MISSING)
			log_msg("INFO", "Waiting for Griffin PowerMate ...");
		sleep(WAIT_SECONDS);
	}
	if (g_stop)
		log_msg("INFO", "Stopped");
	return 0;
}

static void usage(FILE *fp)
{
	fputs("Usage: powermate-scroll [options]\n"
	      "Turn a Griffin PowerMate into a high-resolution scroll wheel.\n\n"
	      "  --config PATH         TOML config (default: ~/.config/powermate-scroll/config.toml)\n"
	      "  --step-size N         HI_RES units per detent (8=fine, 24=default, 120=coarse)\n"
	      "  --invert              Clockwise scrolls up instead of down\n"
	      "  --no-grab             Do not exclusive-grab the PowerMate event node\n"
	      "  --click middle|none   Short click action (default: middle)\n"
	      "  --no-horizontal       Disable hold-and-turn horizontal scrolling\n"
	      "  --device PATH         Event device path (auto-detected if omitted)\n"
	      "  --led 0-255           Base LED brightness\n"
	      "  --list                List input devices and exit\n"
	      "  --print-events        Print mapped events instead of injecting scroll\n",
	      fp);
}

int main(int argc, char **argv)
{
	Settings settings;
	char config_path[512];
	char err[256];
	int do_list = 0;
	int opt;
	int step_cli = -1;
	int led_cli = -1;
	int invert_cli = 0;
	int no_grab = 0;
	int no_horizontal = 0;
	const char *click_cli = NULL;
	const char *device_cli = NULL;
	const char *config_cli = NULL;

	static const struct option longopts[] = {
		{"config", required_argument, NULL, 'c'},
		{"step-size", required_argument, NULL, 's'},
		{"invert", no_argument, NULL, 'i'},
		{"no-grab", no_argument, NULL, 'g'},
		{"click", required_argument, NULL, 'k'},
		{"no-horizontal", no_argument, NULL, 'H'},
		{"device", required_argument, NULL, 'd'},
		{"led", required_argument, NULL, 'l'},
		{"list", no_argument, NULL, 'L'},
		{"print-events", no_argument, NULL, 'p'},
		{"help", no_argument, NULL, 'h'},
		{0, 0, 0, 0},
	};

	settings_init(&settings);
	settings_default_path(config_path, sizeof(config_path));

	while ((opt = getopt_long(argc, argv, "c:s:ik:Hd:l:Lph", longopts, NULL)) != -1) {
		switch (opt) {
		case 'c':
			config_cli = optarg;
			break;
		case 's':
			step_cli = atoi(optarg);
			break;
		case 'i':
			invert_cli = 1;
			break;
		case 'g':
			no_grab = 1;
			break;
		case 'k':
			click_cli = optarg;
			break;
		case 'H':
			no_horizontal = 1;
			break;
		case 'd':
			device_cli = optarg;
			break;
		case 'l':
			led_cli = atoi(optarg);
			break;
		case 'L':
			do_list = 1;
			break;
		case 'p':
			settings.print_events = 1;
			break;
		case 'h':
			usage(stdout);
			return 0;
		default:
			usage(stderr);
			return 2;
		}
	}

	if (config_cli)
		snprintf(config_path, sizeof(config_path), "%s", config_cli);

	err[0] = '\0';
	if (settings_load_toml(&settings, config_path, err, sizeof(err)) != 0) {
		fprintf(stderr, "%s\n", err);
		return 2;
	}

	if (step_cli >= 0)
		settings.step_size = step_cli;
	if (invert_cli)
		settings.invert = 1;
	if (no_grab)
		settings.grab = 0;
	if (click_cli) {
		if (strcmp(click_cli, "middle") == 0)
			settings.click_middle = 1;
		else if (strcmp(click_cli, "none") == 0)
			settings.click_middle = 0;
		else {
			fprintf(stderr, "click must be 'middle' or 'none'\n");
			return 2;
		}
	}
	if (no_horizontal)
		settings.hold_for_horizontal = 0;
	if (device_cli)
		snprintf(settings.device, sizeof(settings.device), "%s", device_cli);
	if (led_cli >= 0)
		settings.led_brightness = led_cli;

	if (settings.step_size <= 0) {
		fprintf(stderr, "step-size must be a positive integer\n");
		return 2;
	}

	if (do_list)
		return list_input_devices();

	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);
	return serve(&settings);
}
