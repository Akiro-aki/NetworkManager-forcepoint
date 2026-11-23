# Local package: run `makepkg -si` in this directory.
pkgname=networkmanager-forcepoint
pkgver=0.2.0
pkgrel=1
pkgdesc="NetworkManager VPN plugin for Forcepoint NGFW SSL VPN (drives forcepoint-client)"
arch=('x86_64')
url="https://github.com/Akiro-aki/NetworkManager-forcepoint"
license=('MIT')
depends=('networkmanager' 'libnm' 'python' 'python-gobject' 'iproute2' 'gtk3')
makedepends=('pkgconf')
optdepends=('forcepoint-client-bin: the Forcepoint SSL VPN client itself (AUR)'
            'nm-connection-editor: edit the connection in a window')

build() {
  make -C "$startdir" clean all
}

check() {
  make -C "$startdir" test
}

package() {
  make -C "$startdir" DESTDIR="$pkgdir" PREFIX=/usr install
}
