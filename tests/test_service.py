# SPDX-License-Identifier: MIT
"""Unit tests for the parts of nm-forcepoint-service that run without root."""

import importlib.machinery
import importlib.util
import os
import struct
import stat
import subprocess
import tempfile
import textwrap
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
LIBEXEC = os.path.join(os.path.dirname(HERE), "libexec")


def load_service():
    path = os.path.join(LIBEXEC, "nm-forcepoint-service")
    loader = importlib.machinery.SourceFileLoader("nm_forcepoint_service", path)
    spec = importlib.util.spec_from_loader(loader.name, loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


svc = load_service()

SAMPLE_XML = textwrap.dedent("""\
    <?xml version='1.0' encoding='UTF-8'?>
    <quicksec>
      <policy>
        <ssl-tunnel name="Example_-_Primary">
          <ssl-peer id="203.0.113.10" id-type="ip" port="4443">203.0.113.10</ssl-peer>
        </ssl-tunnel>
      </policy>
    </quicksec>
    """)


class ParseOptions(unittest.TestCase):
    def setUp(self):
        fd, self.xml = tempfile.mkstemp(suffix=".xml")
        with os.fdopen(fd, "w") as fh:
            fh.write(SAMPLE_XML)

    def tearDown(self):
        os.unlink(self.xml)

    def test_gateway_defaults_to_port_443(self):
        o = svc.parse_options({"gateway": "vpn.example.org", "user": "u"}, {"password": "p"})
        self.assertEqual((o["gateway"], o["port"], o["config"]), ("vpn.example.org", 443, None))
        self.assertTrue(o["dns"])

    def test_config_supplies_gateway_and_port(self):
        o = svc.parse_options({"config": self.xml, "user": "u"}, {"password": "p"})
        self.assertEqual((o["gateway"], o["port"]), ("203.0.113.10", 4443))

    def test_explicit_port_wins_over_config(self):
        o = svc.parse_options({"config": self.xml, "user": "u", "port": "8443"}, {"password": "p"})
        self.assertEqual(o["port"], 8443)

    def test_missing_pieces_are_errors(self):
        cases = [
            ({"user": "u"}, {"password": "p"}),                       # no gateway or config
            ({"gateway": "g"}, {"password": "p"}),                    # no user
            ({"gateway": "g", "user": "u"}, {}),                      # no password
            ({"gateway": "g", "user": "u", "port": "x"}, {"password": "p"}),
            ({"gateway": "g", "user": "u", "port": "70000"}, {"password": "p"}),
            ({"config": "/nonexistent.xml", "user": "u"}, {"password": "p"}),
        ]
        for data, secrets in cases:
            with self.subTest(data=data, secrets=secrets):
                with self.assertRaises(svc.ConfigError):
                    svc.parse_options(data, secrets)

    def test_dns_can_be_disabled(self):
        o = svc.parse_options({"gateway": "g", "user": "u", "dns": "no"}, {"password": "p"})
        self.assertFalse(o["dns"])


class BuildArgv(unittest.TestCase):
    def opts(self, **extra):
        base = {"gateway": "203.0.113.10", "port": 4443, "user": "alice",
                "password": "s3cret!", "config": None, "dns": True}
        base.update(extra)
        return base

    def test_password_never_on_command_line(self):
        argv = svc.build_argv("/usr/bin/_forcepoint-client", self.opts())
        self.assertNotIn("s3cret!", " ".join(argv))
        self.assertIn("--passcmd", argv)

    def test_gateway_mode(self):
        argv = svc.build_argv("/c", self.opts())
        self.assertEqual(argv[-3:], ["--port", "4443", "203.0.113.10"])
        self.assertEqual(argv[argv.index("--resolver") + 1], svc.RESOLVER_SHIM)

    def test_config_mode_and_no_dns(self):
        argv = svc.build_argv("/c", self.opts(config="/x.xml", dns=False))
        self.assertEqual(argv[-2:], ["--config", "/x.xml"])
        self.assertEqual(argv[argv.index("--resolver") + 1], "none")


class Events(unittest.TestCase):
    def test_parse(self):
        self.assertEqual(svc.parse_event("established"), ("established",))
        self.assertEqual(svc.parse_event("resolver dns tun0 10.0.0.1 10.0.0.2 fe80::1%tun0"),
                         ("dns", "tun0", ["10.0.0.1", "10.0.0.2", "fe80::1"]))
        self.assertEqual(svc.parse_event("resolver domain tun0 corp.example ~lan.example"),
                         ("domain", "tun0", ["corp.example", "lan.example"]))
        self.assertEqual(svc.parse_event("resolver default-route tun0 true")[0], "resolver-other")
        self.assertIsNone(svc.parse_event("   "))

    def test_hook_and_shim_write_to_fifo(self):
        with tempfile.TemporaryDirectory() as d:
            fifo = os.path.join(d, "events")
            os.mkfifo(fifo)
            fd = os.open(fifo, os.O_RDWR | os.O_NONBLOCK)
            env = dict(os.environ, NM_FORCEPOINT_EVENTS=fifo)
            subprocess.run([os.path.join(LIBEXEC, "nm-forcepoint-hook"), "established"],
                           env=env, check=True)
            # The client builds resolver commands as one shell line.
            shim = os.path.join(LIBEXEC, "resolvectl")
            subprocess.run(f"{shim} dns tun0 10.0.0.1 ; {shim} domain tun0 corp.example ; ",
                           shell=True, env=env, check=True)
            data = os.read(fd, 4096).decode()
            os.close(fd)
        self.assertEqual(data.splitlines(), ["established", "resolver dns tun0 10.0.0.1",
                                             "resolver domain tun0 corp.example"])

    def test_hook_is_silent_without_service(self):
        env = {k: v for k, v in os.environ.items() if k != "NM_FORCEPOINT_EVENTS"}
        r = subprocess.run([os.path.join(LIBEXEC, "nm-forcepoint-hook"), "established"],
                           env=env, capture_output=True)
        self.assertEqual((r.returncode, r.stdout, r.stderr), (0, b"", b""))

    def test_passcmd_prints_password(self):
        # forcepoint-client runs --passcmd through popen(), i.e. /bin/sh -c.
        cmd = svc.build_argv("/c", BuildArgv().opts())
        passcmd = cmd[cmd.index("--passcmd") + 1]
        out = subprocess.run(["/bin/sh", "-c", passcmd], capture_output=True, text=True,
                             env={"PATH": "/usr/bin:/bin", "NM_FORCEPOINT_PASSWORD": "pa ss'\"$x"})
        self.assertEqual(out.stdout.rstrip("\n"), "pa ss'\"$x")


class Ip4Config(unittest.TestCase):
    def u32(self, a):
        return svc.ip4_u32(a)

    def test_ip4_u32_is_network_order(self):
        self.assertEqual(struct.pack("=I", self.u32("10.8.0.200")), bytes([10, 8, 0, 200]))

    def test_split_tunnel(self):
        addr = [{"family": "inet", "local": "10.8.0.200", "prefixlen": 32}]
        routes = [
            {"dst": "10.0.0.0/8", "dev": "tun0", "scope": "link"},
            {"dst": "172.16.5.0/24", "dev": "tun0", "metric": 50},
            {"dst": "10.8.0.0/24", "dev": "tun0", "protocol": "kernel"},
        ]
        cfg = svc.build_ip4_config(addr, routes, ["10.0.0.1", "fe80::1"], ["corp.example"])
        self.assertEqual(cfg["address"].unpack(), self.u32("10.8.0.200"))
        self.assertEqual(cfg["prefix"].unpack(), 32)
        self.assertTrue(cfg["never-default"].unpack())
        self.assertEqual(cfg["routes"].unpack(), [[self.u32("10.0.0.0"), 8, 0, 0],
                                                   [self.u32("172.16.5.0"), 24, 0, 50]])
        self.assertEqual(cfg["dns"].unpack(), [self.u32("10.0.0.1")])
        self.assertEqual(cfg["domains"].unpack(), ["corp.example"])

    def test_full_tunnel(self):
        addr = [{"local": "10.8.0.200", "prefixlen": 32, "address": "10.8.0.1"}]
        cfg = svc.build_ip4_config(addr, [{"dst": "default", "dev": "tun0"}], [], [])
        self.assertFalse(cfg["never-default"].unpack())
        self.assertNotIn("routes", cfg)
        self.assertEqual(cfg["ptp"].unpack(), self.u32("10.8.0.1"))

    def test_no_address_is_an_error(self):
        with self.assertRaises(svc.ConfigError):
            svc.build_ip4_config([], [], [], [])


class Install(unittest.TestCase):
    def test_make_install_layout(self):
        root = os.path.dirname(HERE)
        with tempfile.TemporaryDirectory() as dest:
            subprocess.run(["make", "-s", "-C", root, f"DESTDIR={dest}", "install"], check=True)
            name = os.path.join(dest, "usr/lib/NetworkManager/VPN/nm-forcepoint-service.name")
            with open(name) as fh:
                content = fh.read()
            self.assertIn("program=/usr/lib/nm-forcepoint/nm-forcepoint-service", content)
            for exe in ("nm-forcepoint-service", "nm-forcepoint-hook", "resolvectl"):
                mode = os.stat(os.path.join(dest, "usr/lib/nm-forcepoint", exe)).st_mode
                self.assertTrue(mode & stat.S_IXUSR, exe)
            self.assertTrue(os.path.isfile(os.path.join(
                dest, "usr/share/dbus-1/system.d/nm-forcepoint-service.conf")))


if __name__ == "__main__":
    unittest.main()
