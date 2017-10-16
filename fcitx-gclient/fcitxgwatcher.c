/*
* Copyright (C) 2017~2017 by CSSlayer
* wengxt@gmail.com
*
* This library is free software; you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as
* published by the Free Software Foundation; either version 2.1 of the
* License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; see the file COPYING. If not,
* see <http://www.gnu.org/licenses/>.
*/

#include "fcitxgwatcher.h"

#define FCITX_MAIN_SERVICE_NAME "org.fcitx.Fcitx5"
#define FCITX_PORTAL_SERVICE_NAME "org.freedesktop.portal.Fcitx"

/**
 * FcitxGWatcher:
 *
 * A FcitxGWatcher allow to create a input context via DBus
 */
struct _FcitxGWatcherPrivate {
    gboolean watched;
    guint watch_id;
    guint portal_watch_id;
    gchar *main_owner, *portal_owner;
    gboolean watch_portal;
    gboolean available;

    GCancellable *cancellable;
    GDBusConnection *connection;
};

FCITXGCLIENT_EXPORT
GType fcitx_g_watcher_get_type(void) G_GNUC_CONST;

G_DEFINE_TYPE(FcitxGWatcher, fcitx_g_watcher, G_TYPE_OBJECT);

#define FCITX_G_WATCHER_GET_PRIVATE(obj)                                       \
    (G_TYPE_INSTANCE_GET_PRIVATE((obj), FCITX_G_TYPE_WATCHER,                  \
                                 FcitxGWatcherPrivate))

enum { AVAILABLITY_CHANGED_SIGNAL, LAST_SIGNAL };

static guint signals[LAST_SIGNAL] = {0};

static void _fcitx_g_watcher_clean_up(FcitxGWatcher *self);
static void _fcitx_g_watcher_get_bus_finished(GObject *source_object,
                                              GAsyncResult *res,
                                              gpointer user_data);
static void _fcitx_g_watcher_update_availability(FcitxGWatcher *self);

static void fcitx_g_watcher_finalize(GObject *object);
static void fcitx_g_watcher_dispose(GObject *object);

static void fcitx_g_watcher_class_init(FcitxGWatcherClass *klass) {
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose = fcitx_g_watcher_dispose;
    gobject_class->finalize = fcitx_g_watcher_finalize;

    g_type_class_add_private(klass, sizeof(FcitxGWatcherPrivate));

    /* install signals */
    /**
     * FcitxGWatcher::availability-changed:
     * @watcher: A FcitxGWatcher
     *
     * Emit when connected to fcitx and created ic
     */
    signals[AVAILABLITY_CHANGED_SIGNAL] = g_signal_new(
        "availability-changed", FCITX_G_TYPE_WATCHER, G_SIGNAL_RUN_LAST, 0, NULL,
        NULL, g_cclosure_marshal_VOID__BOOLEAN, G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}

static void fcitx_g_watcher_init(FcitxGWatcher *self) {
    self->priv = FCITX_G_WATCHER_GET_PRIVATE(self);

    self->priv->connection = NULL;
    self->priv->cancellable = NULL;
    self->priv->watch_id = 0;
    self->priv->portal_watch_id = 0;
    self->priv->main_owner = NULL;
    self->priv->portal_owner = NULL;
    self->priv->watched = FALSE;
}

static void fcitx_g_watcher_finalize(GObject *object) {
    if (G_OBJECT_CLASS(fcitx_g_watcher_parent_class)->finalize != NULL)
        G_OBJECT_CLASS(fcitx_g_watcher_parent_class)->finalize(object);
}

static void fcitx_g_watcher_dispose(GObject *object) {
    FcitxGWatcher *self = FCITX_G_WATCHER(object);

    if (self->priv->watched) {
        fcitx_g_watcher_unwatch(self);
    }

    if (G_OBJECT_CLASS(fcitx_g_watcher_parent_class)->dispose != NULL)
        G_OBJECT_CLASS(fcitx_g_watcher_parent_class)->dispose(object);
}

static void _fcitx_g_watcher_appear(G_GNUC_UNUSED GDBusConnection *conn,
                                    const gchar *name, const gchar *name_owner,
                                    gpointer user_data) {
    g_return_if_fail(FCITX_G_IS_WATCHER(user_data));

    FcitxGWatcher *self = FCITX_G_WATCHER(user_data);
    if (g_strcmp0(name, FCITX_MAIN_SERVICE_NAME) == 0) {
        g_free(self->priv->main_owner);
        self->priv->main_owner = g_strdup(name_owner);
    } else if (g_strcmp0(name, FCITX_PORTAL_SERVICE_NAME) == 0) {
        g_free(self->priv->portal_owner);
        self->priv->portal_owner = g_strdup(name_owner);
    }
    _fcitx_g_watcher_update_availability(self);
}

static void _fcitx_g_watcher_vanish(G_GNUC_UNUSED GDBusConnection *conn,
                                    const gchar *name, gpointer user_data) {
    g_return_if_fail(FCITX_G_IS_WATCHER(user_data));

    FcitxGWatcher *self = FCITX_G_WATCHER(user_data);
    if (g_strcmp0(name, FCITX_MAIN_SERVICE_NAME) == 0) {
        g_free(self->priv->main_owner);
        self->priv->main_owner = NULL;
    } else if (g_strcmp0(name, FCITX_PORTAL_SERVICE_NAME) == 0) {
        g_free(self->priv->portal_owner);
        self->priv->portal_owner = NULL;
    }
    _fcitx_g_watcher_update_availability(self);
}

FCITXGCLIENT_EXPORT
void fcitx_g_watcher_watch(FcitxGWatcher *self) {
    g_return_if_fail(!self->priv->watched);

    g_object_ref(self);
    g_bus_get(G_BUS_TYPE_SESSION, self->priv->cancellable,
              _fcitx_g_watcher_get_bus_finished, self);
    self->priv->watched = TRUE;
};

static void
_fcitx_g_watcher_get_bus_finished(G_GNUC_UNUSED GObject *source_object,
                                  GAsyncResult *res, gpointer user_data) {
    g_return_if_fail(user_data != NULL);
    g_return_if_fail(FCITX_G_IS_WATCHER(user_data));

    FcitxGWatcher *self = FCITX_G_WATCHER(user_data);
    _fcitx_g_watcher_clean_up(self);
    self->priv->connection = g_bus_get_finish(res, NULL);
    if (!self->priv->connection) {
        return;
    }

    self->priv->watch_id =
        g_bus_watch_name(G_BUS_TYPE_SESSION, FCITX_MAIN_SERVICE_NAME,
                         G_BUS_NAME_WATCHER_FLAGS_NONE, _fcitx_g_watcher_appear,
                         _fcitx_g_watcher_vanish, self, NULL);

    if (self->priv->watch_portal) {
        self->priv->portal_watch_id = g_bus_watch_name(
            G_BUS_TYPE_SESSION, FCITX_PORTAL_SERVICE_NAME,
            G_BUS_NAME_WATCHER_FLAGS_NONE, _fcitx_g_watcher_appear,
            _fcitx_g_watcher_vanish, self, NULL);
    }

    _fcitx_g_watcher_update_availability(self);

    /* unref for fcitx_g_watcher_watch */
    g_object_unref(self);
}

FCITXGCLIENT_EXPORT
void fcitx_g_watcher_unwatch(FcitxGWatcher *self) {
    g_return_if_fail(self->priv->watched);
    self->priv->watched = FALSE;
    _fcitx_g_watcher_clean_up(self);
}

/**
 * fcitx_g_watcher_new:
 *
 * New a #FcitxGWatcher
 *
 * Returns: A newly allocated #FcitxGWatcher
 **/
FCITXGCLIENT_EXPORT
FcitxGWatcher *fcitx_g_watcher_new() {
    FcitxGWatcher *self = g_object_new(FCITX_G_TYPE_WATCHER, NULL);
    return FCITX_G_WATCHER(self);
}

/**
 * fcitx_g_watcher_is_valid:
 * @connection: A #FcitxGWatcher
 *
 * Check #FcitxGWatcher is valid to communicate with Fcitx
 *
 * Returns: #FcitxGWatcher is valid or not
 **/
FCITXGCLIENT_EXPORT
gboolean fcitx_g_watcher_is_service_available(FcitxGWatcher *self) {
    return self->priv->available;
}

/**
 * fcitx_g_watcher_get_connection:
 * @connection: A #FcitxGWatcher
 *
 * Return the current #GDBusConnection
 *
 * Returns: (transfer none): #GDBusConnection for current connection
 **/
FCITXGCLIENT_EXPORT
GDBusConnection *fcitx_g_watcher_get_connection(FcitxGWatcher *self) {
    return self->priv->connection;
}

void _fcitx_g_watcher_clean_up(FcitxGWatcher *self) {
    if (self->priv->watch_id) {
        g_bus_unwatch_name(self->priv->watch_id);
        self->priv->watch_id = 0;
    }
    if (self->priv->portal_watch_id) {
        g_bus_unwatch_name(self->priv->portal_watch_id);
        self->priv->portal_watch_id = 0;
    }

    g_clear_pointer(&self->priv->main_owner, g_free);
    g_clear_pointer(&self->priv->portal_owner, g_free);
    g_clear_object(&self->priv->cancellable);
    g_clear_object(&self->priv->connection);
}

FCITXGCLIENT_EXPORT
void fcitx_g_watcher_set_watch_portal(FcitxGWatcher *self, gboolean watch) {
    self->priv->watch_portal = watch;
}

void _fcitx_g_watcher_update_availability(FcitxGWatcher *self) {
    gboolean available = self->priv->connection &&
                         (self->priv->main_owner || self->priv->portal_owner);
    if (available != self->priv->available) {
        self->priv->available = available;
        g_signal_emit(self, signals[AVAILABLITY_CHANGED_SIGNAL], 0);
    }
}

FCITXGCLIENT_EXPORT
const gchar *fcitx_g_watcher_get_service_name(FcitxGWatcher *self) {
    if (self->priv->main_owner) {
        return self->priv->main_owner;
    }
    if (self->priv->portal_owner) {
        return self->priv->portal_owner;
    }
    return NULL;
}