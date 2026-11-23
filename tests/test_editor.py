# SPDX-License-Identifier: MIT
"""Load the built editor plugin the way nm-connection-editor does and drive it."""

import os
import unittest

import gi

gi.require_version("NM", "1.0")
from gi.repository import GLib, NM  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
SO = os.path.join(os.path.dirname(HERE), "build", "libnm-vpn-plugin-forcepoint-editor.so")
SERVICE = "org.freedesktop.NetworkManager.forcepoint"

Gtk = None
if os.path.exists(SO) and (os.environ.get("WAYLAND_DISPLAY") or os.environ.get("DISPLAY")):
    try:
        gi.require_version("Gtk", "3.0")
        from gi.repository import Gtk  # noqa: E402,F811
        if not Gtk.init_check()[0]:
            Gtk = None
    except (ValueError, ImportError):
        Gtk = None


def make_connection(data=None, secrets=None):
    con = NM.SimpleConnection.new()
    s_con = NM.SettingConnection.new()
    s_con.set_property("id", "test")
    s_con.set_property("uuid", NM.utils_uuid_generate())
    s_con.set_property("type", "vpn")
    con.add_setting(s_con)
    s_vpn = NM.SettingVpn.new()
    s_vpn.set_property("service-type", SERVICE)
    for k, v in (data or {}).items():
        s_vpn.add_data_item(k, v)
    for k, v in (secrets or {}).items():
        s_vpn.add_secret(k, v)
    con.add_setting(s_vpn)
    return con


def children(widget):
    out = [widget]
    if isinstance(widget, Gtk.Container):
        for c in widget.get_children():
            out += children(c)
    return out


@unittest.skipUnless(Gtk, "needs the built editor and a display")
class Editor(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # check_owner=-1: the build output is not root-owned.
        cls.plugin = NM.VpnEditorPlugin.load_from_file(SO, SERVICE, -1, lambda *a: True, None)

    def test_plugin_identity(self):
        self.assertEqual(self.plugin.get_property("service"), SERVICE)
        self.assertEqual(self.plugin.get_property("name"), "Forcepoint SSL VPN")

    def editor_for(self, con):
        editor = self.plugin.get_editor(con)
        # GtkGrid lists its children newest first; reverse into on-screen order.
        widgets = children(editor.get_widget())[::-1]
        entries = [w for w in widgets if isinstance(w, Gtk.Entry)]
        checks = [w for w in widgets if isinstance(w, Gtk.CheckButton)]
        return editor, entries, checks

    def test_round_trip_keeps_unknown_keys(self):
        con = make_connection({"config": "/tmp/x.xml", "user": "alice", "port": "4443",
                               "client": "/opt/fp/forcepoint-client"}, {"password": "s3cret"})
        editor, entries, checks = self.editor_for(con)
        texts = [e.get_text() for e in entries]
        self.assertEqual(texts, ["/tmp/x.xml", "", "4443", "alice", "s3cret"])
        self.assertFalse(entries[4].get_visibility())
        dns = [c for c in checks if "DNS" in c.get_label()][0]
        self.assertTrue(dns.get_active())

        entries[3].set_text("bob")
        dns.set_active(False)
        out = make_connection()
        self.assertTrue(editor.update_connection(out))
        s_vpn = out.get_setting_vpn()
        self.assertEqual(s_vpn.get_service_type(), SERVICE)
        self.assertEqual(s_vpn.get_data_item("user"), "bob")
        self.assertEqual(s_vpn.get_data_item("dns"), "no")
        self.assertEqual(s_vpn.get_data_item("client"), "/opt/fp/forcepoint-client")
        self.assertIsNone(s_vpn.get_data_item("gateway"))
        self.assertEqual(s_vpn.get_secret("password"), "s3cret")
        self.assertEqual(s_vpn.get_data_item("password-flags"), "0")

    def test_validation(self):
        cases = [
            ({"user": "alice"}, "gateway"),
            ({"gateway": "vpn.example.org"}, "user name"),
            ({"gateway": "vpn.example.org", "user": "a", "port": "99999"}, "port"),
        ]
        for data, word in cases:
            with self.subTest(data=data):
                editor, _, _ = self.editor_for(make_connection(data))
                with self.assertRaises(GLib.Error) as ctx:
                    editor.update_connection(make_connection())
                self.assertIn(word, ctx.exception.message)

    def test_changed_signal(self):
        editor, entries, _ = self.editor_for(make_connection({"gateway": "g", "user": "u"}))
        fired = []
        editor.connect("changed", lambda *_: fired.append(1))
        entries[1].set_text("other.example.org")
        self.assertTrue(fired)


if __name__ == "__main__":
    unittest.main()
