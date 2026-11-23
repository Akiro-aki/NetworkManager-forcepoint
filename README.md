# NetworkManager-forcepoint

A NetworkManager VPN plugin for **Forcepoint NGFW SSL VPN** gateways on Linux.

Forcepoint's SSL VPN uses its own protocol, which OpenConnect, OpenVPN and
strongSwan do not speak, and Forcepoint ships only a command-line client for
Linux. This plugin lets NetworkManager drive that client, so the VPN becomes an
ordinary connection: `nmcli connection up`, desktop VPN switches and bar
widgets that list NetworkManager VPNs all work, and NetworkManager owns the
tunnel's routes and DNS.

> Unofficial. Not affiliated with or endorsed by Forcepoint. You still need
> Forcepoint's own `forcepoint-client`; this project does not include it.

## How it works

NetworkManager starts `nm-forcepoint-service` (as root) when the connection is
activated. The service:

1. runs `forcepoint-client` with your user name, passing the password through
   `--passcmd` from its environment, never on the command line;
2. passes a stand-in `resolvectl` as `--resolver`, so the DNS servers and
   search domains the gateway sends go to NetworkManager instead of being
   written to systemd-resolved behind NetworkManager's back;
3. waits for the client's `--establishedcmd` hook, reads the TUN device, its
   address and routes, and reports them to NetworkManager;
4. gives the tunnel's DNS servers and domains to systemd-resolved, since
   NetworkManager does not do that for a TUN device the client configured
   itself;
5. on disconnect, sends the client `SIGINT` so it tears the tunnel down itself.

A GTK 3 settings page (`libnm-vpn-plugin-forcepoint-editor.so`) lets
`nm-connection-editor` create and edit these connections.

## Status

Early, but working: tested on Arch Linux with NetworkManager 1.58,
forcepoint-client 2.5.2 and a split-tunnel gateway. Reports from other
distributions and gateway setups are welcome.

## Installation

### 1. Install Forcepoint's client

The plugin drives Forcepoint's own `forcepoint-client` 2.x; it does not
include it. Get it from your organisation's IT department or Forcepoint's
support site.

- **Arch Linux:** the AUR package `forcepoint-client-bin`, e.g.
  `yay -S forcepoint-client-bin`. It installs the client as
  `/usr/bin/_forcepoint-client`; the plugin finds it and its bundled libevent
  automatically.
- **Debian / Ubuntu:** `sudo apt install ./forcepoint-client_*.deb`, which
  installs `/usr/sbin/forcepoint-client`.
- Anywhere else, set `client=` (and `library-path=` if needed) in the
  connection's settings.

### 2. Install the plugin

Get the source:

```sh
git clone https://github.com/Akiro-aki/NetworkManager-forcepoint.git
cd NetworkManager-forcepoint
```

**Arch Linux** (builds, runs the tests and installs a pacman package):

```sh
makepkg -si
```

**Debian / Ubuntu:**

```sh
sudo apt install build-essential pkg-config libnm-dev libgtk-3-dev \
                 python3-gi gir1.2-nm-1.0 iproute2
make && make test
sudo make install
```

**Fedora:**

```sh
sudo dnf install gcc make pkgconf-pkg-config NetworkManager-libnm-devel \
                 gtk3-devel python3-gobject iproute
make && make test
sudo make install
```

`make install` takes the usual `PREFIX` (default `/usr`) and `DESTDIR`
variables. NetworkManager only looks for VPN plugins under its own prefix, so
keep `PREFIX=/usr` unless your NetworkManager lives elsewhere.

### 3. Tell D-Bus and NetworkManager about it

Needed once after the first install:

```sh
sudo systemctl reload dbus
sudo systemctl restart NetworkManager
```

Restarting NetworkManager briefly drops your network connections.

### Updating

Pull and repeat step 2 (`git pull && makepkg -si`, or
`git pull && make && sudo make install`). Reconnect the VPN to use the new
version; restart `nm-connection-editor` if it is open.

### Requirements

- NetworkManager with libnm 1.2 or newer, Python 3.9+, PyGObject, iproute2
- GTK 3, for the settings page in `nm-connection-editor`
- to build: a C compiler, make, pkg-config, and the libnm and GTK 3
  development files

## Set up a connection

**1. Trust the gateway once.** The client asks you to check the gateway's
certificate on the first connection, which it can't do under NetworkManager.
Connect once in a terminal, accept the certificate, then press Ctrl+C:

```sh
sudo forcepoint-client -p 4443 vpn.example.org -u alice
```

The client saves what it learned, including the gateway's CA, to
`~/.config/forcepoint/sslvpn/<tunnel name>` (in your home when run with sudo).

**2. Create the NetworkManager connection.** In `nm-connection-editor`, click
**+**, choose **Forcepoint SSL VPN**, pick that file under **Saved config**
and fill in your user name and password. Or with `nmcli`:

```sh
nmcli connection add type vpn vpn-type forcepoint con-name work-vpn \
  vpn.data "config=$HOME/.config/forcepoint/sslvpn/Example_-_Primary, user=alice"
nmcli connection modify work-vpn vpn.secrets "password=YOUR-PASSWORD"
```

The password is then stored by NetworkManager in
`/etc/NetworkManager/system-connections/work-vpn.nmconnection`, readable only by
root. To keep it out of your shell history, set it in `nm-connection-editor`
instead, or with `nmcli connection edit work-vpn`.

**3. Connect:**

```sh
nmcli connection up work-vpn
nmcli connection down work-vpn
```

### Options (`vpn.data`)

| Key | Meaning |
|---|---|
| `config` | Client XML saved by a previous connection. Pins the gateway's CA. Recommended. |
| `gateway` | Host name or IP of the gateway, used when there is no `config`. |
| `port` | Gateway port. Default 443, or the port recorded in `config`. |
| `user` | User name. Required. |
| `dns` | `no` to ignore DNS servers sent by the gateway. Default `yes`. |
| `client` | Path to `forcepoint-client`, if it isn't in a standard place. |
| `library-path` | Extra `LD_LIBRARY_PATH` for the client. |

Secret (`vpn.secrets`): `password`.

## Limitations

- **The password must be stored in the connection** (`password-flags=0`,
  "Store the password for all users"). libnm's Python bindings can't tell
  NetworkManager which secrets are missing, so the plugin can't prompt.
- **The settings page is GTK 3**, for `nm-connection-editor`. GNOME Settings
  (GTK 4) can connect and disconnect the VPN but not edit it.
- **No two-factor challenge** (`--challengecmd`) support.
- **IPv4 only.** IPv6 addresses sent by the gateway are ignored.

## Troubleshooting

```sh
journalctl -u NetworkManager -b | grep nm-forcepoint
```

The client's own output is logged with a `client:` prefix. For more detail:
`sudo nmcli general logging level DEBUG domains VPN_PLUGIN`.

## Uninstall

`sudo pacman -R networkmanager-forcepoint` on Arch, or `sudo make uninstall`
from the source folder elsewhere. Delete the connection first with
`nmcli connection delete <name>`.

## License

MIT. See [LICENSE](LICENSE).
