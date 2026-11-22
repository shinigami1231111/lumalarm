# Maintainer: Glass Alarm Developer
# Contributor: Arch Linux User
#
# STG-1 packaging contract:
#   * builds from THIS source tree so the package is made from the exact
#     checked-out code (old v1.0.0-v1.0.3 tags predate the D-Bus helper and
#     polkit policy). PKGBUILD.local follows the same local-build strategy.
#   * no sudoers rule is ever installed or requested: wake scheduling runs
#     through /usr/lib/lumalarm/lumalarm-wake-helper + the polkit action
#     org.lumalarm.schedule.wake (STG-1.6).
#   * install hooks live in lumalarm.install — an inline hook function inside a
#     PKGBUILD is ignored by makepkg, so the packaging contract must not
#     depend on one.

pkgname=lumalarm
pkgver=1.0.5
pkgrel=2
pkgdesc="A professional glassmorphism alarm clock for Linux with rtcwake support"
arch=('x86_64' 'aarch64')
url="https://github.com/shinigami1231111/lumalarm"
license=('GPL3')
depends=(
    'qt6-base'
    'qt6-multimedia'
    'qt6-declarative'
    'util-linux'
)
makedepends=(
    'cmake'
    'gcc'
    'qt6-tools'
)
source=()
install=lumalarm.install

build() {
    cd "${startdir}"
    cmake -B build \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_FLAGS="-O2 -pipe" \
        -DLUMALARM_ENABLE_KDE_BLUR=OFF
    local jobs=$(( $(nproc) * 80 / 100 ))
    [ "$jobs" -lt 1 ] && jobs=1
    cmake --build build -j"$jobs"
}

package() {
    cd "${startdir}"
    DESTDIR="${pkgdir}" cmake --install build

    install -Dm644 LICENSE "${pkgdir}/usr/share/licenses/${pkgname}/LICENSE"

    mkdir -p "${pkgdir}/usr/share/${pkgname}/tones"
    install -Dm644 resources/lumalarm.desktop \
        "${pkgdir}/usr/share/applications/${pkgname}.desktop"
}
