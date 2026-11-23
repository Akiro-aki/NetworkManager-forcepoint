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
4. on disconnect, sends the client `SIGINT` so it tears the tunnel down itself.

## Requirements

- NetworkManager with libnm (1.2 or newer), Python 3.9+, PyGObject, iproute2
- `forcepoint-client` 2.x, found at `/usr/bin/_forcepoint-client` (the AUR
  `forcepoint-client-bin` package, whose bundled libevent is added to
  `LD_LIBRARY_PATH` automatically), `/usr/sbin/forcepoint-client` (the Debian
  package) or `/usr/bin/forcepoint-client`; or set `client=` yourself

## Install

Arch Linux:

```sh
makepkg -si
```

Anywhere else:

```sh
make test
sudo make install            # PREFIX=/usr by default
```

Then make D-Bus and NetworkManager notice the new files:

```sh
sudo systemctl reload dbus
sudo systemctl restart NetworkManager
```

## Set up a connection

**1. Trust the gateway once.** The client asks you to check the gateway's
certificate on the first connection, which it can't do under NetworkManager.
Connect once in a terminal, accept the certificate, then press Ctrl+C:

```sh
sudo forcepoint-client -p 4443 vpn.example.org -u alice
```

The client saves what it learned, including the gateway's CA, to
`~/.config/forcepoint/sslvpn/<tunnel name>` (in your home when run with sudo).

**2. Create the NetworkManager connection**, pointing `config` at that file:

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
- **No editor in nm-connection-editor yet.** Create and edit connections with
  `nmcli`. They are listed and can be connected and disconnected from any
  NetworkManager front end.
- **No two-factor challenge** (`--challengecmd`) support.
- **IPv4 only.** IPv6 addresses sent by the gateway are ignored.

## Troubleshooting

```sh
journalctl -u NetworkManager -b | grep nm-forcepoint
```

The client's own output is logged with a `client:` prefix. For more detail:
`sudo nmcli general logging level DEBUG domains VPN_PLUGIN`.

## Uninstall

`sudo pacman -R networkmanager-forcepoint`, or `sudo make uninstall`.

## License

MIT. See [LICENSE](LICENSE).
