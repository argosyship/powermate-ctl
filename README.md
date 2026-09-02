# PowerMate high-resolution scroll

[![test](https://github.com/argosyship/powermate-ctl/actions/workflows/test.yml/badge.svg)](https://github.com/argosyship/powermate-ctl/actions/workflows/test.yml)

Userspace **C** daemon for the **classic USB Griffin PowerMate**. It reads the in-kernel `powermate` driver (`REL_DIAL`, about 94 steps per turn) and injects a virtual mouse wheel using `REL_WHEEL_HI_RES` — 120 units per traditional notch, so the knob can scroll a browser more smoothly than a notched wheel.

This repository is `powermate-ctl`. The installed command and systemd unit are **`powermate-scroll`**.

> Originally prototyped in Python, this daemon was ported to C to drop the interpreter dependency and start faster.

## What you get

| Action | Result |
| --- | --- |
| Turn | High-resolution vertical scroll (clockwise = down) |
| Hold and turn | Horizontal scroll |
| Click without turning | Middle-click (open link in a new tab / autoscroll) |

Hover the cursor over the page. The compositor treats this like a real high-res mouse wheel, so Firefox, Chromium, and other apps scroll the window under the pointer.

## Install

Works on CachyOS / Arch and other systemd Linux desktops. You need a **classic USB Griffin PowerMate** (`077d:0410`) and the in-kernel `powermate` driver (`CONFIG_INPUT_POWERMATE`).

Run `./install.sh` from this project folder (the directory that contains `install.sh`, `Makefile`, and `src/main.c`):

```bash
git clone https://github.com/argosyship/powermate-ctl.git
cd powermate-ctl
./install.sh
```

On CachyOS / Arch the installer will install `base-devel`, `pkgconf`, and `libevdev` if they are missing. On other systemd machines, install a C compiler, `pkg-config`, and `libevdev` first.

The installer builds the C binary, copies it to `/usr/local/bin/powermate-scroll`, installs the udev rule and `uinput` module load, writes `~/.config/powermate-scroll/config.toml` if missing, and enables (and restarts) the user service.

If this was the first time you were added to the `input` group, **log out and back in**, then:

```bash
lsusb | grep -i griffin          # 077d:0410
powermate-scroll --list          # should mark [PowerMate]
systemctl --user status powermate-scroll
```

Hover a browser page and turn the knob (do not press it).

## Tune the resolution

Edit `~/.config/powermate-scroll/config.toml` (created from `config.example.toml`):

```toml
# HI_RES units per PowerMate detent. 120 = one mouse-wheel click.
# Lower is finer. The USB PowerMate has ~94 detents per revolution.
step_size = 24
```

| `step_size` | Feel |
| ---: | --- |
| 8 | Very fine / slower |
| 16 | Smooth reading |
| 24 | Default (~19 wheel notches per full turn, 94 micro-steps) |
| 48 | Fast |
| 120 | Coarse, one notch per detent |

```bash
systemctl --user restart powermate-scroll
```

Or run once in the foreground:

```bash
powermate-scroll --step-size 8 --print-events
```

`--print-events` prints mapped events without injecting input.

## Run without installing

```bash
sudo pacman -S --needed base-devel pkgconf libevdev
make
./powermate-scroll --list
./powermate-scroll --step-size 16
```

You still need read access to the PowerMate event node and write access to `/dev/uinput` (udev rule + `input` group, or run as root for a quick test).

## How it works

1. Linux already ships `drivers/input/misc/powermate.c`. No out-of-tree kernel module.
2. This C daemon (`libevdev` + `uinput`) grabs that event device so Plasma/GNOME do not also consume `REL_DIAL`.
3. Each detent becomes `step_size` units of `REL_WHEEL_HI_RES` on a virtual pointer. When the total crosses 120, it also emits legacy `REL_WHEEL` for apps that ignore high-res events.
4. Wayland compositors expose that as `wl_pointer.axis_value120`, which Firefox and Chromium use for pixel-smooth scrolling.

## Troubleshooting

**Service says `Waiting for Griffin PowerMate` but `lsusb` shows it**  
This login does not have the `input` group yet. Confirm with `id` (no `input`) vs `getent group input` (your user is listed). Log out and back in. Same-session test:

```bash
systemctl --user stop powermate-scroll
newgrp input
powermate-scroll
```

Unplugging and replugging the knob after install can apply the udev `uaccess` ACL without a logout. A new session is the reliable fix.

**`powermate-scroll --list` omits the device**  
Same `input` group issue as above, or the kernel module is missing:

```bash
lsmod | grep powermate
sudo modprobe powermate
zgrep CONFIG_INPUT_POWERMATE /proc/config.gz
```

**Knob does nothing in the browser**  
Confirm `--list` shows `[PowerMate]`, the user service is running (`systemctl --user status powermate-scroll`), and the cursor is over the page. Try `--step-size 120` to rule out a compositor that only honors discrete notches.

**Permission denied on `/dev/uinput` or `/dev/input/event*`**  
Re-run `./install.sh`, then log out. Unplug and replug the knob. Quick test: `sudo ./powermate-scroll`.

**Unit could not be found**  
Run `./install.sh` again from this repo, or copy `packaging/powermate-scroll.service` to `/usr/lib/systemd/user/` and then:

```bash
systemctl --user daemon-reload
systemctl --user enable --now powermate-scroll
```

**Too fast or too slow**  
Change `step_size`. Firefox also has `mousewheel.default.delta_multiplier_y` in `about:config` if only Firefox feels wrong.

**LED stays dark**  
The daemon sets a static glow while it is running. A grab or permissions problem on the event node will skip the LED write; scrolling can still work.

## Development

```bash
make test
```

No PowerMate is required for the unit tests; they cover the high-res accumulation math, click/hold gestures, and TOML config.
