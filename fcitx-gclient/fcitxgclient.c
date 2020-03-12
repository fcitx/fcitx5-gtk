/***************************************************************************
 *   Copyright (C) 2012~2012 by CSSlayer                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.              *
 ***************************************************************************/

#include "fcitxgclient.h"
#include "fcitxgclient_export.h"
#include "fcitxgwatcher.h"
#include "marshall.h"

typedef struct _ProcessKeyStruct ProcessKeyStruct;

/**
 * FcitxGClient:
 *
 * A #FcitxGClient allow to create a input context via DBus
 */

enum {
    PROP_0,
    PROP_WATCHER,
};

struct _ProcessKeyStruct {
    FcitxGClient *self;
    GAsyncReadyCallback callback;
    void *user_data;
};

struct _FcitxGClientPrivate {
    GDBusProxy *improxy;
    GDBusProxy *icproxy;
    gchar *icname;
    guint8 uuid[16];
    gchar *display;
    gchar *program;

    GCancellable *cancellable;
    FcitxGWatcher *watcher;
    guint watch_id;
};

static const gchar introspection_xml[] =
    "<node>"
    "  <interface name=\"org.fcitx.Fcitx.InputMethod1\">"
    "    <method name=\"CreateInputContext\">\n"
    "      <arg direction=\"in\" type=\"a(ss)\"/>\n"
    "      <arg direction=\"out\" type=\"o\"/>\n"
    "      <arg direction=\"out\" type=\"ay\"/>\n"
    "    </method>\n"
    "  </interface>"
    "</node>";

static const gchar ic_introspection_xml[] =
    "<node>\n"
    "  <interface name=\"org.fcitx.Fcitx.InputContext1\">\n"
    "    <method name=\"FocusIn\">\n"
    "    </method>\n"
    "    <method name=\"FocusOut\">\n"
    "    </method>\n"
    "    <method name=\"Reset\">\n"
    "    </method>\n"
    "    <method name=\"SetCursorRect\">\n"
    "      <arg name=\"x\" direction=\"in\" type=\"i\"/>\n"
    "      <arg name=\"y\" direction=\"in\" type=\"i\"/>\n"
    "      <arg name=\"w\" direction=\"in\" type=\"i\"/>\n"
    "      <arg name=\"h\" direction=\"in\" type=\"i\"/>\n"
    "    </method>\n"
    "    <method name=\"SetCapability\">\n"
    "      <arg name=\"caps\" direction=\"in\" type=\"t\"/>\n"
    "    </method>\n"
    "    <method name=\"SetSurroundingText\">\n"
    "      <arg name=\"text\" direction=\"in\" type=\"s\"/>\n"
    "      <arg name=\"cursor\" direction=\"in\" type=\"u\"/>\n"
    "      <arg name=\"anchor\" direction=\"in\" type=\"u\"/>\n"
    "    </method>\n"
    "    <method name=\"SetSurroundingTextPosition\">\n"
    "      <arg name=\"cursor\" direction=\"in\" type=\"u\"/>\n"
    "      <arg name=\"anchor\" direction=\"in\" type=\"u\"/>\n"
    "    </method>\n"
    "    <method name=\"DestroyIC\">\n"
    "    </method>\n"
    "    <method name=\"ProcessKeyEvent\">\n"
    "      <arg name=\"keyval\" direction=\"in\" type=\"u\"/>\n"
    "      <arg name=\"keycode\" direction=\"in\" type=\"u\"/>\n"
    "      <arg name=\"state\" direction=\"in\" type=\"u\"/>\n"
    "      <arg name=\"isRelease\" direction=\"in\" type=\"b\"/>\n"
    "      <arg name=\"time\" direction=\"in\" type=\"u\"/>\n"
    "      <arg name=\"ret\" direction=\"out\" type=\"b\"/>\n"
    "    </method>\n"
    "    <signal name=\"CommitString\">\n"
    "      <arg name=\"str\" type=\"s\"/>\n"
    "    </signal>\n"
    "    <signal name=\"DeleteSurroundingText\">\n"
    "      <arg name=\"offset\" type=\"i\"/>\n"
    "      <arg name=\"nchar\" type=\"u\"/>\n"
    "    </signal>\n"
    "    <signal name=\"UpdateFormattedPreedit\">\n"
    "      <arg name=\"str\" type=\"a(si)\"/>\n"
    "      <arg name=\"cursorpos\" type=\"i\"/>\n"
    "    </signal>\n"
    "    <signal name=\"ForwardKey\">\n"
    "      <arg name=\"keyval\" type=\"u\"/>\n"
    "      <arg name=\"state\" type=\"u\"/>\n"
    "      <arg name=\"type\" type=\"b\"/>\n"
    "    </signal>\n"
    "  </interface>\n"
    "</node>\n";
FCITXGCLIENT_EXPORT
GType fcitx_g_client_get_type(void) G_GNUC_CONST;

G_DEFINE_TYPE(FcitxGClient, fcitx_g_client, G_TYPE_OBJECT);

#define FCITX_G_CLIENT_GET_PRIVATE(obj)                                        \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), FCITX_G_TYPE_CLIENT,                   \
                                 FcitxGClientPrivate))

enum {
    CONNECTED_SIGNAL,
    FORWARD_KEY_SIGNAL,
    COMMIT_STRING_SIGNAL,
    DELETE_SURROUNDING_TEXT_SIGNAL,
    UPDATED_FORMATTED_PREEDIT_SIGNAL,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

static GDBusInterfaceInfo *_fcitx_g_client_get_interface_info(void);
static GDBusInterfaceInfo *_fcitx_g_client_get_clientic_info(void);

static void _fcitx_g_client_update_availability(FcitxGClient *self);
static void _fcitx_g_client_availability_changed(FcitxGWatcher *connection,
                                                 gboolean avail,
                                                 gpointer user_data);
static void _fcitx_g_client_service_vanished(GDBusConnection *conn,
                                             const gchar *name,
                                             gpointer user_data);
static void _fcitx_g_client_create_ic(FcitxGClient *self);
static void _fcitx_g_client_create_ic_phase1_finished(GObject *source_object,
                                                      GAsyncResult *res,
                                                      gpointer user_data);
static void _fcitx_g_client_create_ic_cb(GObject *source_object,
                                         GAsyncResult *res, gpointer user_data);
static void _fcitx_g_client_create_ic_phase2_finished(GObject *source_object,
                                                      GAsyncResult *res,
                                                      gpointer user_data);
static void _fcitx_g_client_g_signal(GDBusProxy *proxy, gchar *sender_name,
                                     gchar *signal_name, GVariant *parameters,
                                     gpointer user_data);
static void _fcitx_g_client_clean_up(FcitxGClient *self);
static gboolean _fcitx_g_client_recheck(gpointer user_data);

static void fcitx_g_client_finalize(GObject *object);
static void fcitx_g_client_dispose(GObject *object);
static void fcitx_g_client_constructed(GObject *object);
static void fcitx_g_client_set_property(GObject *gobject, guint prop_id,
                                        const GValue *value, GParamSpec *pspec);

static void _item_free(gpointer arg);

#define STATIC_INTERFACE_INFO(FUNCTION, XML)                                   \
    static GDBusInterfaceInfo *FUNCTION(void) {                                \
        static gsize has_info = 0;                                             \
        static GDBusInterfaceInfo *info = NULL;                                \
        if (g_once_init_enter(&has_info)) {                                    \
            GDBusNodeInfo *introspection_data;                                 \
            introspection_data = g_dbus_node_info_new_for_xml(XML, NULL);      \
            info = introspection_data->interfaces[0];                          \
            g_once_init_leave(&has_info, 1);                                   \
        }                                                                      \
        return info;                                                           \
    }

STATIC_INTERFACE_INFO(_fcitx_g_client_get_interface_info, introspection_xml)
STATIC_INTERFACE_INFO(_fcitx_g_client_get_clientic_info, ic_introspection_xml)

static void fcitx_g_client_class_init(FcitxGClientClass *klass) {
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = fcitx_g_client_set_property;
    gobject_class->dispose = fcitx_g_client_dispose;
    gobject_class->finalize = fcitx_g_client_finalize;
    gobject_class->constructed = fcitx_g_client_constructed;

    g_type_class_add_private(klass, sizeof(FcitxGClientPrivate));

    g_object_class_install_property(
        gobject_class, PROP_WATCHER,
        g_param_spec_object("watcher", "Fcitx Watcher", "Fcitx Watcher",
                            FCITX_G_TYPE_WATCHER,
                            G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

    /* install signals */
    /**
     * FcitxGClient::connected:
     * @self: A #FcitxGClient
     *
     * Emit when connected to fcitx and created ic
     */
    signals[CONNECTED_SIGNAL] =
        g_signal_new("connected", FCITX_G_TYPE_CLIENT, G_SIGNAL_RUN_LAST, 0,
                     NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

    /**
     * FcitxGClient::forward-key:
     * @self: A #FcitxGClient
     * @keyval: key value
     * @state: key state
     * @type: event type
     *
     * Emit when input method ask for forward a key
     */
    signals[FORWARD_KEY_SIGNAL] =
        g_signal_new("forward-key", FCITX_G_TYPE_CLIENT, G_SIGNAL_RUN_LAST, 0,
                     NULL, NULL, fcitx_marshall_VOID__UINT_UINT_INT,
                     G_TYPE_NONE, 3, G_TYPE_UINT, G_TYPE_INT, G_TYPE_INT);
    /**
     * FcitxGClient::commit-string:
     * @self: A #FcitxGClient
     * @string: string to be commited
     *
     * Emit when input method commit one string
     */
    signals[COMMIT_STRING_SIGNAL] = g_signal_new(
        "commit-string", FCITX_G_TYPE_CLIENT, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
        g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1, G_TYPE_STRING);

    /**
     * FcitxGClient::delete-surrounding-text:
     * @self: A #FcitxGClient
     * @cursor: deletion start
     * @len: deletion length
     *
     * Emit when input method need to delete surrounding text
     */
    signals[DELETE_SURROUNDING_TEXT_SIGNAL] = g_signal_new(
        "delete-surrounding-text", FCITX_G_TYPE_CLIENT, G_SIGNAL_RUN_LAST, 0,
        NULL, NULL, fcitx_marshall_VOID__INT_UINT, G_TYPE_NONE, 2, G_TYPE_INT,
        G_TYPE_UINT);

    /**
     * FcitxGClient::update-formatted-preedit:
     * @self: A #FcitxGClient
     * @preedit: (transfer none) (element-type FcitxGPreeditItem): An
     * #FcitxGPreeditItem List
     * @cursor: cursor postion by utf8 byte
     *
     * Emit when input method need to delete surrounding text
     */
    signals[UPDATED_FORMATTED_PREEDIT_SIGNAL] = g_signal_new(
        "update-formatted-preedit", FCITX_G_TYPE_CLIENT, G_SIGNAL_RUN_LAST, 0,
        NULL, NULL, fcitx_marshall_VOID__BOXED_INT, G_TYPE_NONE, 2,
        G_TYPE_PTR_ARRAY, G_TYPE_INT);
}

static void fcitx_g_client_init(FcitxGClient *self) {
    self->priv = FCITX_G_CLIENT_GET_PRIVATE(self);

    self->priv->watcher = NULL;
    self->priv->cancellable = NULL;
    self->priv->improxy = NULL;
    self->priv->icproxy = NULL;
    self->priv->icname = NULL;
    self->priv->display = NULL;
    self->priv->program = NULL;
    self->priv->watch_id = 0;
}

static void fcitx_g_client_constructed(GObject *object) {
    FcitxGClient *self = FCITX_G_CLIENT(object);
    G_OBJECT_CLASS(fcitx_g_client_parent_class)->constructed(object);
    if (!self->priv->watcher) {
        self->priv->watcher = fcitx_g_watcher_new();
        g_object_ref_sink(self->priv->watcher);
        fcitx_g_watcher_watch(self->priv->watcher);
    }
    g_signal_connect(self->priv->watcher, "availability-changed",
                     (GCallback)_fcitx_g_client_availability_changed, self);
    _fcitx_g_client_availability_changed(
        self->priv->watcher,
        fcitx_g_watcher_is_service_available(self->priv->watcher), self);
}

static void fcitx_g_client_finalize(GObject *object) {
    if (G_OBJECT_CLASS(fcitx_g_client_parent_class)->finalize != NULL)
        G_OBJECT_CLASS(fcitx_g_client_parent_class)->finalize(object);
}

static void fcitx_g_client_dispose(GObject *object) {
    FcitxGClient *self = FCITX_G_CLIENT(object);

    if (self->priv->icproxy) {
        g_dbus_proxy_call(self->priv->icproxy, "DestroyIC", NULL,
                          G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
    }

    g_signal_handlers_disconnect_by_data(self->priv->watcher, self);
    _fcitx_g_client_clean_up(self);

    g_clear_pointer(&self->priv->display, g_free);

    if (G_OBJECT_CLASS(fcitx_g_client_parent_class)->dispose != NULL)
        G_OBJECT_CLASS(fcitx_g_client_parent_class)->dispose(object);
}

/**
 * fcitx_g_client_get_uuid
 * @self: a #FcitxGWatcher
 *
 * Returns: (transfer none): the current uuid of input context.
 */
FCITXGCLIENT_EXPORT
const guint8 *fcitx_g_client_get_uuid(FcitxGClient *self) {
    return self->priv->uuid;
}

/**
 * fcitx_g_client_focus_in:
 * @self: A #FcitxGClient
 *
 * tell fcitx current client has focus
 **/
FCITXGCLIENT_EXPORT
void fcitx_g_client_focus_in(FcitxGClient *self) {
    g_return_if_fail(fcitx_g_client_is_valid(self));
    g_dbus_proxy_call(self->priv->icproxy, "FocusIn", NULL,
                      G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

/**
 * fcitx_g_client_focus_out:
 * @self: A #FcitxGClient
 *
 * tell fcitx current client has lost focus
 **/
FCITXGCLIENT_EXPORT
void fcitx_g_client_focus_out(FcitxGClient *self) {
    g_return_if_fail(fcitx_g_client_is_valid(self));
    g_dbus_proxy_call(self->priv->icproxy, "FocusOut", NULL,
                      G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

/**
 * fcitx_g_client_reset:
 * @self: A #FcitxGClient
 *
 * tell fcitx current client is reset from client side
 **/
FCITXGCLIENT_EXPORT
void fcitx_g_client_reset(FcitxGClient *self) {
    g_return_if_fail(fcitx_g_client_is_valid(self));
    g_dbus_proxy_call(self->priv->icproxy, "Reset", NULL,
                      G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

/**
 * fcitx_g_client_set_capability:
 * @self: A #FcitxGClient
 * @flags: capability
 *
 * set client capability of input context.
 **/
FCITXGCLIENT_EXPORT
void fcitx_g_client_set_capability(FcitxGClient *self, guint64 flags) {
    g_return_if_fail(fcitx_g_client_is_valid(self));
    g_dbus_proxy_call(self->priv->icproxy, "SetCapability",
                      g_variant_new("(t)", flags), G_DBUS_CALL_FLAGS_NONE, -1,
                      NULL, NULL, NULL);
}

/**
 * fcitx_g_client_set_cursor_rect:
 * @self: A #FcitxGClient
 * @x: x of cursor
 * @y: y of cursor
 * @w: width of cursor
 * @h: height of cursor
 *
 * tell fcitx current client's cursor geometry info
 **/
FCITXGCLIENT_EXPORT
void fcitx_g_client_set_cursor_rect(FcitxGClient *self, gint x, gint y, gint w,
                                    gint h) {

    g_return_if_fail(fcitx_g_client_is_valid(self));
    g_dbus_proxy_call(self->priv->icproxy, "SetCursorRect",
                      g_variant_new("(iiii)", x, y, w, h),
                      G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

/**
 * fcitx_g_client_set_surrounding_text:
 * @self: A #FcitxGClient
 * @text: (transfer none) (allow-none): surroundng text
 * @cursor: cursor position coresponding to text
 * @anchor: anchor position coresponding to text
 **/
FCITXGCLIENT_EXPORT
void fcitx_g_client_set_surrounding_text(FcitxGClient *self, gchar *text,
                                         guint cursor, guint anchor) {
    g_return_if_fail(fcitx_g_client_is_valid(self));
    if (text) {
        g_dbus_proxy_call(self->priv->icproxy, "SetSurroundingText",
                          g_variant_new("(suu)", text, cursor, anchor),
                          G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
    } else {
        g_dbus_proxy_call(self->priv->icproxy, "SetSurroundingTextPosition",
                          g_variant_new("(uu)", cursor, anchor),
                          G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
    }
}

/**
 * fcitx_g_client_process_key_finish:
 * @self: A #FcitxGClient
 * @res: result
 *
 * use this function with #fcitx_g_client_process_key_async
 *
 * Returns: process key result
 **/
FCITXGCLIENT_EXPORT
gboolean fcitx_g_client_process_key_finish(FcitxGClient *self,
                                           GAsyncResult *res) {
    g_return_val_if_fail(fcitx_g_client_is_valid(self), FALSE);

    gboolean ret = FALSE;
    GVariant *result = g_dbus_proxy_call_finish(self->priv->icproxy, res, NULL);
    if (result) {
        g_variant_get(result, "(b)", &ret);
        g_variant_unref(result);
    }
    return ret;
}

void _process_key_data_free(ProcessKeyStruct *pk) {
    g_object_unref(pk->self);
    g_free(pk);
}

void _fcitx_g_client_process_key_cb(G_GNUC_UNUSED GObject *source_object,
                                    GAsyncResult *res, gpointer user_data) {
    ProcessKeyStruct *pk = user_data;
    pk->callback(G_OBJECT(pk->self), res, pk->user_data);
    _process_key_data_free(pk);
}

void _fcitx_g_client_process_key_cancelled(
    G_GNUC_UNUSED GCancellable *cancellable, gpointer user_data) {
    ProcessKeyStruct *pk = user_data;
    _process_key_data_free(pk);
}

/**
 * fcitx_g_client_process_key:
 * @self: A #FcitxGClient
 * @keyval: key value
 * @keycode: hardware key code
 * @state: key state
 * @isRelease: event type is key release
 * @t: timestamp
 * @timeout_msec: timeout in millisecond
 * @cancellable: cancellable
 * @callback: (scope async) (closure user_data): callback
 * @user_data: (closure): user data
 *
 * use this function with #fcitx_g_client_process_key_finish
 **/
FCITXGCLIENT_EXPORT
void fcitx_g_client_process_key(FcitxGClient *self, guint32 keyval,
                                guint32 keycode, guint32 state,
                                gboolean isRelease, guint32 t,
                                gint timeout_msec, GCancellable *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer user_data) {
    g_return_if_fail(fcitx_g_client_is_valid(self));
    ProcessKeyStruct *pk = g_new(ProcessKeyStruct, 1);
    pk->self = g_object_ref(self);
    pk->callback = callback;
    pk->user_data = user_data;
    g_dbus_proxy_call(
        self->priv->icproxy, "ProcessKeyEvent",
        g_variant_new("(uuubu)", keyval, keycode, state, isRelease, t),
        G_DBUS_CALL_FLAGS_NONE, timeout_msec, cancellable,
        _fcitx_g_client_process_key_cb, pk);
}

/**
 * fcitx_g_client_process_key_sync:
 * @self: A #FcitxGClient
 * @keyval: key value
 * @keycode: hardware key code
 * @state: key state
 * @isRelease: is key release
 * @t: timestamp
 *
 * send a key event to fcitx synchronizely
 *
 * Returns: the key is processed or not
 */
FCITXGCLIENT_EXPORT
gboolean fcitx_g_client_process_key_sync(FcitxGClient *self, guint32 keyval,
                                         guint32 keycode, guint32 state,
                                         gboolean isRelease, guint32 t) {
    g_return_val_if_fail(fcitx_g_client_is_valid(self), FALSE);
    gboolean ret = FALSE;
    GVariant *result = g_dbus_proxy_call_sync(
        self->priv->icproxy, "ProcessKeyEvent",
        g_variant_new("(uuubu)", keyval, keycode, state, isRelease, t),
        G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL);

    if (result) {
        g_variant_get(result, "(b)", &ret);
        g_variant_unref(result);
    }

    return ret;
}

static void
_fcitx_g_client_availability_changed(G_GNUC_UNUSED FcitxGWatcher *connection,
                                     G_GNUC_UNUSED gboolean avail,
                                     gpointer user_data) {
    FcitxGClient *self = user_data;
    _fcitx_g_client_update_availability(self);
}

static void _fcitx_g_client_update_availability(FcitxGClient *self) {
    g_timeout_add_full(G_PRIORITY_DEFAULT, 100, _fcitx_g_client_recheck,
                       g_object_ref(self), g_object_unref);
}

static gboolean _fcitx_g_client_recheck(gpointer user_data) {
    FcitxGClient *self = user_data;
    // Check we are not valid or in the process of create ic.
    if (!fcitx_g_client_is_valid(self) && self->priv->cancellable == NULL &&
        fcitx_g_watcher_is_service_available(self->priv->watcher)) {
        _fcitx_g_client_create_ic(self);
    }
    if (!fcitx_g_watcher_is_service_available(self->priv->watcher)) {
        _fcitx_g_client_clean_up(self);
    }
    return FALSE;
}

static void _fcitx_g_client_create_ic(FcitxGClient *self) {
    g_return_if_fail(fcitx_g_watcher_is_service_available(self->priv->watcher));

    _fcitx_g_client_clean_up(self);

    const gchar *service_name =
        fcitx_g_watcher_get_service_name(self->priv->watcher);
    GDBusConnection *connection =
        fcitx_g_watcher_get_connection(self->priv->watcher);
    self->priv->watch_id = g_bus_watch_name_on_connection(
        connection, service_name, G_BUS_NAME_WATCHER_FLAGS_NONE, NULL,
        _fcitx_g_client_service_vanished, self, NULL);

    self->priv->cancellable = g_cancellable_new();
    g_dbus_proxy_new(connection, G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                     _fcitx_g_client_get_interface_info(), service_name,
                     "/org/freedesktop/portal/inputmethod",
                     "org.fcitx.Fcitx.InputMethod1", self->priv->cancellable,
                     _fcitx_g_client_create_ic_phase1_finished,
                     g_object_ref(self));
}

static void
_fcitx_g_client_service_vanished(G_GNUC_UNUSED GDBusConnection *conn,
                                 G_GNUC_UNUSED const gchar *name,
                                 gpointer user_data) {
    FcitxGClient *self = user_data;
    _fcitx_g_client_clean_up(self);
    _fcitx_g_client_update_availability(self);
}

static void
_fcitx_g_client_create_ic_phase1_finished(G_GNUC_UNUSED GObject *source_object,
                                          GAsyncResult *res,
                                          gpointer user_data) {
    FcitxGClient *self = user_data;
    g_return_if_fail(user_data != NULL);
    g_return_if_fail(FCITX_G_IS_CLIENT(user_data));

    g_clear_object(&self->priv->cancellable);
    g_clear_object(&self->priv->improxy);

    self->priv->improxy = g_dbus_proxy_new_finish(res, NULL);
    if (!self->priv->improxy) {
        _fcitx_g_client_clean_up(self);
        g_object_unref(self);
        return;
    }

    self->priv->cancellable = g_cancellable_new();

    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a(ss)"));
    if (self->priv->display) {
        g_variant_builder_add(&builder, "(ss)", "display", self->priv->display);
    }
    if (self->priv->program) {
        g_variant_builder_add(&builder, "(ss)", "program", self->priv->program);
    }
    g_dbus_proxy_call(self->priv->improxy, "CreateInputContext",
                      g_variant_new("(a(ss))", &builder),
                      G_DBUS_CALL_FLAGS_NONE, -1, /* timeout */
                      self->priv->cancellable, _fcitx_g_client_create_ic_cb,
                      self);
}

static void _fcitx_g_client_create_ic_cb(GObject *source_object,
                                         GAsyncResult *res,
                                         gpointer user_data) {
    FcitxGClient *self = (FcitxGClient *)user_data;
    g_clear_object(&self->priv->cancellable);

    g_autoptr(GVariant) result =
        g_dbus_proxy_call_finish(G_DBUS_PROXY(source_object), res, NULL);

    if (!result) {
        _fcitx_g_client_clean_up(self);
        g_object_unref(self);
        return;
    }

    GVariantIter iter, inner;
    g_variant_iter_init(&iter, result);
    GVariant *pathVariant = g_variant_iter_next_value(&iter);
    const gchar *path = g_variant_get_string(pathVariant, NULL);
    GVariant *uuidVariant = g_variant_iter_next_value(&iter);
    size_t size = g_variant_iter_init(&inner, uuidVariant);
    if (size == 16) {
        int i = 0;
        GVariant *byte;
        while ((byte = g_variant_iter_next_value(&inner))) {
            self->priv->uuid[i] = g_variant_get_byte(byte);
            i++;
        }
    }

    self->priv->icname = g_strdup(path);
    self->priv->cancellable = g_cancellable_new();
    g_dbus_proxy_new(
        g_dbus_proxy_get_connection(self->priv->improxy),
        G_DBUS_PROXY_FLAGS_NONE, _fcitx_g_client_get_clientic_info(),
        g_dbus_proxy_get_name(self->priv->improxy), self->priv->icname,
        "org.fcitx.Fcitx.InputContext1", self->priv->cancellable,
        _fcitx_g_client_create_ic_phase2_finished, self);
}

static void
_fcitx_g_client_create_ic_phase2_finished(G_GNUC_UNUSED GObject *source_object,
                                          GAsyncResult *res,
                                          gpointer user_data) {
    g_return_if_fail(user_data != NULL);
    g_return_if_fail(FCITX_G_IS_CLIENT(user_data));
    FcitxGClient *self = (FcitxGClient *)user_data;
    g_clear_object(&self->priv->cancellable);
    g_clear_object(&self->priv->icproxy);
    self->priv->icproxy = g_dbus_proxy_new_finish(res, NULL);
    if (!self->priv->icproxy) {
        _fcitx_g_client_clean_up(self);
        g_object_unref(self);
        return;
    }

    g_signal_connect(self->priv->icproxy, "g-signal",
                     G_CALLBACK(_fcitx_g_client_g_signal), self);
    g_signal_emit(self, signals[CONNECTED_SIGNAL], 0);

    /* unref for _fcitx_g_client_create_ic_cb */
    g_object_unref(self);
}

static void _item_free(gpointer arg) {
    FcitxGPreeditItem *item = arg;
    g_free(item->string);
    g_free(item);
}

static void _fcitx_g_client_g_signal(G_GNUC_UNUSED GDBusProxy *proxy,
                                     G_GNUC_UNUSED gchar *sender_name,
                                     gchar *signal_name, GVariant *parameters,
                                     gpointer user_data) {
    if (g_strcmp0(signal_name, "CommitString") == 0) {
        const gchar *data = NULL;
        g_variant_get(parameters, "(s)", &data);
        if (data) {
            g_signal_emit(user_data, signals[COMMIT_STRING_SIGNAL], 0, data);
        }
    } else if (g_strcmp0(signal_name, "ForwardKey") == 0) {
        guint32 key, state;
        gboolean isRelease;
        g_variant_get(parameters, "(uub)", &key, &state, &isRelease);
        g_signal_emit(user_data, signals[FORWARD_KEY_SIGNAL], 0, key, state,
                      isRelease);
    } else if (g_strcmp0(signal_name, "DeleteSurroundingText") == 0) {
        guint32 nchar;
        gint32 offset;
        g_variant_get(parameters, "(iu)", &offset, &nchar);
        g_signal_emit(user_data, signals[DELETE_SURROUNDING_TEXT_SIGNAL], 0,
                      offset, nchar);
    } else if (g_strcmp0(signal_name, "UpdateFormattedPreedit") == 0) {
        int cursor_pos;
        GPtrArray *array = g_ptr_array_new_with_free_func(_item_free);
        GVariantIter *iter;
        g_variant_get(parameters, "(a(si)i)", &iter, &cursor_pos);

        gchar *string;
        int type;
        while (g_variant_iter_next(iter, "(si)", &string, &type, NULL)) {
            FcitxGPreeditItem *item = g_malloc0(sizeof(FcitxGPreeditItem));
            item->string = g_strdup(string);
            item->type = type;
            g_ptr_array_add(array, item);
            g_free(string);
        }
        g_variant_iter_free(iter);
        g_signal_emit(user_data, signals[UPDATED_FORMATTED_PREEDIT_SIGNAL], 0,
                      array, cursor_pos);
        g_ptr_array_free(array, TRUE);
    }
}

/**
 * fcitx_g_client_new:
 *
 * New a #FcitxGClient
 *
 * Returns: A newly allocated #FcitxGClient
 **/
FCITXGCLIENT_EXPORT
FcitxGClient *fcitx_g_client_new() {
    FcitxGClient *self = g_object_new(FCITX_G_TYPE_CLIENT, NULL);
    return FCITX_G_CLIENT(self);
}

/**
 * fcitx_g_client_new_with_connection:
 * @connection: the #FcitxConnection to be used with this client
 *
 * New a #FcitxGClient
 *
 * Returns: A newly allocated #FcitxGClient
 **/
FCITXGCLIENT_EXPORT
FcitxGClient *fcitx_g_client_new_with_watcher(FcitxGWatcher *watcher) {
    FcitxGClient *self =
        g_object_new(FCITX_G_TYPE_CLIENT, "watcher", watcher, NULL);
    return FCITX_G_CLIENT(self);
}

FCITXGCLIENT_EXPORT
void fcitx_g_client_set_display(FcitxGClient *self, const gchar *display) {
    g_free(self->priv->display);
    self->priv->display = g_strdup(display);
}

FCITXGCLIENT_EXPORT
void fcitx_g_client_set_program(FcitxGClient *self, const gchar *program) {
    g_free(self->priv->program);
    self->priv->program = g_strdup(program);
}

/**
 * fcitx_g_client_is_valid:
 * @self: A #FcitxGClient
 *
 * Check #FcitxGClient is valid to communicate with Fcitx
 *
 * Returns: #FcitxGClient is valid or not
 **/
FCITXGCLIENT_EXPORT
gboolean fcitx_g_client_is_valid(FcitxGClient *self) {
    return self->priv->icproxy != NULL;
}

static void fcitx_g_client_set_property(GObject *gobject, guint prop_id,
                                        const GValue *value,
                                        GParamSpec *pspec) {
    FcitxGClient *self = FCITX_G_CLIENT(gobject);
    FcitxGWatcher *watcher;
    switch (prop_id) {
    case PROP_WATCHER:
        watcher = g_value_get_object(value);
        if (watcher) {
            self->priv->watcher = watcher;
            g_object_ref_sink(self->priv->watcher);
        }
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(gobject, prop_id, pspec);
        break;
    }
}

static void _fcitx_g_client_clean_up(FcitxGClient *self) {
    if (self->priv->cancellable) {
        g_cancellable_cancel(self->priv->cancellable);
    }

    g_clear_object(&self->priv->cancellable);
    g_clear_object(&self->priv->improxy);
    g_clear_pointer(&self->priv->icname, g_free);

    if (self->priv->icproxy) {
        g_signal_handlers_disconnect_by_func(
            self->priv->icproxy, G_CALLBACK(_fcitx_g_client_g_signal), self);
    }
    g_clear_object(&self->priv->icproxy);
    if (self->priv->watch_id) {
        g_bus_unwatch_name(self->priv->watch_id);
        self->priv->watch_id = 0;
    }
}

// kate: indent-mode cstyle; replace-tabs on;
