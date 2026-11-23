PREFIX     ?= /usr
LIBEXECDIR ?= $(PREFIX)/lib/nm-forcepoint
NMVPNDIR   ?= $(PREFIX)/lib/NetworkManager/VPN
DBUSDIR    ?= $(PREFIX)/share/dbus-1/system.d
DOCDIR     ?= $(PREFIX)/share/doc/networkmanager-forcepoint
LICENSEDIR ?= $(PREFIX)/share/licenses/networkmanager-forcepoint

.PHONY: all test install uninstall

all:
	@echo "Nothing to build. Run 'make test' or 'sudo make install'."

test:
	python3 -m unittest discover -s tests -v

install:
	install -Dm755 libexec/nm-forcepoint-service $(DESTDIR)$(LIBEXECDIR)/nm-forcepoint-service
	install -Dm755 libexec/nm-forcepoint-hook    $(DESTDIR)$(LIBEXECDIR)/nm-forcepoint-hook
	install -Dm755 libexec/resolvectl            $(DESTDIR)$(LIBEXECDIR)/resolvectl
	install -d $(DESTDIR)$(NMVPNDIR)
	sed 's|@LIBEXECDIR@|$(LIBEXECDIR)|' data/nm-forcepoint-service.name.in \
	    > $(DESTDIR)$(NMVPNDIR)/nm-forcepoint-service.name
	chmod 644 $(DESTDIR)$(NMVPNDIR)/nm-forcepoint-service.name
	install -Dm644 data/nm-forcepoint-service.conf $(DESTDIR)$(DBUSDIR)/nm-forcepoint-service.conf
	install -Dm644 README.md $(DESTDIR)$(DOCDIR)/README.md
	install -Dm644 LICENSE   $(DESTDIR)$(LICENSEDIR)/LICENSE

uninstall:
	rm -f  $(DESTDIR)$(NMVPNDIR)/nm-forcepoint-service.name
	rm -f  $(DESTDIR)$(DBUSDIR)/nm-forcepoint-service.conf
	rm -rf $(DESTDIR)$(LIBEXECDIR) $(DESTDIR)$(DOCDIR) $(DESTDIR)$(LICENSEDIR)
