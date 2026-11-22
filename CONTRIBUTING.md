# Contributing to Lumalarm

Lumalarm is a Qt 6 / C++17 alarm clock with a QML front end and a privileged D-Bus wake helper.
Bug reports, docs, translations, tests and code are all welcome. These are the conventions that
keep review cheap.

## Prerequisites

- Qt >= 6.5 (`Core`, `Gui`, `Qml`, `Quick`, `QuickControls2`, `Multimedia`, `Widgets`, `Test`, `DBus`)
- CMake >= 3.21, a C++17 compiler (GCC or Clang)
- Optional: KF6WindowSystem (KWin blur), Qt Linguist tools (`lupdate` / `lrelease`)
- CI covers Ubuntu 26.04, Fedora 40 and Arch Linux. Ubuntu 24.04 ships Qt 6.4 and is unsupported.

```sh
sudo pacman -S --needed base-devel cmake ninja pkgconf qt6-base qt6-declarative qt6-multimedia qt6-tools
```

## Build, run, test

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
JOBS=$(( $(nproc) * 80 / 100 )); [ "$JOBS" -lt 1 ] && JOBS=1
cmake --build build -j"$JOBS"
ctest --test-dir build --output-on-failure
./build/lumalarm
```

KDE blur is auto-detected; force it off with `-DLUMALARM_ENABLE_KDE_BLUR=OFF`.
Run the app inside a desktop session (Wayland or X11), not a bare TTY.
Every tracked build command caps jobs at `max(1, floor(nproc * 80 / 100))`.

There are 22 test binaries covering the alarm/wake state machine, models, audio stack, config,
theme, blur, QML resource layout, notifier, tray, night mode, wizard, i18n and helper security.
Keep them green and add a test with new behavior.

`bash scripts/ci-local.sh` mirrors CI locally: Release build with blur OFF, full `ctest`, then a
**mandatory** clang-tidy pass (`--no-lint` skips it; GitHub's `lint.yml` is advisory).
`make ci`, `make ci-fast` and `make clean-ci` are shortcuts. An optional pre-push hook is
installed with `bash scripts/install-hooks.sh`.

## Translations

English is the source language. User-visible strings use `qsTr()` in QML and `tr()` in C++;
logs and product names stay untranslated. Catalogs live in `translations/`:

| File | Contents |
|---|---|
| `lumalarm_en.ts` | English source template |
| `lumalarm_ar.ts` | Arabic (RTL) |
| `lumalarm_ja.ts` | Japanese |

```sh
cmake --build build --target update_translations   # refresh .ts files (opt-in)
cmake --build build                                # compile and embed the .qm files
```

`lumalarm_ar.ts` and `lumalarm_ja.ts` must have 0 unfinished entries — `tst_i18n` enforces it.
Keep the trailing space in `"h "` and the leading space in `" min"`. `Qt6::LinguistTools` is
optional; without it the build succeeds in English and catalog tests skip.

## Layout and style

- C++ includes are rooted at `src/` (`#include "wake/Scheduler.h"`, never `../wake/Scheduler.h`)
- QML types resolve through `import GlassAlarm`; resource paths mirror the source tree and are guarded by `tst_qmlresources`
- Directories: `src/{app,models,managers,audio,wake,ui}`, `qml/{components,pages,pickers,overlays,wizard}`, `helper/`, `data/`, `tests/`, `translations/`, `resources/`
- C++: 4 spaces, Qt naming (`m_` members), one class per header/source pair where needed
- QML: PascalCase file names, camelCase properties, 4-space indent
- Run `.clang-format` / `.clang-tidy` on touched code only; do not reformat unrelated files

Commits use [Conventional Commits](https://www.conventionalcommits.org/) (`feat`, `fix`, `docs`,
`test`, `ci`, ...), one logical change per commit.

## Privileged components

`helper/` and `data/` contain the root-owned wake helper. Changes there get extra review:

- Never add `setuid` binaries, `system()` calls, or shell command strings built by concatenation
- Keep the interface XML, helper implementation and `src/wake/WakeManager.cpp` in sync
- Files: `data/org.lumalarm.Helper.{service,xml,conf}`, `data/org.lumalarm.schedule.policy`,
  `data/lumalarm-wake-helper.service`

## Release process

Releases come from `main` after CI is green and `CHANGELOG.md` has a dated section. Keep the
version in `CMakeLists.txt` and `PKGBUILD` in sync.

1. Check `git status --short`, `git log --oneline origin/main..HEAD`, `git tag -l`
2. Push with the SSH deploy remote: `git push deploy main` (`origin` is HTTPS)
3. Tag after CI passes: `git tag -a vX.Y.Z -m "..."` then `git push deploy vX.Y.Z`
4. Push the built `lumalarm-X.Y.Z-1-x86_64.pkg.tar.zst` and its `SHA256SUMS` to the
   `release-assets` branch
5. Point the AUR `PKGBUILD` at that versioned raw URL, pin its SHA-256 and refresh `.SRCINFO`

Do not tag, push or publish a release without being asked.

## Reporting bugs

Include your OS/Qt version, the Lumalarm version (`git describe --tags`), exact steps and logs.
For wake problems: `journalctl -u lumalarm-wake-helper`.

## License

GPL-3.0-or-later; see [LICENSE](LICENSE). Contributions are accepted under the same terms.
