PREFIX     ?= /usr
LIBEXECDIR ?= $(PREFIX)/lib/nm-forcepoint
NMVPNDIR   ?= $(PREFIX)/lib/NetworkManager/VPN
DBUSDIR    ?= $(PREFIX)/share/dbus-1/system.d
DOCDIR     ?= $(PREFIX)/share/doc/networkmanager-forcepoint
LICENSEDIR ?= $(PREFIX)/share/licenses/networkmanager-forcepoint
NMLIBDIR   ?= $(PREFIX)/lib/NetworkManager

CC         ?= cc
PKG_CONFIG ?= pkg-config
CFLAGS     ?= -O2 -g
EDITOR_SO   = build/libnm-vpn-plugin-forcepoint-editor.so
EDITOR_PKGS = libnm gtk+-3.0

.PHONY: all editor test clean install uninstall

all: editor

editor: $(EDITOR_SO)

$(EDITOR_SO): editor/nm-forcepoint-editor.c
	@mkdir -p build
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -Wall -Wextra -Wno-unused-parameter \
	    $(shell $(PKG_CONFIG) --cflags $(EDITOR_PKGS)) -shared -o $@ $< \
	    $(LDFLAGS) -Wl,--no-undefined $(shell $(PKG_CONFIG) --libs $(EDITOR_PKGS))

clean:
	rm -rf build

test:
	python3 -m unittest discover -s tests -v

install: all
	install -Dm755 libexec/nm-forcepoint-service $(DESTDIR)$(LIBEXECDIR)/nm-forcepoint-service
	install -Dm755 libexec/nm-forcepoint-hook    $(DESTDIR)$(LIBEXECDIR)/nm-forcepoint-hook
	install -Dm755 libexec/resolvectl            $(DESTDIR)$(LIBEXECDIR)/resolvectl
	install -d $(DESTDIR)$(NMVPNDIR)
	install -Dm755 $(EDITOR_SO) $(DESTDIR)$(NMLIBDIR)/libnm-vpn-plugin-forcepoint-editor.so
	sed -e 's|@LIBEXECDIR@|$(LIBEXECDIR)|' -e 's|@NMLIBDIR@|$(NMLIBDIR)|' \
	    data/nm-forcepoint-service.name.in \
	    > $(DESTDIR)$(NMVPNDIR)/nm-forcepoint-service.name
	chmod 644 $(DESTDIR)$(NMVPNDIR)/nm-forcepoint-service.name
	install -Dm644 data/nm-forcepoint-service.conf $(DESTDIR)$(DBUSDIR)/nm-forcepoint-service.conf
	install -Dm644 README.md $(DESTDIR)$(DOCDIR)/README.md
	install -Dm644 LICENSE   $(DESTDIR)$(LICENSEDIR)/LICENSE

uninstall: all
	rm -f  $(DESTDIR)$(NMVPNDIR)/nm-forcepoint-service.name
	rm -f  $(DESTDIR)$(NMLIBDIR)/libnm-vpn-plugin-forcepoint-editor.so
	rm -f  $(DESTDIR)$(DBUSDIR)/nm-forcepoint-service.conf
	rm -rf $(DESTDIR)$(LIBEXECDIR) $(DESTDIR)$(DOCDIR) $(DESTDIR)$(LICENSEDIR)
