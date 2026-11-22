<div align="center">

<img src="banner.svg" alt="Lumalarm" width="680">

**Lumalarm** — a smart alarm clock for Linux that wakes your computer from sleep

rtcwake wake scheduling · alarm editor · timer · stopwatch · sound manager

[![License: GPLv3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
[![Qt](https://img.shields.io/badge/Qt-6-41CD52?logo=qt)](https://www.qt.io/)
[![Platform](https://img.shields.io/badge/platform-Linux-informational)]()

[![CI](https://github.com/shinigami1231111/lumalarm/actions/workflows/ci.yml/badge.svg)](https://github.com/shinigami1231111/lumalarm/actions/workflows/ci.yml)
[![Lint](https://github.com/shinigami1231111/lumalarm/actions/workflows/lint.yml/badge.svg)](https://github.com/shinigami1231111/lumalarm/actions/workflows/lint.yml)

</div>

> [!NOTE]
> **Current source version: [v1.0.5](CHANGELOG.md#105---2026-11-22)** (not published on GitHub yet) —
> secure D-Bus wake helper, English / Arabic / Japanese UI, emoji density control, and a frameless,
> mouse-draggable window.

---

## Building & Testing

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
JOBS=$(( $(nproc) * 80 / 100 )); [ "$JOBS" -lt 1 ] && JOBS=1
cmake --build build -j"$JOBS"
ctest --test-dir build --output-on-failure
```

The local mirror of CI is `bash scripts/ci-local.sh` (or `make ci`): same Release build
with KDE blur disabled, then ctest, then a **mandatory** clang-tidy pass locally (the GitHub
Lint workflow is advisory).

Translations are compiled into the binary automatically when Qt Linguist is installed;
see [CONTRIBUTING.md](CONTRIBUTING.md#translations) for how to add or refresh a language.

See [CONTRIBUTING.md](CONTRIBUTING.md) for prerequisites, the CI matrix, the optional
pre-push hook, and everything else.

---

## The idea

**Lumalarm is an alarm clock that turns your Linux PC into a real alarm clock.**

Most alarm apps assume your computer is already awake. Lumalarm doesn't. Hit **Arm Wake** and a privileged D-Bus helper programs the hardware RTC with `rtcwake` and suspends the machine; it wakes back up right before your alarm fires. No leaving the PC running all night just to hear a beep in the morning.

When the alarm fires, a ringing overlay shows its name and time with a **Stop** action, the tone stops automatically after the configured interval, and an optional shell command can run. The timer, stopwatch, sound manager and themable interface are built on top of that core.

---

## Screenshots

Current UI captures.

**Alarms — list and editor**

![Alarms 1](screenshots/alarms-1.jpeg)
![Alarms 2 — alarm editor](screenshots/alarms-2.jpeg)

**Timer**

![Timer](screenshots/timer.jpeg)

**Stopwatch**

![Stopwatch](screenshots/stopwatch.jpeg)

**Sound Manager**

![Sounds](screenshots/sounds.jpeg)

**Settings**

![Settings](screenshots/settings.jpeg)

---

## Features

- **Alarms that wake a sleeping PC** — per-day schedules, a valid `rtcwake` sleep mode, and an optional alarm name
- **Ringing overlay** — shows the alarm name and time with a **Stop** action; tones stop after the configured auto-stop interval
- **Sound manager** — import and preview your own tones (`wav`, `mp3`, `ogg`, `flac`, `aac`)
- **System volume policy** — raise the sink to a configured minimum while an alarm rings and restore it exactly afterwards, or leave system volume untouched
- **Countdown timer & stopwatch** — with laps and a completion sound for the timer
- **Custom commands** — run a configured shell command when an alarm fires
- **Fully themeable** — built-in color pickers for background, accent, text and borders, plus a background-opacity slider and a choice of wheel or spin time picker
- **System tray** — a tray icon with the next alarm in its tooltip and **Show Lumalarm** / **Quit** actions, toggleable from Settings
- **Desktop notifications** — a heads-up before an alarm is due, a notification when it rings and one for an armed test wake, over the freedesktop notification service (no libnotify)
- **Test alarm** — arm a real wake 60 s to 1 h ahead without creating an alarm, to prove the machine actually wakes
- **Night mode** — dims the window a few minutes before the alarm so it never lights up a dark room
- **First-run setup wizard** — four skippable steps on the very first launch: language, wake method, notifications and a summary. Existing installations are never prompted after an upgrade, and `--show-wizard` reopens it any time
- **English, Arabic & Japanese UI** — pick the language in Settings or during setup; Arabic mirrors the layout right-to-left and Japanese is available as a full catalog
- **Emoji density** — Off / Less / More, with legacy settings migrated automatically
- **Frameless window** — drag the title area to move and the edges/corners to resize; no visible bar is added

---

## Table of Contents

- [Building & Testing](#building--testing)
- [The idea](#the-idea)
- [Screenshots](#screenshots)
- [Features](#features)
- [Installation](#installation)
  - [Dependencies](#dependencies)
  - [Build from Source](#build-from-source)
  - [Arch package](#arch-package)
- [Configuration](#configuration)
- [Theming](#theming)
- [Wake Setup (D-Bus helper)](#wake-setup-d-bus-helper)
- [Contributing](#contributing)
- [Changelog](#changelog)
- [License](#license)

---

## Installation

### Dependencies

- **Qt 6.5+** — Core, Gui, Multimedia, Qml, Quick, QuickControls2, Widgets, DBus (Test is needed for the test build)
- **CMake** ≥ 3.21 and **Ninja** for the local CI mirror
- **C++17** compiler (GCC or Clang)
- **KF6WindowSystem** *(optional but recommended for KDE Plasma users)* — provides KWin blur-behind; package `kwindowsystem` (Arch), `libkf6windowsystem-dev` (Debian/Ubuntu), `kf6-kwindowsystem` (Fedora). If absent, the app still builds/runs without KWin-native blur.
- **Qt 6 Linguist tools** *(optional for compiling catalogs, but CI installs them anyway)* — package `qt6-tools` (Arch), `qt6-tools-dev` (Debian/Ubuntu), `qt6-qttools-devel` (Fedora). `lrelease` compiles the committed catalogs and `lupdate` refreshes them; the distro package may also be needed for QML module generation.
- **rtcwake** — from `util-linux`; required for wake-from-suspend. Alarm-only use works without it.

<details>
<summary><b>Arch Linux</b></summary>

```bash
sudo pacman -S --needed base-devel cmake ninja pkgconf qt6-base qt6-declarative qt6-multimedia qt6-quickcontrols2 qt6-tools kwindowsystem
```
</details>

<details>
<summary><b>Ubuntu / Debian</b></summary>

```bash
sudo apt install cmake ninja-build build-essential pkg-config qt6-base-dev qt6-base-dev-tools qt6-declarative-dev qt6-multimedia-dev qt6-tools-dev libkf6windowsystem-dev
```
</details>

<details>
<summary><b>Fedora</b></summary>

```bash
sudo dnf install cmake ninja-build gcc-c++ pkgconf-pkg-config qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtmultimedia-devel qt6-qttools-devel kf6-kwindowsystem
```
</details>

### Build from Source

```bash
git clone https://github.com/shinigami1231111/lumalarm.git
cd lumalarm
cmake -B build -DCMAKE_BUILD_TYPE=Release
JOBS=$(( $(nproc) * 80 / 100 )); [ "$JOBS" -lt 1 ] && JOBS=1
cmake --build build -j"$JOBS"
./build/lumalarm
```

### Arch package

The repository's `PKGBUILD` builds directly from this source tree (`source=()`). The AUR package
named `lumalarm` is still at **1.0.0-10** and predates the D-Bus helper, so `paru -S lumalarm` /
`yay -S lumalarm` will not install this version. From the project root:

```bash
makepkg -si
```

---

## Configuration

Writable app data lives in `~/.config/lumalarm/`; shared tones are also read from the XDG data
directories (the package installs `/usr/share/lumalarm/tones`).

| Path | Purpose |
|---|---|
| `theme.conf` | **Theme** — colors, blur mode, font, corner radius, window buttons (hand-editable, live-reloaded) |
| `settings.ini` | Non-theme behavior — wake mode, time picker, emoji level, language, audio policy, tray/close/night mode, background opacity (`theme/opacity`) |
| `alarms.json` | Alarm list (persisted, human-readable) |
| `scheduler.json` | Wake/test-alarm scheduling state |
| `lumalarm.lock` | Single-instance lock |
| `tones/` | Imported alarm sound files |

---

## Theming

Lumalarm is built for **ricing / tiling-WM users** (Hyprland, Sway, etc.) and uses
**true window-level transparency** — the window surface has a real per-pixel alpha
channel, so a Wayland/X11 compositor can blur the desktop *behind* it. It does **not**
fake transparency with a translucent gray rectangle.

### `theme.conf`

Located at `~/.config/lumalarm/theme.conf` (KDE-style `key=value`, easy to hand-edit
and commit to a dotfiles repo). Editing it **reloads the theme live** — no restart.

| Key | Meaning | Example |
|---|---|---|
| `background_color` | App background (RGB hex) | `#1e1e2e` |
| `accent_color` | Accent / highlight | `#cba6f7` |
| `text_primary` | Primary text | `#cdd6f4` |
| `text_secondary` | Secondary text | `#a6adc8` |
| `border_color` | Window/card border | `#cba6f7` |
| `blur_radius` | (reserved) blur radius hint | `20` |
| `blur_mode` | `compositor` or `app` | `compositor` |
| `font_family` | Nerd/font family, blank = system | `JetBrains Mono` |
| `corner_radius` | Window corner radius (px) | `22` |
| `background_preset` | Background style name | `Dark Glass` |
| `show_minimize_button` / `show_maximize_button` / `show_close_button` / `show_tray_icon` | Window/tray toggles | `true` |
| `current_preset` | Last applied palette preset | `Nord` |

Background opacity is **not** in `theme.conf`; it is `theme/opacity` in `settings.ini` (0.0–1.0, default `0.55`).

### Built-in palette presets

Selectable from **Settings → Appearance → Palette Presets**; each writes a complete
`theme.conf` you can then hand-tune:

- **Catppuccin Mocha**
- **Nord**
- **Gruvbox Dark**
- **Rose Pine**
- **Tokyo Night**

### Blur mode

- **Compositor** (default, for Hyprland/Sway): the window stays genuinely transparent
  and the compositor blurs the desktop behind it.

  - **Hyprland / Sway users:** add a window rule so your compositor actually blurs Lumalarm:

    ```ini
    # Hyprland (~/.config/hypr/hyprland.conf)
    windowrulev2 = blur, class:^(lumalarm)$
    ```

    For Sway, blur through a background tool (e.g. `swaybg` + a blur shader, or a fork
    that supports it); Lumalarm simply exposes the transparent surface. This is the
    only step needed on Hyprland — Hyprland's rule-based blur works on the existing
    transparent window with no extra code.

  - **KDE Plasma (KWin) users:** transparent windows are **not** blurred automatically
    by KWin the way they are under Hyprland. Lumalarm additionally requests blur-behind
    via KWindowSystem, but two things are required on your side:
    1. The **KWindowSystem** runtime dependency must be installed (see Dependencies),
       and Lumalarm must have been built with KDE blur support (it is by default when
       the library is present).
    2. The **"Blur" desktop effect must be enabled** in
       *System Settings → Desktop Effects → Blur*. With it off, KWin shows plain
       transparency (no crash, no broken visuals) — that's expected KWin behavior.
    The blur-behind request is re-applied on resize and whenever you switch blur modes.

- **App** (fallback for X11 / GNOME without compositor blur): Lumalarm simply does
  **not** request KWin blur-behind. Background alpha still follows the Background
  Opacity setting (default `0.55`), so the window remains genuinely translucent.

### pywal import

Manual only (no auto-apply on wallpaper change). From **Settings → Appearance → Import
from pywal**, if `~/.cache/wal/colors.json` exists Lumalarm maps its roles
(`background`, `foreground`, `color1`/`color4`, `color8`) onto the theme fields.
If the file is absent, the button is disabled and nothing happens.

### Fonts

`font_family` is free-text. If the font isn't installed, Lumalarm silently falls back
to the system default — it never crashes on a missing font.

### Alarm tones

To add tones without the GUI:

```bash
cp my-sound.wav ~/.config/lumalarm/tones/
```

---

## Wake Setup (D-Bus helper)

Install a v1.0.5 source-built package (or run `cmake --install build`); the systemd unit,
D-Bus activation file, bus policy and polkit action are installed with it. The current AUR
package is older and does not include this helper. No `/etc/sudoers.d/lumalarm` rule is
needed or used.

Wake scheduling runs behind a privilege boundary. The app calls the `org.lumalarm.Helper`
system service over the system bus; the helper runs as root, validates the delay
(`60 <= seconds <= 86400`) and the mode, then starts a **detached `rtcwake -m <mode> -s <N>`**
which programs the RTC alarm and suspends in one step. It uses `QProcess` argument lists only —
no shell, no `system()`, no configuration. Systemd is not used to arm the wake; `systemctl` is
only consulted to rediscover or stop legacy `lumalarm-wake-*.timer` units after a helper restart.
A desktop user is authorized by the polkit action `org.lumalarm.schedule.wake`
(`auth_admin_keep`); `pkcheck` is called with `--allow-user-interaction` so the authentication
agent can present the challenge.

**Requirement:** `rtcwake` from `util-linux`. A systemd installation is optional.

**Troubleshooting:** if scheduling fails, check the helper log:

```bash
journalctl -u lumalarm-wake-helper
```

**Available sleep modes** (passed through to `rtcwake -m`):

| Mode | Behavior |
|---|---|
| `freeze` | Suspend-to-idle |
| `standby` | Power-on standby (S1) |
| `mem` | Suspend-to-RAM (S3) |
| `disk` | Hibernate (S4) |
| `off` | Power off (S5, where supported) |
| `no` | Set the RTC alarm without suspending |
| `on` | Keep the machine on |

---

## Contributing

Contributions are welcome! Feel free to open an issue or submit a pull request for bug fixes, new features, or theme presets. See [CONTRIBUTING.md](CONTRIBUTING.md) for prerequisites, the CI matrix and the release process.

---

## Changelog

Release history and verified changes live in [CHANGELOG.md](CHANGELOG.md). The current source version is [v1.0.5](CHANGELOG.md#105---2026-11-22); it has not been published on GitHub yet.

---

## License

Licensed under the **GNU General Public License v3.0** — see [LICENSE](LICENSE) for details.
