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
#ifndef _FCITX_GCLIENT_FCITXWATCHER_H_
#define _FCITX_GCLIENT_FCITXWATCHER_H_

#include <gio/gio.h>

/*
 * Type macros
 */

/* define GOBJECT macros */
#define FCITX_G_TYPE_WATCHER (fcitx_g_watcher_get_type())
#define FCITX_G_WATCHER(o)                                                     \
    (G_TYPE_CHECK_INSTANCE_CAST((o), FCITX_G_TYPE_WATCHER, FcitxGWatcher))
#define FCITX_G_IS_WATCHER(object)                                             \
    (G_TYPE_CHECK_INSTANCE_TYPE((object), FCITX_G_TYPE_WATCHER))
#define FCITX_G_WATCHER_CLASS(k)                                               \
    (G_TYPE_CHECK_CLASS_CAST((k), FCITX_G_TYPE_WATCHER, FcitxGWatcherClass))
#define FCITX_G_WATCHER_GET_CLASS(o)                                           \
    (G_TYPE_INSTANCE_GET_CLASS((o), FCITX_G_TYPE_WATCHER, FcitxGWatcherClass))

G_BEGIN_DECLS

typedef struct _FcitxGWatcher FcitxGWatcher;
typedef struct _FcitxGWatcherClass FcitxGWatcherClass;
typedef struct _FcitxGWatcherPrivate FcitxGWatcherPrivate;

struct _FcitxGWatcher {
    GObject parent_instance;
    /* instance member */
    FcitxGWatcherPrivate *priv;
};

struct _FcitxGWatcherClass {
    GObjectClass parent_class;
    /* signals */

    /*< private >*/
    /* padding */
};

GType fcitx_g_watcher_get_type(void) G_GNUC_CONST;
FcitxGWatcher *fcitx_g_watcher_new();

void fcitx_g_watcher_watch(FcitxGWatcher *self);
void fcitx_g_watcher_unwatch(FcitxGWatcher *self);

void fcitx_g_watcher_set_watch_portal(FcitxGWatcher *self, gboolean watch);
gboolean fcitx_g_watcher_is_service_available(FcitxGWatcher *self);
const gchar *fcitx_g_watcher_get_service_name(FcitxGWatcher *self);
GDBusConnection *fcitx_g_watcher_get_connection(FcitxGWatcher *self);

G_END_DECLS

#endif // _FCITX_GCLIENT_FCITXWATCHER_H_
