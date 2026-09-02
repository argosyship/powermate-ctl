#!/usr/bin/env bash
# Install powermate-scroll on CachyOS / Arch (and other systemd Linux desktops).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
BIN="/usr/local/bin/powermate-scroll"

if [[ ! -f "${ROOT}/src/main.c" || ! -f "${ROOT}/Makefile" ]]; then
  echo "This installer must be run from the powermate-scroll project folder."
  echo "Current directory: $(pwd)"
  echo "Look for a folder that contains install.sh, Makefile, and src/main.c"
  exit 1
fi

if [[ "${EUID}" -eq 0 ]]; then
  echo "Run this as your normal user. It will sudo when it needs root."
  exit 1
fi

need_pkgs=()
if ! command -v gcc >/dev/null 2>&1 || ! command -v make >/dev/null 2>&1; then
  need_pkgs+=(base-devel)
fi
if ! command -v pkg-config >/dev/null 2>&1; then
  need_pkgs+=(pkgconf)
fi
if ! pkg-config --exists libevdev 2>/dev/null; then
  need_pkgs+=(libevdev)
fi
if ((${#need_pkgs[@]})) && command -v pacman >/dev/null 2>&1; then
  echo "Installing build dependencies: ${need_pkgs[*]}"
  sudo pacman -S --needed "${need_pkgs[@]}"
elif ((${#need_pkgs[@]})); then
  echo "Missing: ${need_pkgs[*]}"
  echo "On CachyOS / Arch: sudo pacman -S base-devel pkgconf libevdev"
  exit 1
fi

echo "Building powermate-scroll"
make -C "${ROOT}"

echo "Installing program to ${BIN}"
sudo install -Dm755 "${ROOT}/powermate-scroll" "${BIN}"
sudo rm -rf /usr/local/lib/powermate-scroll

echo "Installing udev rules and uinput module load"
sudo install -Dm644 "${ROOT}/packaging/99-powermate-scroll.rules" \
  /etc/udev/rules.d/99-powermate-scroll.rules
sudo install -Dm644 "${ROOT}/packaging/uinput.conf" \
  /etc/modules-load.d/powermate-scroll.conf
sudo modprobe uinput || true
sudo udevadm control --reload-rules
sudo udevadm trigger

added_input=0
if getent group input >/dev/null; then
  if ! id -nG "${USER}" | grep -qw input; then
    echo "Adding ${USER} to the input group (log out and back in afterwards)"
    sudo usermod -aG input "${USER}"
    added_input=1
  fi
fi

mkdir -p "${XDG_CONFIG_HOME:-$HOME/.config}/powermate-scroll"
if [[ ! -f "${XDG_CONFIG_HOME:-$HOME/.config}/powermate-scroll/config.toml" ]]; then
  cp "${ROOT}/config.example.toml" \
    "${XDG_CONFIG_HOME:-$HOME/.config}/powermate-scroll/config.toml"
  echo "Wrote ~/.config/powermate-scroll/config.toml"
fi

sudo install -Dm644 "${ROOT}/packaging/powermate-scroll.service" \
  /usr/lib/systemd/user/powermate-scroll.service
mkdir -p "${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user"
install -Dm644 "${ROOT}/packaging/powermate-scroll.service" \
  "${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user/powermate-scroll.service"

systemctl --user daemon-reload
systemctl --user enable powermate-scroll.service
# enable --now does not restart an already-running unit (e.g. an older install).
systemctl --user restart powermate-scroll.service
systemctl --user --no-pager --full status powermate-scroll.service || true

echo
echo "============================================================"
echo "Installed the scroller to: ${BIN}"
echo "============================================================"
if ((added_input)); then
  echo "Log out and back in so the input group applies, then hover a page"
  echo "and turn the knob (do not press it)."
else
  echo "Hover a browser page and turn the knob (do not press it)."
fi
echo "Config: ~/.config/powermate-scroll/config.toml"
echo
"${BIN}" --list || true
ls -l "${BIN}"
file "${BIN}"
