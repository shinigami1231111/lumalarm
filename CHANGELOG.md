# Changelog

All notable changes to Lumalarm are documented here.
Format based on Keep a Changelog (https://keepachangelog.com/).

## [Unreleased]

### Added

- Source tree reorganized into `src/models`, `src/managers`, `src/audio`,
  `src/wake` and `src/ui`, and QML into `qml/components`, `qml/pages`,
  `qml/pickers`, `qml/overlays` and `qml/wizard` (STG-3). Module
  boundaries are now enforced by tests instead of convention.
- A 14-binary test suite: alarm/wake state machine, alarm model and
  selection, audio player, config, theme, blur, QML resource layout,
  test-alarm surface, notifier, tray icon, night mode, wizard and i18n.
- `CONTRIBUTING.md` — prerequisites, CI matrix, project layout, code
  style, commit conventions and the privileged-components rules — plus
  CI badges in the README.
- Continuous integration: `.github/workflows/ci.yml` (Release build and
  `ctest` on Ubuntu 24.04, Fedora 40 and Arch) and
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
  an Open/Quit menu and a Settings toggle.
- Automatic pre-alarm screen dimming (STG-5.6): `NightMode` plus
  `qml/NightDim.qml` hold a translucent overlay for the configured lead
  time before the next alarm, until it fires or is cancelled, disabled
  or deleted. Per-window only; no backlight or compositor-specific API.
- First-run setup wizard (STG-5.5): six skippable steps (welcome, language,
  default sound, wake method with a non-destructive test alarm, notification
  probe, summary) shown once per installation and re-runnable from Settings.
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

### Changed

- `.clang-format` now matches the style actually used in the tree, and
  the two noisiest clang-tidy checks were dropped; 28 `bugprone-*`
  findings were resolved in `src/` (follow-up A1–A4). Lint stays
  advisory in both CI and `ci-local.sh`.

## [1.0.5] - 2026-10-02

### Added
- D-Bus wake helper (STG-1.1): removes the /etc/sudoers.d/lumalarm
  requirement entirely. Scheduling now goes through a privileged
  helper with strict input validation (60 <= seconds <= 86400).
- Polkit action org.lumalarm.schedule.wake for authenticated scheduling.
- systemd activation files for the helper (D-Bus system service).

### Changed
- WakeManager now uses QDBusInterface to reach the helper, falling
  back to direct rtcwake if the helper is unavailable.
- Wake verification switched from RTC readback to `systemctl list-timers`
  while the system is awake (RTC is only written at suspend entry; see
  "Known limitations").
- PKGBUILD hooks moved to lumalarm.install (inline makepkg hooks are no-ops).

### Removed
- README instructions for editing /etc/sudoers.d/lumalarm.

### Fixed
- WheelTimePicker quirks repaired, and a secondary spin-style time
  picker added as an alternative to the wheels (`timePickerMode`).
- Toggling an alarm no longer loses its id: `setAlarmEnabled` is
  id-based instead of index-based, so a model refresh between click and
  write could not disable the wrong row.

### Deviations from original spec (empirically required)

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

### Known limitations

- **B1 (RTC readback):** systemd's WakeSystem= uses CLOCK_BOOTTIME
  and writes the RTC only at suspend entry. Reading
  /sys/class/rtc/rtc0/wakealarm while the system is awake therefore
  always returns 0. Verification now uses `systemctl list-timers`;
  the RTC check remains in place only for the rtcwake fallback path.
