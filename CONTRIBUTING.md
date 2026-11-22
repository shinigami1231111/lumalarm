# Contributing to Lumalarm

**Lumalarm** is a smart alarm clock for Linux that wakes your computer from sleep —
`rtcwake` wake scheduling through a privileged D-Bus helper, an alarm editor, timer,
stopwatch and a sound manager, written in C++/Qt 6 with a QML front end. Contributions of all kinds are
welcome: bug reports, documentation, UI copy and translations, refactors, tests and
features. Read on for the conventions that keep review cheap.

## Prerequisites

- **Qt >= 6.5** with the modules this project uses: `Core`, `Gui`, `Qml`, `Quick`,
  `QuickControls2`, `Multimedia`, `Widgets`, `Test`, `DBus`
- **CMake >= 3.21**
- A **C++17** compiler (GCC or Clang); CI covers the toolchains shipped by Ubuntu 24.04,
  Fedora 40 and Arch Linux
- Optional: **KF6WindowSystem** (KWin), which enables native KDE blur-behind
- Optional: **Qt Linguist tools** (`lupdate`/`lrelease`, package `qt6-tools` on Arch),
  needed only to compile or refresh translations — see [Translations](#translations).
  Without them the build still succeeds and the UI stays in English.
- Platforms: developed and tested on Arch Linux. Ubuntu 24.04 and Fedora 40 are
  expected to work; please report what breaks elsewhere.

On Arch:

```sh
sudo pacman -S --needed base-devel cmake ninja pkgconf qt6-base qt6-declarative qt6-multimedia qt6-tools
```

## Building

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
JOBS=$(( $(nproc) * 80 / 100 )); [ "$JOBS" -lt 1 ] && JOBS=1
cmake --build build -j"$JOBS"
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
| `tst_qmlresources` | every QML file listed in `QML_FILES` and every icon referenced from QML resolves under the `GlassAlarm` resource prefix; installed app icons are checked by packaging, not this test |
| `tst_systemmixer` | `SystemMixer` (audio): backend fallback order, parsers, the async CLI runner with timeout, `FakeMixer` |
| `tst_volumepolicy` | `VolumePolicy` (audio): raise/clamp decisions, single-factor balance, 3 % override tolerance |
| `tst_restorejournal` | `RestoreJournal` (audio): write-ahead record, atomic 0600 file, compare-and-set crash recovery |
| `tst_soundresolver` | `SoundResolver` (audio): per-alarm → global default → first tone → empty |
| `tst_alarmplayer` | `AlarmPlayer` (audio): media seam, stream-volume clamp (never > 1.0), mandatory stream verification/forcing |
| `tst_mprispause` | `MprisPause` (audio): MPRIS bus-name filter, no-bus no-op, resume-only-what-we-paused |
| `tst_audiocontroller` | `AlarmAudioController` (audio): ring flow end to end — journal before raise, exact restore, alarmOnly, user override, bounded quit, startup recovery |
| `tst_configmanager` | `ConfigManager` (managers): settings round-trips, `timePickerMode` normalization |
| `tst_thememanager` | `ThemeManager` (managers): seeded defaults, palette presets, theme file round-trip |
| `tst_blurmanager` | `BlurManager` (ui): construction and null-window contract |
| `tst_wakemanager` | `WakeManager` (wake): test-alarm range validation plus the arm/cancel surface; the privileged D-Bus cases skip when no helper is present |
| `tst_stg1security` | wake-helper security contract: polkit action and argument list, `60 <= seconds <= 86400` bounds, D-Bus/polkit/systemd file and install contract, live bounds/replace/restart/denial; the helper itself is rtcwake-only |
| `tst_wakecaps` | fixture-based `WakePolicy` capability parsing: sleep states, `mem_sleep` default, numeric RTC ordering, wake-capable RTC selection and `capabilitiesToVariantMap()` |
| `tst_notifier` | `Notifier` (ui): construction, master switch and the no-bus path (live delivery is checked manually with `busctl --user monitor`) |
| `tst_trayicon` | `TrayController` (ui): construction, `usable()` contract and tooltip text on a headless box (offscreen platform, no system tray) |
| `tst_nightmode` | `NightMode` (ui): edge-trigger semantics, settings round-trip and lead-window clamping |
| `tst_wizard` | `WizardController` (ui): step machine, next/back gates, first-run vs. existing-profile detection, the `wizard/completed` flag, language selection, and the guarantee that a wizard run writes no alarm |
| `tst_i18n` | i18n: English source fallback, the embedded Arabic and Japanese catalogs at `:/i18n`, `QQmlEngine::retranslate()`, the `ui/language` round-trip and `.ts` catalog consistency |

Any PR must keep all tests green. New behavior should come with a new test.

## Translations

Every user-facing string goes through `qsTr()` in QML and `tr()` in C++. The catalogs
live in `translations/`:

| File | Contents |
|---|---|
| `translations/lumalarm_en.ts` | source-language template — English *is* the source, so its entries stay `type="unfinished"` by design |
| `translations/lumalarm_ar.ts` | Arabic catalog, kept at 0 unfinished entries |
| `translations/lumalarm_ja.ts` | Japanese catalog, kept at 0 unfinished entries |

```sh
cmake --build build --target update_translations   # re-run lupdate over the sources
cmake --build build                                # lrelease + embed the .qm files
```

`update_translations` is opt-in: a normal build only compiles the committed `.ts`
files and never rewrites them, so building cannot dirty the working tree.

Rules for string changes:

- Wrap anything a user can read — `qsTr("...")` in QML, `tr("...")` in C++. Leave logs
  and `qWarning()` output alone, along with product names ("Lumalarm"), palette preset
  names, and any combo whose `currentText` is stored verbatim as a setting (translating
  those would corrupt the value; combos selected by `currentIndex` are safe to
  translate).
- Keep the trailing space in `"h "` and the leading space in `" min"` — these countdown
  fragments are asserted by `tst_i18n`. Keep the trailing space in `"Tones folder: "`,
  which prefixes the tones directory (not covered by that test).
- After adding a string, run `update_translations` and translate the new entry in the
  same commit. `tst_i18n` fails on any `type="unfinished"` entry in `lumalarm_ar.ts` or
  `lumalarm_ja.ts`, so an untranslated string cannot ship unnoticed.
- `Qt6::LinguistTools` is found **optionally** in `CMakeLists.txt`, so a machine or CI
  image without `lupdate`/`lrelease` still configures, builds and runs; only the
  catalog-dependent cases of `tst_i18n` report a skip there.

Right-to-left: `main.cpp` sets the application layout direction from the *loaded*
catalog, and `qml/main.qml` enables `LayoutMirroring` from
`Qt.application.layoutDirection`, so Arabic mirrors the whole window. Japanese is a
left-to-right catalog and loads for both `ja` and `ja_JP`; RTL mirroring applies to Arabic,
or whenever `mirrorApp` is enabled.

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
Release build, blur OFF, then the full test suite, then a **mandatory** clang-tidy pass that
fails the run on any finding (skip with `--no-lint`). Only GitHub `lint.yml` is advisory.
It needs no Docker, no containers and no network.

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

`.clang-format` and `.clang-tidy` are at the repository root. `.clang-format` is a target
for touched code, not a gate. Lint is advisory in GitHub `lint.yml` only; `ci-local.sh`
treats clang-tidy findings as failures unless `--no-lint` is passed (`.clang-tidy` sets
`WarningsAsErrors: '*'`). Do not reformat untouched files in a pull request.

## Project structure

```
src/
  main.cpp
  app/       AppLifecycle, SingleInstance
  models/    Alarm, AlarmModel
  managers/  AlarmManager, ConfigManager, ThemeManager, TonePaths.h
  audio/     AlarmAudioController, SystemMixer (+ pactl/wpctl/amixer/NULL
             backends), MixerBackend, FakeMixer, VolumePolicy, RestoreJournal,
             SoundResolver, AlarmPlayer, MprisPause
  wake/      Scheduler, SchedulerState, WakeManager, WakeCaps, WakeSecurity.h
  ui/        BlurManager, Notifier, TrayController, NightMode, WizardController
qml/
  main.qml
  NightDim.qml
  components/   GlassButton, GlassCard, GlassCheckBox, RoundedCombo,
                SegmentedControl, ToggleSwitch, Tokens, WindowResizeHandle
  pages/        AlarmEditor, AlarmListView, SettingsPage, StopwatchPage, TimerPage
  pickers/      ClockFace, WheelTimePicker
  overlays/     RingingOverlay, RingingWindow, SoundManager, CloseDialog
  wizard/       WizardOverlay, WizardPage, pages/ (the four setup steps:
                Language, WakeMethod, Notification, Done)
translations/   lumalarm_en.ts, lumalarm_ar.ts, lumalarm_ja.ts
helper/    lumalarm-wake-helper.cpp   (privileged D-Bus wake helper)
data/      D-Bus activation + policy + interface XML, Polkit action, systemd unit
tests/     one .cpp per test binary (22 in total)
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
  `tests/tst_qmlresources.cpp`, which fails `ctest` if a listed QML file or
  icon goes missing.

## Code style

- **C++**: match the surrounding code — 4 spaces, Qt naming (camelCase methods and
  parameters, `m_` prefix for members, PascalCase classes), one class per header/source
  pair where needed. Header-only value types and policy helpers (`Alarm.h`,
  `WakeSecurity.h`, `TonePaths.h`) are established exceptions.
- **QML**: PascalCase file names, camelCase property names, 4-space indent, keep
  components in the right sub-directory.
- **Formatting**: `.clang-format` and `.clang-tidy` are configured at the repository
  root — see [Formatting and lint configs](#formatting-and-lint-configs). The
  pre-existing code is not fully reformatted yet, so run them on the code you touch
  instead of reformatting a whole file.
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

Releases are cut from `main`; the tag is created after all in-flight tasks are merged and
`CHANGELOG.md` has a dated section for the version. Version metadata lives in
`CMakeLists.txt`, `PKGBUILD` and the local-only `PKGBUILD.local`; keep `pkgrel` and the
`project(... VERSION ...)` in sync. Builds are capped at 80% of available CPUs in every
tracked build command.

1. Check the local state: `git log --oneline origin/main..HEAD`, `git status --short`, `git tag -l`.
2. Push commits with the SSH deploy remote: `git push deploy main`
   (`origin` is HTTPS and does not use the generated deploy key).
3. Create and push an annotated tag:
   `git tag -a vX.Y.Z -m "Release X.Y.Z — ..."` then `git push deploy vX.Y.Z`.
4. Optionally publish the release notes straight from the changelog:
   `gh release create vX.Y.Z --title "vX.Y.Z — ..." --notes-file <(sed -n '/## \[X.Y.Z\]/,/## \[/p' CHANGELOG.md | head -n -1)`

Do not tag, push or create a GitHub release without being asked. Local, git-ignored release
checklists may be kept outside the source tree; they are not part of the repository.

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
