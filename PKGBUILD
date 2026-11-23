# Local package: run `makepkg -si` in this directory.
pkgname=networkmanager-forcepoint
pkgver=0.1.0
pkgrel=1
pkgdesc="NetworkManager VPN plugin for Forcepoint NGFW SSL VPN (drives forcepoint-client)"
arch=('any')
url="https://github.com/Akiro-aki/NetworkManager-forcepoint"
license=('MIT')
depends=('networkmanager' 'libnm' 'python' 'python-gobject' 'iproute2')
optdepends=('forcepoint-client-bin: the Forcepoint SSL VPN client itself (AUR)')
checkdepends=()

check() {
  make -C "$startdir" test
}

package() {
  make -C "$startdir" DESTDIR="$pkgdir" PREFIX=/usr install
}
