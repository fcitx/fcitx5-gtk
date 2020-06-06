/*
 * SPDX-FileCopyrightText: 2017~2017 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "fcitxgwatcher.h"

#define FCITX_MAIN_SERVICE_NAME "org.fcitx.Fcitx5"
#define FCITX_PORTAL_SERVICE_NAME "org.freedesktop.portal.Fcitx"

typedef struct _FcitxGWatcherPrivate FcitxGWatcherPrivate;

struct _FcitxGWatcher {
    GObject parent_instance;
    /* instance member */
    FcitxGWatcherPrivate *priv;
};

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

G_DEFINE_TYPE_WITH_PRIVATE(FcitxGWatcher, fcitx_g_watcher, G_TYPE_OBJECT);

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

    /* install signals */
    /**
     * FcitxGWatcher::availability-changed:
     * @watcher: A FcitxGWatcher
     * @available: whether fcitx service is available.
     *
     * Emit when connected to fcitx and created ic
     */
    signals[AVAILABLITY_CHANGED_SIGNAL] = g_signal_new(
        "availability-changed", FCITX_G_TYPE_WATCHER, G_SIGNAL_RUN_LAST, 0,
        NULL, NULL, g_cclosure_marshal_VOID__BOOLEAN, G_TYPE_NONE, 1,
        G_TYPE_BOOLEAN);
}

static void fcitx_g_watcher_init(FcitxGWatcher *self) {
    self->priv = fcitx_g_watcher_get_instance_private(self);

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

/**
 * fcitx_g_watcher_watch
 * @self: a #FcitxGWatcher
 *
 * Watch for the fcitx serivce.
 */
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

/**
 * fcitx_g_watcher_unwatch
 * @self: a #FcitxGWatcher
 *
 * Unwatch for the fcitx serivce, should only be called after
 * calling fcitx_g_watcher_watch.
 */
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
gboolean fcitx_g_watcher_is_service_available(FcitxGWatcher *self) {
    return self->priv->available;
}

/**
 * fcitx_g_watcher_get_connection:
 * self: A #FcitxGWatcher
 *
 * Return the current #GDBusConnection
 *
 * Returns: (transfer none): #GDBusConnection for current connection
 **/
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

/**
 * fcitx_g_watcher_set_watch_portal:
 * self: A #FcitxGWatcher
 * watch: to monitor the portal service or not.
 *
 **/
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

/**
 * fcitx_g_watcher_get_service_name:
 * @self: A #FcitxGWatcher
 *
 * Returns: (transfer none): an available service name.
 *
 **/
const gchar *fcitx_g_watcher_get_service_name(FcitxGWatcher *self) {
    if (self->priv->main_owner) {
        return self->priv->main_owner;
    }
    if (self->priv->portal_owner) {
        return self->priv->portal_owner;
    }
    return NULL;
}
