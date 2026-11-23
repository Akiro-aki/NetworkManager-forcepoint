// SPDX-License-Identifier: MIT
/*
 * Settings page for Forcepoint SSL VPN connections in nm-connection-editor
 * (a libnm NMVpnEditorPlugin, GTK 3).
 *
 * Not affiliated with or endorsed by Forcepoint.
 */

#include <NetworkManager.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>

#define FP_SERVICE     "org.freedesktop.NetworkManager.forcepoint"
#define FP_NAME        "Forcepoint SSL VPN"
#define FP_DESCRIPTION "Forcepoint NGFW SSL VPN, through forcepoint-client"

/* vpn.data keys this page edits; any other key is kept as it was. */
static const char *const managed_keys[] = {"gateway", "port", "config", "user", "dns", NULL};

/* ------------------------------------------------------------------------ */
/* Editor                                                                   */
/* ------------------------------------------------------------------------ */

#define FP_TYPE_EDITOR (fp_editor_get_type())
G_DECLARE_FINAL_TYPE(FpEditor, fp_editor, FP, EDITOR, GObject)

struct _FpEditor {
    GObject       parent;
    GtkWidget    *grid;
    GtkWidget    *gateway;
    GtkWidget    *port;
    GtkWidget    *config;
    GtkWidget    *user;
    GtkWidget    *password;
    GtkWidget    *show_password;
    GtkWidget    *dns;
    NMSettingVpn *original; /* copy of the connection's vpn setting, or NULL */
};

static void fp_editor_interface_init(NMVpnEditorInterface *iface);

G_DEFINE_TYPE_WITH_CODE(FpEditor, fp_editor, G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(NM_TYPE_VPN_EDITOR, fp_editor_interface_init))

static void
emit_changed(FpEditor *self)
{
    g_signal_emit_by_name(self, "changed");
}

static void
on_entry_changed(GtkEditable *editable, gpointer user_data)
{
    emit_changed(FP_EDITOR(user_data));
}

static void
on_toggled(GtkToggleButton *button, gpointer user_data)
{
    emit_changed(FP_EDITOR(user_data));
}

static void
on_show_password(GtkToggleButton *button, gpointer user_data)
{
    FpEditor *self = FP_EDITOR(user_data);

    gtk_entry_set_visibility(GTK_ENTRY(self->password), gtk_toggle_button_get_active(button));
}

static void
on_browse(GtkButton *button, gpointer user_data)
{
    FpEditor  *self   = FP_EDITOR(user_data);
    GtkWidget *top    = gtk_widget_get_toplevel(self->grid);
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Choose the client's saved gateway file",
                                                    GTK_IS_WINDOW(top) ? GTK_WINDOW(top) : NULL,
                                                    GTK_FILE_CHOOSER_ACTION_OPEN,
                                                    "_Cancel", GTK_RESPONSE_CANCEL,
                                                    "_Select", GTK_RESPONSE_ACCEPT,
                                                    NULL);
    const char *current = gtk_entry_get_text(GTK_ENTRY(self->config));
    char       *dir     = g_build_filename(g_get_home_dir(), ".config", "forcepoint", "sslvpn", NULL);

    if (current && *current)
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), current);
    else if (g_file_test(dir, G_FILE_TEST_IS_DIR))
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), dir);
    gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(dialog), TRUE);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

        gtk_entry_set_text(GTK_ENTRY(self->config), path ? path : "");
        g_free(path);
    }
    gtk_widget_destroy(dialog);
    g_free(dir);
}

static GtkWidget *
add_row(GtkGrid *grid, int row, const char *label_text, GtkWidget *field, const char *tooltip)
{
    GtkWidget *label = gtk_label_new_with_mnemonic(label_text);

    gtk_label_set_xalign(GTK_LABEL(label), 1.0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), field);
    gtk_widget_set_hexpand(field, TRUE);
    if (tooltip)
        gtk_widget_set_tooltip_text(field, tooltip);
    gtk_grid_attach(grid, label, 0, row, 1, 1);
    gtk_grid_attach(grid, field, 1, row, 1, 1);
    return field;
}

static GtkWidget *
new_entry(FpEditor *self, const char *placeholder)
{
    GtkWidget *entry = gtk_entry_new();

    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    if (placeholder)
        gtk_entry_set_placeholder_text(GTK_ENTRY(entry), placeholder);
    g_signal_connect(entry, "changed", G_CALLBACK(on_entry_changed), self);
    return entry;
}

static GtkWidget *
section_label(const char *text)
{
    GtkWidget *label  = gtk_label_new(NULL);
    char      *markup = g_markup_printf_escaped("<b>%s</b>", text);

    gtk_label_set_markup(GTK_LABEL(label), markup);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);
    g_free(markup);
    return label;
}

static void
build_widget(FpEditor *self)
{
    GtkGrid   *grid;
    GtkWidget *box, *browse;
    int        row = 0;

    self->grid = gtk_grid_new();
    g_object_ref_sink(self->grid);
    grid = GTK_GRID(self->grid);
    gtk_grid_set_row_spacing(grid, 6);
    gtk_grid_set_column_spacing(grid, 12);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 12);

    gtk_grid_attach(grid, section_label("Gateway"), 0, row++, 2, 1);

    self->config = new_entry(self, "~/.config/forcepoint/sslvpn/<tunnel name>");
    browse       = gtk_button_new_with_mnemonic("_Browse…");
    g_signal_connect(browse, "clicked", G_CALLBACK(on_browse), self);
    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_hexpand(self->config, TRUE);
    gtk_box_pack_start(GTK_BOX(box), self->config, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), browse, FALSE, FALSE, 0);
    add_row(grid, row++, "Saved _config:", box,
            "The file forcepoint-client saved after you first connected and accepted the "
            "gateway's certificate. It pins the gateway's CA. Recommended.");
    gtk_label_set_mnemonic_widget(
        GTK_LABEL(gtk_grid_get_child_at(grid, 0, row - 1)), self->config);

    self->gateway = add_row(grid, row++, "_Gateway:", new_entry(self, "taken from the saved config"),
                            "Host name or IP address. Needed only without a saved config.");
    self->port    = add_row(grid, row++, "_Port:", new_entry(self, "443, or the saved config's"),
                            "Leave empty to use the saved config's port, or 443.");

    gtk_grid_attach(grid, section_label("Authentication"), 0, row++, 2, 1);
    self->user     = add_row(grid, row++, "_User name:", new_entry(self, NULL), NULL);
    self->password = add_row(grid, row++, "Pass_word:", new_entry(self, NULL),
                             "Stored by NetworkManager, readable only by root.");
    gtk_entry_set_visibility(GTK_ENTRY(self->password), FALSE);
    gtk_entry_set_input_purpose(GTK_ENTRY(self->password), GTK_INPUT_PURPOSE_PASSWORD);

    self->show_password = gtk_check_button_new_with_mnemonic("Sh_ow password");
    g_signal_connect(self->show_password, "toggled", G_CALLBACK(on_show_password), self);
    gtk_grid_attach(grid, self->show_password, 1, row++, 1, 1);

    gtk_grid_attach(grid, section_label("Network"), 0, row++, 2, 1);
    self->dns = gtk_check_button_new_with_mnemonic("Use _DNS servers sent by the gateway");
    g_signal_connect(self->dns, "toggled", G_CALLBACK(on_toggled), self);
    gtk_grid_attach(grid, self->dns, 1, row++, 1, 1);

    gtk_widget_show_all(self->grid);
}

static void
set_entry(GtkWidget *entry, const char *value)
{
    gtk_entry_set_text(GTK_ENTRY(entry), value ? value : "");
}

static void
fill_from_connection(FpEditor *self, NMConnection *connection)
{
    NMSettingVpn *s_vpn = connection ? nm_connection_get_setting_vpn(connection) : NULL;
    const char   *dns;

    if (!s_vpn) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(self->dns), TRUE);
        return;
    }
    self->original = NM_SETTING_VPN(nm_setting_duplicate(NM_SETTING(s_vpn)));
    set_entry(self->config, nm_setting_vpn_get_data_item(s_vpn, "config"));
    set_entry(self->gateway, nm_setting_vpn_get_data_item(s_vpn, "gateway"));
    set_entry(self->port, nm_setting_vpn_get_data_item(s_vpn, "port"));
    set_entry(self->user, nm_setting_vpn_get_data_item(s_vpn, "user"));
    set_entry(self->password, nm_setting_vpn_get_secret(s_vpn, "password"));
    dns = nm_setting_vpn_get_data_item(s_vpn, "dns");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(self->dns),
                                 !(dns && (!g_ascii_strcasecmp(dns, "no")
                                           || !g_ascii_strcasecmp(dns, "false")
                                           || !strcmp(dns, "0"))));
}

static char *
entry_text(GtkWidget *entry)
{
    return g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(entry))));
}

static gboolean
is_managed(const char *key)
{
    return g_strv_contains(managed_keys, key) || !strcmp(key, "password-flags");
}

static void
copy_unmanaged(const char *key, const char *value, gpointer user_data)
{
    if (!is_managed(key))
        nm_setting_vpn_add_data_item(NM_SETTING_VPN(user_data), key, value);
}

static gboolean
update_connection(NMVpnEditor *editor, NMConnection *connection, GError **error)
{
    FpEditor     *self     = FP_EDITOR(editor);
    NMSettingVpn *s_vpn    = NM_SETTING_VPN(nm_setting_vpn_new());
    char         *config   = entry_text(self->config);
    char         *gateway  = entry_text(self->gateway);
    char         *port     = entry_text(self->port);
    char         *user     = entry_text(self->user);
    const char   *password = gtk_entry_get_text(GTK_ENTRY(self->password));
    gboolean      ok       = FALSE;

    g_object_set(s_vpn, NM_SETTING_VPN_SERVICE_TYPE, FP_SERVICE, NULL);

    if (!*config && !*gateway) {
        g_set_error(error, NM_CONNECTION_ERROR, NM_CONNECTION_ERROR_INVALID_PROPERTY,
                    "choose the saved config or enter a gateway");
        goto out;
    }
    if (*port) {
        char  *end = NULL;
        gint64 n   = g_ascii_strtoll(port, &end, 10);

        if (!end || *end || n < 1 || n > 65535) {
            g_set_error(error, NM_CONNECTION_ERROR, NM_CONNECTION_ERROR_INVALID_PROPERTY,
                        "the port must be a number from 1 to 65535");
            goto out;
        }
    }
    if (!*user) {
        g_set_error(error, NM_CONNECTION_ERROR, NM_CONNECTION_ERROR_INVALID_PROPERTY,
                    "enter a user name");
        goto out;
    }

    if (self->original)
        nm_setting_vpn_foreach_data_item(self->original, copy_unmanaged, s_vpn);
    if (*config)
        nm_setting_vpn_add_data_item(s_vpn, "config", config);
    if (*gateway)
        nm_setting_vpn_add_data_item(s_vpn, "gateway", gateway);
    if (*port)
        nm_setting_vpn_add_data_item(s_vpn, "port", port);
    nm_setting_vpn_add_data_item(s_vpn, "user", user);
    if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(self->dns)))
        nm_setting_vpn_add_data_item(s_vpn, "dns", "no");

    /* The service can't ask for a missing password, so it is always stored
     * by NetworkManager (password-flags=0). */
    if (password && *password)
        nm_setting_vpn_add_secret(s_vpn, "password", password);
    nm_setting_set_secret_flags(NM_SETTING(s_vpn), "password", NM_SETTING_SECRET_FLAG_NONE, NULL);

    nm_connection_add_setting(connection, NM_SETTING(g_steal_pointer(&s_vpn)));
    ok = TRUE;

out:
    g_clear_object(&s_vpn);
    g_free(config);
    g_free(gateway);
    g_free(port);
    g_free(user);
    return ok;
}

static GObject *
get_widget(NMVpnEditor *editor)
{
    return G_OBJECT(FP_EDITOR(editor)->grid);
}

static void
fp_editor_dispose(GObject *object)
{
    FpEditor *self = FP_EDITOR(object);

    g_clear_object(&self->grid);
    g_clear_object(&self->original);
    G_OBJECT_CLASS(fp_editor_parent_class)->dispose(object);
}

static void
fp_editor_init(FpEditor *self)
{
}

static void
fp_editor_class_init(FpEditorClass *klass)
{
    G_OBJECT_CLASS(klass)->dispose = fp_editor_dispose;
}

static void
fp_editor_interface_init(NMVpnEditorInterface *iface)
{
    iface->get_widget        = get_widget;
    iface->update_connection = update_connection;
}

static NMVpnEditor *
fp_editor_new(NMConnection *connection)
{
    FpEditor *self = g_object_new(FP_TYPE_EDITOR, NULL);

    build_widget(self);
    fill_from_connection(self, connection);
    return NM_VPN_EDITOR(self);
}

/* ------------------------------------------------------------------------ */
/* Editor plugin                                                            */
/* ------------------------------------------------------------------------ */

#define FP_TYPE_EDITOR_PLUGIN (fp_editor_plugin_get_type())
G_DECLARE_FINAL_TYPE(FpEditorPlugin, fp_editor_plugin, FP, EDITOR_PLUGIN, GObject)

struct _FpEditorPlugin {
    GObject parent;
};

enum { PROP_0, PROP_NAME, PROP_DESC, PROP_SERVICE };

static void fp_editor_plugin_interface_init(NMVpnEditorPluginInterface *iface);

G_DEFINE_TYPE_WITH_CODE(FpEditorPlugin, fp_editor_plugin, G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(NM_TYPE_VPN_EDITOR_PLUGIN,
                                              fp_editor_plugin_interface_init))

static NMVpnEditor *
get_editor(NMVpnEditorPlugin *plugin, NMConnection *connection, GError **error)
{
    g_return_val_if_fail(!error || !*error, NULL);
    return fp_editor_new(connection);
}

static NMVpnEditorPluginCapability
get_capabilities(NMVpnEditorPlugin *plugin)
{
    return NM_VPN_EDITOR_PLUGIN_CAPABILITY_NONE;
}

static void
get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    switch (prop_id) {
    case PROP_NAME:
        g_value_set_string(value, FP_NAME);
        break;
    case PROP_DESC:
        g_value_set_string(value, FP_DESCRIPTION);
        break;
    case PROP_SERVICE:
        g_value_set_string(value, FP_SERVICE);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
fp_editor_plugin_init(FpEditorPlugin *self)
{
}

static void
fp_editor_plugin_class_init(FpEditorPluginClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->get_property = get_property;
    g_object_class_override_property(object_class, PROP_NAME, NM_VPN_EDITOR_PLUGIN_NAME);
    g_object_class_override_property(object_class, PROP_DESC, NM_VPN_EDITOR_PLUGIN_DESCRIPTION);
    g_object_class_override_property(object_class, PROP_SERVICE, NM_VPN_EDITOR_PLUGIN_SERVICE);
}

static void
fp_editor_plugin_interface_init(NMVpnEditorPluginInterface *iface)
{
    iface->get_editor       = get_editor;
    iface->get_capabilities = get_capabilities;
}

G_MODULE_EXPORT NMVpnEditorPlugin *
nm_vpn_editor_plugin_factory(GError **error)
{
    g_return_val_if_fail(!error || !*error, NULL);
    return g_object_new(FP_TYPE_EDITOR_PLUGIN, NULL);
}
