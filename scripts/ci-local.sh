#!/usr/bin/env bash
#
# ci-local.sh — run the GitHub Actions CI checks locally, on the host.
#
# Purpose:
#   Reproduce what .github/workflows/ci.yml does — a Release build with KDE
#   blur disabled followed by the full ctest suite — before you ever push.
#   No Docker, no containers, no network access, no package installation.
#
# Usage:
#   bash scripts/ci-local.sh [--no-lint] [--keep-build]
#   make ci          # identical to: bash scripts/ci-local.sh
#   make ci-fast     # identical to: bash scripts/ci-local.sh --no-lint
#   make clean-ci    # rm -rf build-local-ci
#
# Relationship to .github/workflows/ci.yml:
#   Same configure flags (CMAKE_BUILD_TYPE=Release,
#   LUMALARM_ENABLE_KDE_BLUR=OFF) and the same build + ctest invocation. The
#   workflow additionally builds inside Fedora 40 and Arch containers on an
#   Ubuntu 24.04 runner; this script covers the host only. The lint step
#   mirrors .github/workflows/lint.yml, which is advisory (continue-on-error
#   there) — so lint findings are reported but do not fail the run, and lint
#   is skipped entirely while no .clang-tidy config exists in the repo.
#
# Exit status: 0 when configure, build and ctest are all green; non-zero on
# the first hard failure. Missing prerequisites exit 1 with the exact install
# command for the detected distribution.

set -euo pipefail

REPO_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

BUILD_DIR="build-local-ci"
RUN_LINT=1
KEEP_BUILD=0
DISTRO_ID="unknown"
QT_VERSION="not found"

usage() {
    cat <<EOF
Usage: bash scripts/ci-local.sh [OPTIONS]

Runs the same checks as .github/workflows/ci.yml on this host:
Release build (KDE blur OFF) -> ctest -> optional clang-tidy lint.

Options:
  --no-lint     Skip the clang-tidy step (lint.yml is advisory).
  --keep-build  Reuse $BUILD_DIR instead of deleting it (incremental rebuild).
  --help        Show this help and exit.
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --no-lint)    RUN_LINT=0 ;;
        --keep-build) KEEP_BUILD=1 ;;
        --help|-h)    usage; exit 0 ;;
        *)            echo "ci-local: unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

# ── host detection ────────────────────────────────────────────────────────────

detect_distro() {
    DISTRO_ID="unknown"
    DISTRO_VERSION=""
    if [ -r /etc/os-release ]; then
        # shellcheck disable=SC1091
        . /etc/os-release
        DISTRO_ID="${ID:-unknown}"
        DISTRO_VERSION="${VERSION_ID:-}"
    fi
}

distro_name() {
    case "$DISTRO_ID" in
        arch)                     printf 'Arch Linux%s\n'    "${DISTRO_VERSION:+ $DISTRO_VERSION}" ;;
        fedora)                   printf 'Fedora %s\n'      "$DISTRO_VERSION" ;;
        ubuntu)                   printf 'Ubuntu %s\n'      "$DISTRO_VERSION" ;;
        debian)                   printf 'Debian %s\n'      "$DISTRO_VERSION" ;;
        *)                        printf '%s%s\n'           "$DISTRO_ID" "${DISTRO_VERSION:+ $DISTRO_VERSION}" ;;
    esac
}

# Exact install command for the missing prerequisites — printed, never run.
install_hint() {
    case "$DISTRO_ID" in
        arch)
            echo "sudo pacman -S --needed base-devel cmake ninja pkgconf"
            echo "sudo pacman -S --needed qt6-base qt6-declarative qt6-multimedia qt6-tools"
            ;;
        fedora)
            echo "sudo dnf install -y cmake ninja-build gcc-c++ pkgconf-pkg-config"
            echo "sudo dnf install -y qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtmultimedia-devel qt6-qttools-devel"
            ;;
        ubuntu)
            echo "sudo apt-get install -y cmake ninja-build g++ pkg-config"
            echo "sudo apt-get install -y qt6-base-dev qt6-base-dev-tools qt6-declarative-dev qt6-multimedia-dev qt6-tools-dev"
            ;;
        *)
            echo "Install: cmake (>= 3.21), ninja, a C++17 compiler (g++ or clang++),"
            echo "pkg-config, and Qt 6.5+ with Core/Gui/Qml/Quick/QuickControls2/"
            echo "Multimedia/Widgets/Test/DBus — then re-run this script."
            ;;
    esac
}

detect_qt_version() {
    if command -v qmake6 >/dev/null 2>&1; then
        QT_VERSION="$(qmake6 -query QT_VERSION 2>/dev/null || echo "unknown")"
    elif command -v qtpaths6 >/dev/null 2>&1; then
        QT_VERSION="$(qtpaths6 --qt-version 2>/dev/null || echo "unknown")"
    else
        QT_VERSION="not found"
    fi
}

print_banner() {
    echo "=============================================================="
    echo " Lumalarm local CI (mirrors .github/workflows/ci.yml)"
    echo "=============================================================="
    echo " distro      : $(distro_name)"
    echo " Qt          : ${QT_VERSION}"
    echo " CMake       : $(cmake --version | head -n 1)"
    echo " compiler    : $(detect_compiler)"
    echo " build dir   : $BUILD_DIR"
    echo " lint        : $([ "$RUN_LINT" -eq 1 ] && echo enabled || echo disabled)"
    echo "=============================================================="
}

detect_compiler() {
    if command -v g++ >/dev/null 2>&1; then
        g++ --version | head -n 1
    elif command -v clang++ >/dev/null 2>&1; then
        clang++ --version | head -n 1
    else
        echo "none found"
    fi
}

print_summary() {
    local result="$1"
    echo "=============================================================="
    echo " RESULT: ${result}"
    echo " distro   : $(distro_name)"
    echo " Qt       : ${QT_VERSION}"
    echo " elapsed  : ${SECONDS}s"
    echo "=============================================================="
}

on_error() {
    local code="$1" line="$2"
    echo ""
    echo "[ci-local] FAILED at line ${line} (exit ${code})"
    print_summary "FAIL"
    exit "$code"
}
trap 'on_error "$?" "$LINENO"' ERR

# ── preflight: tools ──────────────────────────────────────────────────────────

check_prerequisites() {
    local missing=()
    command -v cmake     >/dev/null 2>&1 || missing+=("cmake")
    command -v ninja     >/dev/null 2>&1 || missing+=("ninja")
    command -v ctest     >/dev/null 2>&1 || missing+=("ctest")
    if ! command -v g++ >/dev/null 2>&1 && ! command -v clang++ >/dev/null 2>&1; then
        missing+=("g++ or clang++")
    fi
    command -v pkg-config >/dev/null 2>&1 || missing+=("pkg-config")

    if [ "${#missing[@]}" -gt 0 ]; then
        echo "[ci-local] missing prerequisites: ${missing[*]}" >&2
        echo "[ci-local] install them with:" >&2
        install_hint >&2
        exit 1
    fi
}

# ── steps ─────────────────────────────────────────────────────────────────────

configure() {
    echo "[ci-local] configuring (Release, KDE blur OFF — matches the CI matrix)"
    cmake -B "$BUILD_DIR" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DLUMALARM_ENABLE_KDE_BLUR=OFF
}

build() {
    echo "[ci-local] building with -j$(nproc)"
    cmake --build "$BUILD_DIR" -j"$(nproc)"
}

run_tests() {
    echo "[ci-local] running ctest"
    ctest --test-dir "$BUILD_DIR" --output-on-failure
}

run_lint() {
    if [ "$RUN_LINT" -eq 0 ]; then
        echo "[ci-local] lint skipped (--no-lint)"
        return 0
    fi
    if [ ! -f .clang-tidy ]; then
        echo "[ci-local] no .clang-tidy yet — skipping lint"
        return 0
    fi
    if ! command -v clang-tidy >/dev/null 2>&1; then
        echo "[ci-local] .clang-tidy present but clang-tidy is not installed — skipping lint"
        return 0
    fi

    echo "[ci-local] reconfiguring with CMAKE_EXPORT_COMPILE_COMMANDS=ON"
    cmake -B "$BUILD_DIR" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DLUMALARM_ENABLE_KDE_BLUR=OFF \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON >/dev/null

    echo "[ci-local] running clang-tidy over src/ (advisory, like lint.yml)"
    local findings=0
    while IFS= read -r -d '' file; do
        if ! clang-tidy -p "$BUILD_DIR" --quiet "$file"; then
            findings=$((findings + 1))
        fi
    done < <(find src -name '*.cpp' -print0)

    if [ "$findings" -gt 0 ]; then
        echo "[ci-local] clang-tidy reported findings in ${findings} file(s) — advisory only"
    else
        echo "[ci-local] clang-tidy clean"
    fi
}

# ── main ──────────────────────────────────────────────────────────────────────

detect_distro
detect_qt_version
check_prerequisites
print_banner

if [ "$KEEP_BUILD" -eq 1 ] && [ -d "$BUILD_DIR" ]; then
    echo "[ci-local] reusing $BUILD_DIR (--keep-build): incremental run"
else
    echo "[ci-local] removing $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

configure
build
run_tests
run_lint

print_summary "PASS"
exit 0
