# Contributing to Lumalarm

**Lumalarm** is a smart alarm clock for Linux that wakes your computer from sleep —
suspend-to-RAM wake scheduling, anti-oversleep challenges, timer, stopwatch and a sound
manager, written in C++/Qt 6 with a QML front end. Contributions of all kinds are
welcome: bug reports, documentation, UI copy and translations, refactors, tests and
features. Read on for the conventions that keep review cheap.

## Prerequisites

- **Qt >= 6.5** with the modules this project uses: `Core`, `Gui`, `Qml`, `Quick`,
  `QuickControls2`, `Multimedia`, `Widgets`, `Test`, `DBus`
- **CMake >= 3.21**
- A **C++17** compiler — GCC 11+ or Clang 14+
- Optional: **KF6WindowSystem** (KWin), which enables native KDE blur-behind
- Platforms: developed and tested on Arch Linux. Ubuntu 24.04 and Fedora 40 are
  expected to work; please report what breaks elsewhere.

On Arch:

```sh
sudo pacman -S --needed base-devel cmake qt6-base qt6-declarative qt6-multimedia qt6-tools
```

## Building

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

The KDE blur-behind integration is optional and auto-detected. Force it off with:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DLUMALARM_ENABLE_KDE_BLUR=OFF
```

Arch packaging is driven by `PKGBUILD` at the repository root; keep it in sync when you
add or remove installed files.

## Running

```sh
./build/lumalarm
```

The app is a normal desktop application — run it inside your session (Wayland or X11),
not from a bare TTY.

## Testing

```sh
ctest --test-dir build --output-on-failure
```

| Test binary | What it covers |
|---|---|
| `tst_schedulerlogic` | `Scheduler`, `SchedulerState`, `WakeManager` (wake module) and `AlarmManager` (managers module): alarm/wake state machine |
| `tst_alarmmodel` | `AlarmModel` (models): rows, signals, roles, sort order, count/get shape |
| `tst_selectionbehavior` | `AlarmModel` (models): `selectedId` selection semantics |
| `tst_qmlresources` | every shipped QML file and icon resolves inside the app's embedded QML module resources |
| `tst_audioplayer` | `AudioPlayer` (audio): defaults, property clamping, missing-file error path |
| `tst_configmanager` | `ConfigManager` (managers): settings round-trips, `timePickerMode` normalization |
| `tst_thememanager` | `ThemeManager` (managers): seeded defaults, palette presets, theme file round-trip |
| `tst_blurmanager` | `BlurManager` (ui): construction and null-window contract |

Any PR must keep all tests green. New behavior should come with a new test.

## Continuous Integration

CI runs on every push and pull request to `main` via GitHub Actions.

| Workflow | What it does |
|---|---|
| `.github/workflows/ci.yml` | Release build + `ctest` on three Linux environments |
| `.github/workflows/lint.yml` | advisory `clang-tidy` over `src/` (`continue-on-error: true`) |

The build matrix covers **Ubuntu 24.04** (host runner), **Fedora 40** and **Arch Linux**
(containers). KDE blur-behind is disabled (`-DLUMALARM_ENABLE_KDE_BLUR=OFF`) in every
leg so the build stays portable across all three.

### Running CI locally

`bash scripts/ci-local.sh` runs the same configure + build + `ctest` flow on your host —
Release build, blur OFF, then the full test suite, then an advisory lint pass. It needs
no Docker, no containers and no network.

| Command | What it does |
|---|---|
| `make ci` | full run: `bash scripts/ci-local.sh` |
| `bash scripts/ci-local.sh --no-lint` | skip the clang-tidy step |
| `make ci-fast` | same as `--no-lint` |
| `make clean-ci` | `rm -rf build-local-ci` |

Add `--keep-build` to reuse the previous build directory for a fast incremental run
(`bash scripts/ci-local.sh --keep-build`). `bash scripts/ci-local.sh --help` lists
everything.

### Optional pre-push hook

```sh
bash scripts/install-hooks.sh              # install
bash scripts/install-hooks.sh --uninstall  # remove
```

The hook runs `ci-local.sh --no-lint` before every push and aborts the push when it
fails. It is entirely opt-in, lives in `.git/hooks/pre-push` and is therefore never
committed. A foreign `pre-push` hook is backed up to `pre-push.bak` instead of being
deleted, and `--uninstall` refuses to remove a hook that Lumalarm did not install.

### Formatting and lint configs

`.clang-format` and `.clang-tidy` are at the repository root. The existing code is not
yet fully reformatted, so treat them as a **target, not a gate** — use them to keep new
code consistent, and do not reformat untouched files in a pull request. Lint findings
are advisory everywhere today (`ci-local.sh` reports them, `lint.yml` is
`continue-on-error: true`).

## Project structure

```
src/
  main.cpp
  models/    Alarm, AlarmModel
  managers/  AlarmManager, ConfigManager, ThemeManager
  audio/     AudioPlayer
  wake/      Scheduler, SchedulerState, WakeManager
  ui/        BlurManager
qml/
  main.qml
  components/   GlassButton, GlassCard, GlassCheckBox, RoundedCombo,
                SegmentedControl, ToggleSwitch
  pages/        AlarmEditor, AlarmListView, SettingsPage, StopwatchPage, TimerPage
  pickers/      ClockFace, WheelTimePicker
  overlays/     RingingOverlay, SoundManager
helper/    lumalarm-wake-helper.cpp   (privileged D-Bus wake helper)
data/      D-Bus activation + policy, Polkit action, systemd unit
tests/     one .cpp per test binary (8 in total)
resources/ icons, .qrc, .desktop file, app icons
```

Layout rules:

- C++ includes are rooted at `src/` — write `#include "wake/Scheduler.h"`, never
  `../wake/Scheduler.h` (e.g. see `src/wake/WakeManager.cpp`).
- QML types resolve through `import GlassAlarm`; there are no relative file imports.
- QML resources are served from the `GlassAlarm` QML module prefix required by Qt
  policy QTP0001, and the resource paths mirror the source layout: `qml/main.qml`
  ships inside that prefix, `qml/pages/AlarmEditor.qml` likewise under its
  `pages` sub-directory. Any literal resource path you add must be covered by
  `tests/tst_qmlresources.cpp`, which fails the build if a listed QML file or
  icon goes missing.

## Code style

- **C++**: match the surrounding code — 4 spaces, Qt naming (camelCase methods and
  parameters, `m_` prefix for members, PascalCase classes), one class per file pair
  (`.h`/`.cpp`).
- **QML**: PascalCase file names, camelCase property names, 4-space indent, keep
  components in the right sub-directory.
- There is **no automated formatter configured** — no `.clang-format` at the repo root —
  so match the neighbors of the code you touch.
- Do not reformat unrelated code in a pull request; it buries the real change.

## Commit conventions

Use [Conventional Commits](https://www.conventionalcommits.org/): `feat`, `fix`,
`refactor`, `test`, `docs`, `chore`, `perf`, `build`, `ci`.

One logical change per commit; prefer several small commits over one large one. Examples
from this repository's history:

```
feat(security): add D-Bus wake helper, remove sudoers requirement
refactor(src): reorganize into models/managers/audio/wake/ui modules
test: add per-module unit tests
```

## Release process

Releases are cut from `main`; the tag is created after all in-flight tasks are merged.
See `RELEASE_DEFERRED.md` at the repository root for the exact command sequence, and
`CHANGELOG.md` for what each version contains. Do not tag or push without being asked.

## Privileged components (D-Bus helper)

`helper/` and `data/` implement the privileged wake helper, which replaces the old
`sudoers` NOPASSWD `rtcwake` rule:

- D-Bus system service `org.lumalarm.Helper` — activation file
  `data/org.lumalarm.Helper.service`, interface definition
  `data/org.lumalarm.Helper.xml`, and bus policy `data/org.lumalarm.Helper.conf`
  (without that policy the system bus denies `own`/`method_call` and the helper cannot
  register its name)
- Polkit action `org.lumalarm.schedule.wake` in `data/org.lumalarm.schedule.policy`
- systemd unit `data/lumalarm-wake-helper.service`

Rules for changes here:

- These files run as **root**. They get extra review — call that out in the PR.
- **Never** introduce `setuid` binaries.
- **Never** call `system()`, and never build shell command strings by concatenation.
  Use `QProcess` with an explicit argument list (or D-Bus), as the helper already does.
- Keep the client/helper contract in sync: interface XML in `data/`, implementation in
  `helper/lumalarm-wake-helper.cpp`, and the client side in `src/wake/WakeManager.cpp`.

## Reporting bugs

Please include:

- OS and version (e.g. `uname -a`, distribution release)
- Qt version — `qmake6 --version`, or `qtpaths6 --qt-version` where `qtpaths6` is
  installed
- Lumalarm version — the app has no `--version` flag, so use `git describe --tags`
  from a checkout
- Exact steps to reproduce, and what you expected instead
- Relevant log output. For wake-related problems, the helper runs as its own unit:

  ```sh
  journalctl -u lumalarm-wake-helper
  ```

## License

Lumalarm is released under the **GNU General Public License v3.0** — see the `LICENSE`
file at the repository root for the full text (it also carries the copyright line for
the project). Contributions are accepted under those same terms.
