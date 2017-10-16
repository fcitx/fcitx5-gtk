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

#include "fcitxconnection.h"
#include "fcitxgclient_export.h"
#include "marshall.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/**
 * FcitxConnection:
 *
 * A FcitxConnection allow to create a input context via DBus
 */

#define fcitx_gclient_debug(...)                                               \
    g_log("fcitx-connection", G_LOG_LEVEL_DEBUG, __VA_ARGS__)
struct _FcitxConnectionPrivate {
    char servicename[64];
    guint watch_id;
    GCancellable *cancellable;
    GDBusConnection *connection;
};

FCITXGCLIENT_EXPORT
GType fcitx_connection_get_type(void) G_GNUC_CONST;

G_DEFINE_TYPE(FcitxConnection, fcitx_connection, G_TYPE_OBJECT);

#define FCITX_CONNECTION_GET_PRIVATE(obj)                                      \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), FCITX_TYPE_CONNECTION,                 \
                                 FcitxConnectionPrivate))

enum { CONNECTED_SIGNAL, DISCONNECTED_SIGNAL, LAST_SIGNAL };

static guint signals[LAST_SIGNAL] = {0};
static void _fcitx_connection_connect(FcitxConnection *self);
static void fcitx_connection_init(FcitxConnection *self);
static void fcitx_connection_finalize(GObject *object);
static void fcitx_connection_dispose(GObject *object);
static void _fcitx_connection_appear(GDBusConnection *connection,
                                     const gchar *name, const gchar *name_owner,
                                     gpointer user_data);
static void _fcitx_connection_vanish(GDBusConnection *connection,
                                     const gchar *name, gpointer user_data);
static void _fcitx_connection_bus_finished(GObject *source_object,
                                           GAsyncResult *res,
                                           gpointer user_data);
static void _fcitx_connection_clean_up(FcitxConnection *self,
                                       gboolean dont_emit_disconn);
static void _fcitx_connection_unwatch(FcitxConnection *self);
static void _fcitx_connection_watch(FcitxConnection *self);

static void fcitx_connection_class_init(FcitxConnectionClass *klass);

static void fcitx_connection_finalize(GObject *object) {
    if (G_OBJECT_CLASS(fcitx_connection_parent_class)->finalize != NULL)
        G_OBJECT_CLASS(fcitx_connection_parent_class)->finalize(object);
}

static void fcitx_connection_dispose(GObject *object) {
    FcitxConnection *self = FCITX_CONNECTION(object);

    _fcitx_connection_unwatch(self);

    _fcitx_connection_clean_up(self, TRUE);

    if (G_OBJECT_CLASS(fcitx_connection_parent_class)->dispose != NULL)
        G_OBJECT_CLASS(fcitx_connection_parent_class)->dispose(object);
}

static gboolean _fcitx_connection_new_service_appear(gpointer user_data) {
    FcitxConnection *self = (FcitxConnection *)user_data;
    if (!self->priv->connection ||
        g_dbus_connection_is_closed(self->priv->connection))
        _fcitx_connection_connect(self);
    return FALSE;
}

static void _fcitx_connection_appear(G_GNUC_UNUSED GDBusConnection *connection,
                                     G_GNUC_UNUSED const gchar *name,
                                     const gchar *name_owner,
                                     gpointer user_data) {
    FcitxConnection *self = (FcitxConnection *)user_data;
    gboolean new_owner_good = name_owner && (name_owner[0] != '\0');
    if (new_owner_good) {
        g_timeout_add_full(G_PRIORITY_DEFAULT, 100,
                           _fcitx_connection_new_service_appear,
                           g_object_ref(self), g_object_unref);
    }
}

static void _fcitx_connection_vanish(G_GNUC_UNUSED GDBusConnection *connection,
                                     G_GNUC_UNUSED const gchar *name,
                                     gpointer user_data) {
    FcitxConnection *self = (FcitxConnection *)user_data;
    _fcitx_connection_clean_up(self, FALSE);
}

static void fcitx_connection_init(FcitxConnection *self) {
    self->priv = FCITX_CONNECTION_GET_PRIVATE(self);

    sprintf(self->priv->servicename, "org.fcitx.Fcitx5");

    self->priv->connection = NULL;
    self->priv->cancellable = NULL;
    self->priv->watch_id = 0;

    _fcitx_connection_connect(self);
}

static void _fcitx_connection_connect(FcitxConnection *self) {
    fcitx_gclient_debug("_fcitx_connection_create_ic");
    _fcitx_connection_unwatch(self);
    _fcitx_connection_clean_up(self, FALSE);
    self->priv->cancellable = g_cancellable_new();

    g_object_ref(self);

    _fcitx_connection_watch(self);
    g_bus_get(G_BUS_TYPE_SESSION, self->priv->cancellable,
              _fcitx_connection_bus_finished, self);
};

static void
_fcitx_connection_connection_closed(G_GNUC_UNUSED GDBusConnection *connection,
                                    G_GNUC_UNUSED gboolean remote_peer_vanished,
                                    G_GNUC_UNUSED GError *error,
                                    gpointer user_data) {
    fcitx_gclient_debug("_fcitx_connection_connection_closed");
    FcitxConnection *self = (FcitxConnection *)user_data;
    _fcitx_connection_clean_up(self, FALSE);

    _fcitx_connection_watch(self);
}

static void _fcitx_connection_bus_finished(G_GNUC_UNUSED GObject *source_object,
                                           GAsyncResult *res,
                                           gpointer user_data) {
    fcitx_gclient_debug("_fcitx_connection_bus_finished");
    g_return_if_fail(user_data != NULL);
    g_return_if_fail(FCITX_IS_CONNECTION(user_data));

    FcitxConnection *self = (FcitxConnection *)user_data;
    if (self->priv->cancellable) {
        g_object_unref(self->priv->cancellable);
        self->priv->cancellable = NULL;
    }

    GDBusConnection *connection = g_bus_get_finish(res, NULL);

    if (connection) {
        _fcitx_connection_clean_up(self, FALSE);
        self->priv->connection = connection;
        g_signal_connect(connection, "closed",
                         G_CALLBACK(_fcitx_connection_connection_closed), self);
        g_signal_emit(self, signals[CONNECTED_SIGNAL], 0);
    }
    /* unref for _fcitx_connection_connect */
    g_object_unref(self);
}

static void _fcitx_connection_watch(FcitxConnection *self) {
    if (self->priv->watch_id)
        return;
    fcitx_gclient_debug("_fcitx_connection_watch");

    self->priv->watch_id = g_bus_watch_name(
        G_BUS_TYPE_SESSION, self->priv->servicename,
        G_BUS_NAME_WATCHER_FLAGS_NONE, _fcitx_connection_appear,
        _fcitx_connection_vanish, self, NULL);
}

static void _fcitx_connection_unwatch(FcitxConnection *self) {
    if (self->priv->watch_id)
        g_bus_unwatch_name(self->priv->watch_id);
    self->priv->watch_id = 0;
}

static void fcitx_connection_class_init(FcitxConnectionClass *klass) {
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose = fcitx_connection_dispose;
    gobject_class->finalize = fcitx_connection_finalize;

    g_type_class_add_private(klass, sizeof(FcitxConnectionPrivate));

    /* install signals */
    /**
     * FcitxConnection::connected:
     * @connection: A FcitxConnection
     *
     * Emit when connected to fcitx and created ic
     */
    signals[CONNECTED_SIGNAL] =
        g_signal_new("connected", FCITX_TYPE_CONNECTION, G_SIGNAL_RUN_LAST, 0,
                     NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

    /**
     * FcitxConnection::disconnected:
     * @connection: A FcitxConnection
     *
     * Emit when disconnected from fcitx
     */
    signals[DISCONNECTED_SIGNAL] = g_signal_new(
        "disconnected", FCITX_TYPE_CONNECTION, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
        g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

/**
 * fcitx_connection_new:
 *
 * New a #FcitxConnection
 *
 * Returns: A newly allocated #FcitxConnection
 **/
FCITXGCLIENT_EXPORT
FcitxConnection *fcitx_connection_new() {
    FcitxConnection *self = g_object_new(FCITX_TYPE_CONNECTION, NULL);
    return FCITX_CONNECTION(self);
}

/**
 * fcitx_connection_is_valid:
 * @connection: A #FcitxConnection
 *
 * Check #FcitxConnection is valid to communicate with Fcitx
 *
 * Returns: #FcitxConnection is valid or not
 **/
FCITXGCLIENT_EXPORT
gboolean fcitx_connection_is_valid(FcitxConnection *self) {
    return self->priv->connection != NULL;
}

/**
 * fcitx_connection_get_g_dbus_connection:
 * @connection: A #FcitxConnection
 *
 * Return the current #GDBusConnection
 *
 * Returns: (transfer none): #GDBusConnection for current connection
 **/
FCITXGCLIENT_EXPORT
GDBusConnection *
fcitx_connection_get_g_dbus_connection(FcitxConnection *connection) {
    return connection->priv->connection;
}

static void _fcitx_connection_clean_up(FcitxConnection *self,
                                       gboolean dont_emit_disconn) {
    if (self->priv->connection) {
        g_signal_handlers_disconnect_by_func(
            self->priv->connection,
            G_CALLBACK(_fcitx_connection_connection_closed), self);
        g_object_unref(self->priv->connection);
        self->priv->connection = NULL;
        if (!dont_emit_disconn)
            g_signal_emit(self, signals[DISCONNECTED_SIGNAL], 0);
    }
}

// kate: indent-mode cstyle; replace-tabs on;
