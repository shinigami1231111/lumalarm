# Maintainer: Glass Alarm Developer
# Contributor: Arch Linux User
#
# STG-1 packaging contract:
#   * builds from THIS source tree — no release tag yet contains the D-Bus
#     wake helper or the polkit policy (v1.0.0/v1.0.2/v1.0.3 all predate
#     them), so a tarball recipe would ship a non-STG-1 application. The
#     repository's current packaging strategy is therefore a local build,
#     which is also what PKGBUILD.local does.
#   * no sudoers rule is ever installed or requested: wake scheduling runs
#     through /usr/lib/lumalarm/lumalarm-wake-helper + the polkit action
#     org.lumalarm.schedule.wake (STG-1.6).
#   * install hooks live in lumalarm.install — an inline hook function inside a
#     PKGBUILD is ignored by makepkg, so the packaging contract must not
#     depend on one.

pkgname=lumalarm
pkgver=1.0.5
pkgrel=1
pkgdesc="A professional glassmorphism alarm clock for Linux with rtcwake support"
arch=('x86_64' 'aarch64')
url="https://github.com/shinigami1231111/lumalarm"
license=('GPL3')
depends=(
    'qt6-base'
    'qt6-multimedia'
    'qt6-declarative'
    'qt6-quickcontrols2'
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
        -DCMAKE_CXX_FLAGS="-O2 -pipe"
    cmake --build build -j$(nproc)
}

package() {
    cd "${startdir}"
    DESTDIR="${pkgdir}" cmake --install build

    install -Dm644 LICENSE "${pkgdir}/usr/share/licenses/${pkgname}/LICENSE"

    mkdir -p "${pkgdir}/usr/share/${pkgname}/tones"
    install -Dm644 resources/lumalarm.desktop \
        "${pkgdir}/usr/share/applications/${pkgname}.desktop"
}
