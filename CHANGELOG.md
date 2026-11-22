# Changelog

All notable user-facing changes are documented here. Entries follow
[Keep a Changelog](https://keepachangelog.com/en/).

> [!NOTE]
> **Current version: [v1.0.5](https://github.com/shinigami1231111/lumalarm/tree/v1.0.5) — 2026-11-22.**
> The Arch package is available from the [AUR](https://aur.archlinux.org/packages/lumalarm) and the
> [GitHub package assets](https://github.com/shinigami1231111/lumalarm/tree/v1.0.5/assets).

## [Unreleased]

### Changed

- The RTCwake selector is now a sleep-method chooser with short visible names (`mem`, `freeze`,
  `standby`, `off`, `disk`). **mem** is first, then **freeze**, then a visual experimental wall;
  "Recommended", "Experimental" and every full explanation appear only in hover tooltips, whose text,
  background and accent follow the application theme. `no` and `on` remain valid only in the
  privileged helper allow-list.
- Settings presents alarm playback and the diagnostic wake together as **Alarm Sound & Wake Test**.
  The unused Font Family editor and the redundant Cancel test alarm button were removed.
- The countdown Timer no longer offers Auto Stop; it remains an alarm-editor setting.
- The Arch package is rebuilt as `1.0.5-3` from the current source with optional KDE blur disabled.

### Fixed

- Desktop notifications no longer block the alarm-trigger signal chain, so the ringing overlay appears
  even when the notification daemon is slow or unreachable.
- The wizard's alarm-sound page imports `QtQuick.Controls`, so a rebuilt app no longer exits during QML
  startup because `Slider` cannot be resolved.
- The saved RTCwake method now reaches `WakeManager` at startup and on change; emoji density changes
  apply live without a restart; the Classic time picker honours 12/24-hour format; the Timer starts
  with stable field keys and plays its completion sound on the zero tick; first-run sound preview uses
  a temporary fallback tone and the alarm-volume policy.

## [1.0.5] - 2026-11-22

### Added

- Japanese catalog embedded and selectable (`ja` / `ja_JP`), with live retranslation.
- Three-level emoji density (Off / Less / More), migrated from the old boolean; More is the default.
- Frameless window with native system move and eight transparent resize handles.
- Prebuilt Arch package with a pinned SHA-256 checksum, available through the AUR and as a versioned
  package asset; local compilation is not needed to install the published package.
- First-run wizard (language, wake method, notifications and summary), system tray icon, desktop
  notifications, test alarm and night mode.
- Privileged D-Bus wake helper, polkit action `org.lumalarm.schedule.wake` and systemd activation
  files; wake scheduling no longer depends on a passwordless sudo rule.
- GitHub CI workflows, a local CI mirror, an optional pre-push hook and `.clang-format` / `.clang-tidy`
  configuration.

### Changed

- Tracked build commands cap parallel jobs at `max(1, floor(nproc * 80 / 100))`.
- Local CI and GitHub build jobs disable optional KDE blur for portability; the app remains translucent.
- C++ and QML sources are organized into focused modules with module boundaries covered by tests.
- Ubuntu CI uses a Qt 6.5+ environment; the lint workflow normalizes compiler flags for clang-tidy.

### Fixed

- The emoji setting now controls decorative glyphs throughout the interface.
- Frameless windows can be moved and resized with the pointer.
- Toggling an alarm no longer loses its id after a model refresh.
- Security tests pass in a clean checkout when the local-only `PKGBUILD.local` is absent.
- The scheduler's absolute-UTC round-trip test no longer depends on the host timezone.
- Package runtime dependencies no longer include the unavailable `qt6-quickcontrols2` package.

### Removed

- The legacy passwordless `rtcwake` sudoers rule and instructions for creating it. The package upgrade
  hook removes the obsolete Lumalarm rule left by earlier versions.

<details>
<summary>Verification record</summary>

- `ctest`: 22/22 passed in clean Arch containers and on the development host.
- GitHub checks: Ubuntu 26.04, Fedora 40, Arch Linux and clang-tidy completed successfully for the
  release branch state.
- The local CI mirror passed with and without clang-tidy; package audits found no personal paths,
  secrets or privileged shell rule in the built artifact.
- Release checks now record 278 messages in each catalog, 132 gated emoji glyphs, five wizard steps,
  five offered wake modes (seven remain accepted by the helper) and frameless move/resize handlers.

</details>

### Known limitations

- RTC wake support and resume behavior depend on a machine's firmware, RTC device and kernel sleep
  modes. Test a short wake on the target hardware before relying on it.
- Manual visual confirmation of pointer move/resize and the Japanese layout remains open.
- KWin blur requires the Plasma Blur effect; without it, the window remains translucent.
- RTC readback while the machine is awake is historical. The helper arms `rtcwake` and relies on the
  RTC as the hardware source of truth.

[Unreleased]: https://github.com/shinigami1231111/lumalarm/compare/v1.0.5...HEAD
[1.0.5]: https://github.com/shinigami1231111/lumalarm/compare/v1.0.0...v1.0.5
[1.0.0]: https://github.com/shinigami1231111/lumalarm/releases/tag/v1.0.0
