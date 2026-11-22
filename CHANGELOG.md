# Changelog

All notable changes to Lumalarm are documented here.
Format based on [Keep a Changelog](https://keepachangelog.com/).

> [!NOTE]
> **Latest release: [v1.0.5](#105---2026-11-22) — 2026-11-22.** Secure D-Bus wake helper,
> English / Arabic / Japanese UI, emoji density control, frameless move & resize, and an
> 80%-CPU-capped build. The full record, including gate evidence, is below.

## [Unreleased]

## [1.0.5] - 2026-11-22

### Added

- Japanese catalog (`translations/lumalarm_ja.ts`) embedded and selectable, so
  `ja` / `ja_JP` load a complete left-to-right catalog with live retranslation instead of
  falling back to English.
- Three-level emoji density (**Off / Less / More**) wired through
  `ConfigManager::emoji()`, replacing the old boolean; **More** is now the default and
  legacy `ui/showEmojis` values migrate to it.
- Frameless window move and resize: the title area uses `startSystemMove()` and eight
  transparent edge/corner strips use `startSystemResize(Qt.Edge)`, with a clamped
  geometry fallback on Qt 6.5. No visible bar is added; the five main tabs now rely
  on their existing SVG icons only.
- Source tree reorganized into `src/models`, `src/managers`, `src/audio`,
  `src/wake` and `src/ui` (with `src/app` added later), and QML into `qml/components`, `qml/pages`,
  `qml/pickers`, `qml/overlays` and `qml/wizard` (STG-3). Module
  boundaries are now enforced by tests instead of convention.
- A 22-binary test suite: alarm/wake state machine, alarm model and
  selection, audio player, config, theme, blur, QML resource layout,
  test-alarm surface, notifier, tray icon, night mode, wizard, i18n,
  helper security and helper capabilities.
- `CONTRIBUTING.md` — prerequisites, CI matrix, project layout, code
  style, commit conventions and the privileged-components rules — plus
  CI badges in the README.
- Continuous integration: `.github/workflows/ci.yml` (Release build and
  `ctest` on Ubuntu 26.04, Fedora 40 and Arch) and
  `.github/workflows/lint.yml` (advisory clang-tidy), with a
  container-free local mirror in `scripts/ci-local.sh` (`make ci`), an
  optional pre-push hook (`scripts/install-hooks.sh`), and
  `.clang-format` / `.clang-tidy` at the repository root.
- Test alarm (STG-5.1): arm a real wake 60–3600 s ahead without creating
  an alarm, to prove the machine actually wakes, and cancel it again —
  `WakeManager::armTestWake()` / `cancelTestWake()`.
- Desktop notifications (STG-5.3): upcoming alarm (at the threshold in
  `scheduler.json`), fired alarm and armed test wake, spoken directly
  over `org.freedesktop.Notifications` — no libnotify dependency.
  `Scheduler::alarmApproaching` is the signal behind them.
- System tray icon (STG-5.4) showing the next alarm in its tooltip, with
  a **Show Lumalarm** / Quit menu and a Settings toggle.
- Automatic pre-alarm screen dimming (STG-5.6): `NightMode` plus
  `qml/NightDim.qml` hold a translucent overlay for the configured lead
  time before the next alarm, until it fires or is cancelled, disabled
  or deleted. Per-window only; no backlight or compositor-specific API.
- First-run setup wizard (STG-5.5): four skippable steps (language, wake method,
  notification probe, summary) shown once per installation and re-runnable via
  `--show-wizard`.
  New key `wizard/completed` and the `--show-wizard` flag. Only a profile with
  no `settings.ini` and no `theme.conf` counts as a first run, so existing
  installations are never prompted after an upgrade.
- Translations (STG-5.2): every user-facing string now goes through
  `qsTr()` / `tr()`, with a complete Arabic catalog embedded under
  `:/i18n` and a Language selector in Settings that swaps catalogs and
  re-evaluates live bindings without restarting. New key `ui/language`
  (default: follow the system locale) and automatic right-to-left
  mirroring when the Arabic catalog loads. Qt Linguist is an optional
  **build** tool — without it the app builds and runs in English.
- D-Bus wake helper (STG-1.1): removes the /etc/sudoers.d/lumalarm
  requirement entirely. Scheduling now goes through a privileged
  helper with strict input validation (60 <= seconds <= 86400).
- Polkit action org.lumalarm.schedule.wake for authenticated scheduling.
- systemd activation files for the helper (D-Bus system service).

### Changed

- Local and CI builds are capped at **80% CPU**: `max(1, floor(nproc * 80 / 100))`
  jobs in `scripts/ci-local.sh`, README, CONTRIBUTING, PKGBUILD and the GitHub Actions
  workflow. No tracked build command uses `-j$(nproc)`.
- Source export hygiene: removed a developer home path from `WheelTimePicker.qml` and
  replaced real user/host names in the system-mixer fixture with generic values.
- `.clang-format` now matches the style actually used in the tree, and
  the two noisiest clang-tidy checks were dropped; 28 `bugprone-*`
  findings were resolved in `src/` (follow-up A1–A4). Lint is advisory in GitHub
  `lint.yml`; local `ci-local.sh` treats clang-tidy findings as failures unless
  `--no-lint` is passed.
- WakeManager now uses QDBusInterface to reach the helper, falling
  back to direct rtcwake if the helper is unavailable.
- Wake verification switched from RTC readback to querying systemd timer units
  with `systemctl list-units --all --no-legend --plain --full 'lumalarm-wake-*.timer'`
  while the system is awake; the RTC check remains the hardware source of truth
  for the rtcwake path (see "Known limitations").
- PKGBUILD hooks moved to lumalarm.install (inline makepkg hooks are no-ops).

### Removed

- README instructions for editing /etc/sudoers.d/lumalarm.

### Fixed

- The `showEmojis` setting was dead; it now gates nine labels/buttons and all decorative
  glyphs, with a static guard against raw emoji literals.
- The frameless window could not be moved or resized with the mouse without native
  decorations; native system gestures plus invisible handles now provide both.
- WheelTimePicker quirks repaired, and a secondary spin-style time
  picker added as an alternative to the wheels (`timePickerMode`).
- Toggling an alarm no longer loses its id: `setAlarmEnabled` is
  id-based instead of index-based, so a model refresh between click and
  write could not disable the wrong row.
- GitHub CI: the Ubuntu leg now builds in an Ubuntu 26.04 container because
  Ubuntu 24.04 ships Qt 6.4.2, below the project's Qt 6.5 minimum. Package-manager
  detection no longer assumes only dnf/pacman can run inside containers.
- `tst_stg1security` no longer fails in a clean checkout when the git-ignored
  developer file `PKGBUILD.local` is absent; it is audited when present.

### Verified

<details>
<summary><b>Verified items and evidence — expand to see the full list</b></summary>

All items below have recorded gate evidence in the project's verification notes.
Items marked *manual* also have user-observed pass evidence.

- **STG-1 Security** — D-Bus wake helper, polkit action, `60 <= seconds <= 86400`,
  rtcwake-only privileged helper (with a direct unprivileged rtcwake fallback in
  `WakeManager`), sudoers removal; `tst_stg1security` 22/22 and live manual 7/7.
- **STG-2 UI / Build** — Qt 6.5 setup, QTP0001 QML paths, id-safe alarm toggle, version 1.0.5,
  manual UI smoke 7/7.
- **STG-3 Architecture** — `QAbstractListModel` AlarmModel, `src/` and `qml/` reorganization,
  QML resource-layout negative tests, `CONTRIBUTING.md`.
- **STG-4 CI** — `ci.yml` / `lint.yml`, `ci-local.sh`, Makefile, clang-format / clang-tidy,
  badges; local runner PASS. GitHub-side CI activates only after the first push.
- **STG-5.1 Test mode** — arm / cancel / re-arm, custom duration handling and a real 60 s wake,
  confirmed by the user.
- **STG-5.2 i18n (English source + Arabic / Japanese catalogs)** — wrapped strings, runtime
  retranslation, RTL mirroring for Arabic, language persistence; manual 3/3.
- **STG-5.4 Close behaviour / tray** — Ask / Hide / Quit, `CloseDialog`, tray **Show** item;
  manual boxes 1–4 pass.
- **STG-5.6 Night mode** — settings card, preview, dim / undim around an alarm;
  `alarms.json` unchanged.
- **Issue #2 Sound discovery / import** — unified format list, robust paths, duplicate **Replace**,
  protected shared tones; manual 4/4.
- **A1–A5 cleanup** — formatting and clang-tidy findings cleared; 0 clang-format deviations
  across 46 files.
- **UI-C1–C5 constraints** — no new dependencies, Qt modules, Qt5Compat, third-party QML,
  or non-C++17/QML stack.
- **Japanese catalog** — `ja` / `ja_JP` load the real left-to-right catalog instead of falling
  back to English; `tst_i18n` has 8 test functions, and all three catalogs contain 247 messages.
- **Emoji density** — Off / Less / More with migration and static guards; More is the default;
  `ctest` 22/22.
- **Frameless move / resize** — static and gate verified (`ctest` 22/22, `tst_qmlresources` 12/12);
  **manual mouse pass still open**.
- **Release hygiene** — 80% build cap, local annotated `v1.0.5` tag, pure 156-file source export,
  GitHub deploy key generated.
- **Screenshots** — all six README gallery images replaced with v1.0.5 captures; the local
  username visible in the Sounds capture was replaced with a generic `/home/user/` path.

Still open, so not claimed as verified here: the manual mouse drag/resize pass, Japanese visual
confirmation, GitHub-side CI after the first push, and the remaining release gates in
`00-Missions.md`.

</details>

### Deviations from original spec (empirically required)

<details>
<summary><b>D1–D5 — why the implementation differs from the written spec</b></summary>

These are historical deviations recorded during development. The shipped v1.0.5 helper is
rtcwake-only; the systemd notes below apply to the earlier design that was tested and then
replaced.

- **D1:** Added data/org.lumalarm.Helper.conf. The default
  /etc/dbus-1/system.conf denies own/send, so D-Bus activation of the
  helper fails without a tailored policy file.
- **D2:** Polkit authorization is enforced inside the helper
  (via pkcheck) in addition to the D-Bus policy. Without it the
  delivered polkit action is decorative and the helper is callable
  by any local process.
- **D3:** systemd invocation uses --timer-property=WakeSystem=yes
  instead of the spec-literal --property=WakeSystem=yes, which
  fails on systemd 262 with "Unknown assignment".
- **D4:** The helper class declares Q_CLASSINFO("D-Bus Interface",
  "org.lumalarm.Helper"). Without it Qt exports the interface under
  local.<something> and D-Bus clients cannot find it.
- **D5:** Cancel/replace behavior fixed after testing: a bare-name
  `systemctl stop` hit the .service instead of the .timer, and
  re-scheduling orphaned the previous timer. Both now use the
  fully-qualified unit name and stop the previous timer first.

</details>

### Known limitations

<details>
<summary><b>B1 — RTC readback while awake</b></summary>

- **B1 (RTC readback):** the original systemd `WakeSystem=` design used
  CLOCK_BOOTTIME and wrote the RTC only at suspend entry, so
  /sys/class/rtc/rtc0/wakealarm read 0 while awake. That is historical: the
  shipped helper arms with detached `rtcwake`, and the RTC check
  (`rtcwake -m show`) is the primary hardware source of truth. Systemd timer
  units are queried only to rediscover or stop legacy units after a restart.

</details>

[Unreleased]: https://github.com/shinigami1231111/lumalarm/compare/v1.0.5...HEAD
[1.0.5]: https://github.com/shinigami1231111/lumalarm/compare/v1.0.0...v1.0.5
[1.0.0]: https://github.com/shinigami1231111/lumalarm/releases/tag/v1.0.0
