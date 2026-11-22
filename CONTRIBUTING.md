# Contributing to Lumalarm

Bug reports, documentation, translations, tests and code contributions are welcome. Lumalarm is a
Qt 6 / C++17 desktop app with a QML interface and a privileged D-Bus wake helper. This guide covers
local development and the checks expected in a pull request.

## Development requirements

- CMake 3.21 or newer and a C++17 compiler (GCC or Clang)
- Qt 6.5 or newer: Core, Gui, Qml, Quick, QuickControls2, Multimedia, Widgets, DBus and Test
- Ninja and `pkgconf` are recommended
- Optional: KF6WindowSystem for native KWin blur; Qt Linguist tools for updating translations
- `util-linux` provides `rtcwake`, used by the wake helper

On Arch Linux, install the main build dependencies with:

```sh
sudo pacman -S --needed base-devel cmake ninja pkgconf qt6-base qt6-declarative qt6-multimedia qt6-tools
```

The current CI matrix and workflow details are maintained in [`.github/workflows/`](.github/workflows/).

## Build and run

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DLUMALARM_ENABLE_KDE_BLUR=OFF
JOBS=$(( $(nproc) * 80 / 100 )); [ "$JOBS" -lt 1 ] && JOBS=1
cmake --build build -j"$JOBS"
./build/lumalarm
```

KDE blur is optional and is detected automatically when available; the command above disables it to
match the portable CI build. Remove `-DLUMALARM_ENABLE_KDE_BLUR=OFF` to test native KWin blur. Run the
app inside a graphical Wayland or X11 session.

## Tests and local CI

Run the full test suite after code changes:

```sh
ctest --test-dir build --output-on-failure
```

Tests cover scheduling logic, models, configuration, theme, audio, QML resources, notifications,
tray behavior, night mode, the first-run wizard, translations and helper security. Keep existing
checks green and add focused coverage for new behavior.

The local CI mirror builds in Release mode with KDE blur disabled, runs `ctest`, then runs clang-tidy:

```sh
bash scripts/ci-local.sh
```

The local clang-tidy pass is a required gate unless `--no-lint` is supplied. GitHub's separate lint
workflow is advisory. Shortcuts are `make ci`, `make ci-fast` and `make clean-ci`. An optional
pre-push hook can be installed with `bash scripts/install-hooks.sh`.

Tests must not suspend or hibernate a real machine. Use fixtures or test doubles for unit coverage;
any live helper check must use a non-suspending mode, document its prerequisites and leave no wake
armed. Describe hardware-dependent checks separately in the pull request.

## Translations

English is the source language. Wrap user-visible QML strings in `qsTr()` and C++ strings in `tr()`;
keep product names and diagnostic logs untranslated. Catalogs are in `translations/`:

| File | Purpose |
|---|---|
| `lumalarm_en.ts` | English source catalog |
| `lumalarm_ja.ts` | Japanese translation |

Refresh catalogs when source strings change, then build and run the i18n tests:

```sh
cmake --build build --target update_translations
cmake --build build
ctest --test-dir build --output-on-failure
```

The Japanese catalog should have no unfinished entries. Preserve meaningful leading or
trailing spaces in translated strings, such as the time-unit labels.

## Project layout and style

- C++: `src/{app,models,managers,audio,wake,ui}`
- QML: `qml/{components,pages,pickers,overlays,wizard}`
- Privileged helper and service policy: `helper/` and `data/`
- Tests: `tests/`; translation catalogs: `translations/`
- C++ uses four spaces and Qt naming conventions; QML uses PascalCase filenames, camelCase properties
  and four-space indentation
- Include C++ headers from the `src/` root, for example `#include "wake/Scheduler.h"`
- Keep CMake QML resource declarations and resource tests in sync when adding or renaming QML files
- Format only touched code with `.clang-format` and `.clang-tidy`; avoid unrelated reformatting

Use a short-lived branch and keep each change focused. Pull requests should explain the user-visible
behavior, mention relevant tradeoffs, list the checks run and call out hardware-dependent checks that
were not run. Conventional Commit messages are encouraged (`feat`, `fix`, `docs`, `test`, `ci`, etc.).

## Privileged components

Changes to `helper/` or `data/` need careful security review:

- Keep the desktop app → system D-Bus → helper → polkit authorization boundary intact
- Do not add setuid binaries, `system()` calls, shell-built privileged commands or passwordless sudo rules
- Keep the D-Bus XML, helper implementation, polkit action and `src/wake/WakeManager.cpp` contract in sync
- Preserve the helper's delay bounds and allow-listed wake modes
- Review service sandboxing and capability settings along with any helper change

## Release maintenance

Release publication is maintainer-only. Before a release, ensure CI is green, the version in
`CMakeLists.txt` and package metadata agrees with the dated `CHANGELOG.md` section, and the tag points
to the reviewed commit. When publishing a binary package, update its checksum manifest and the AUR
metadata together. Do not include local machine paths, credentials or personal configuration in a
release or pull request.

## Reporting bugs

Include your distribution and version, Qt version, Lumalarm version (`git describe --tags`), exact
steps to reproduce and relevant logs. For wake problems, include the selected wake mode and output
from `journalctl -b -u lumalarm-wake-helper.service`. Redact usernames, paths and other personal data
before sharing logs publicly.

## License

Contributions are made under the project's **GNU GPL-3.0-or-later** license; see [LICENSE](LICENSE).
