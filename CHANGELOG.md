# Changelog

All notable changes to Lumalarm are documented here. Format: [Keep a Changelog](https://keepachangelog.com/).

> [!NOTE]
> **Latest release: [v1.0.5](#105---2026-11-22) — 2026-11-22.** Secure D-Bus wake helper,
> English / Arabic / Japanese UI, emoji density, frameless move & resize, and prebuilt AUR packages.

## [Unreleased]

## [1.0.5] - 2026-11-22

### Added

- Japanese catalog embedded and selectable (`ja` / `ja_JP`), with live retranslation.
- Three-level emoji density (Off / Less / More), migrated from the old boolean; More is the default.
- Frameless window: native system move plus eight transparent resize handles; no visible bar.
- Prebuilt Arch package: the AUR `lumalarm` package and the GitHub release asset install the
  compiled binary directly with `pacman -U`, with a pinned SHA-256 and no local build.
- First-run wizard (language, wake method, notifications, summary), tray icon, freedesktop
  notifications, test alarm, night mode and a 22-binary test suite.
- D-Bus wake helper, polkit action `org.lumalarm.schedule.wake` and systemd activation files.
- CI (`ci.yml`, `lint.yml`), a local mirror (`scripts/ci-local.sh`), optional pre-push hook,
  and `.clang-format` / `.clang-tidy`.

### Changed

- Builds are capped at `max(1, floor(nproc * 80 / 100))` jobs everywhere; no tracked command uses `-j$(nproc)`.
- Local and CI builds disable optional KDE blur for portability; native translucency still works.
- Source and QML trees reorganized into focused modules, with module boundaries enforced by tests.
- Ubuntu CI runs in a 26.04 container (24.04's Qt 6.4 is below the Qt 6.5 minimum); the advisory
  clang-tidy job does the same and normalizes GCC-only compile flags.

### Fixed

- `showEmojis` was a no-op; it now gates all decorative glyphs.
- Frameless windows could not be moved/resized with the mouse without native decorations.
- Toggling an alarm no longer loses its id after a model refresh.
- `tst_stg1security` no longer fails in a clean checkout when the local-only `PKGBUILD.local` is absent.
- `tst_schedulerlogic::occurrenceIsAbsoluteUtc` now uses an explicit UTC zone on both sides of
  its round-trip, so Arch, Fedora and Ubuntu all pass.
- Packaging depends only on `qt6-base`, `qt6-multimedia`, `qt6-declarative` and `util-linux`;
  the unavailable `qt6-quickcontrols2` dependency was dropped.

### Removed

- The old passwordless rtcwake rule and all instructions for creating it. `lumalarm.install`
  removes any legacy `/etc/sudoers.d/lumalarm` file left by earlier releases.

### Verified

- `ctest` 22/22 in clean Arch containers and on the host; all four GitHub checks green
  (Ubuntu 26.04, Fedora 40, Arch Linux, clang-tidy).
- `ci-local.sh` PASS with and without clang-tidy; package audits show no personal paths,
  no secrets and no privileged shell rule in the built artifact.
- 247 messages in each of the three catalogs; 119 gated emoji glyphs; four wizard steps;
  seven wake modes; frameless move/resize handlers present.

Manual mouse drag/resize and Japanese visual confirmation are still open.

### Known limitations

- KWin blur needs the Blur desktop effect enabled; without it the window is simply translucent.
- RTC readback while awake is historical — the shipped helper arms with detached `rtcwake` and
  uses the RTC as the hardware source of truth.

[Unreleased]: https://github.com/shinigami1231111/lumalarm/compare/v1.0.5...HEAD
[1.0.5]: https://github.com/shinigami1231111/lumalarm/compare/v1.0.0...v1.0.5
[1.0.0]: https://github.com/shinigami1231111/lumalarm/releases/tag/v1.0.0
