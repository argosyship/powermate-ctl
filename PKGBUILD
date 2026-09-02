# Maintainer: Argosy
pkgname=powermate-scroll
pkgver=2.0.0
pkgrel=1
pkgdesc="High-resolution browser scrolling with a Griffin PowerMate"
arch=('x86_64')
license=('MIT')
depends=('libevdev')
makedepends=('gcc' 'make' 'pkgconf')
source=()
sha256sums=()

prepare() {
  mkdir -p "$srcdir/$pkgname"
  cp -a "$startdir"/{Makefile,src,packaging,config.example.toml,README.md,LICENSE} \
    "$srcdir/$pkgname/"
}

build() {
  cd "$srcdir/$pkgname"
  make
}

package() {
  cd "$srcdir/$pkgname"
  make DESTDIR="$pkgdir" PREFIX=/usr install
  install -Dm644 packaging/99-powermate-scroll.rules \
    "$pkgdir/usr/lib/udev/rules.d/99-powermate-scroll.rules"
  install -Dm644 packaging/uinput.conf \
    "$pkgdir/usr/lib/modules-load.d/powermate-scroll.conf"
  sed 's|/usr/local/bin/powermate-scroll|/usr/bin/powermate-scroll|' \
    packaging/powermate-scroll.service \
    > "$srcdir/powermate-scroll.service"
  install -Dm644 "$srcdir/powermate-scroll.service" \
    "$pkgdir/usr/lib/systemd/user/powermate-scroll.service"
  install -Dm644 config.example.toml \
    "$pkgdir/usr/share/doc/$pkgname/config.example.toml"
  install -Dm644 LICENSE "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
