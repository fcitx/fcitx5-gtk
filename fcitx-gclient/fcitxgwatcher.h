/*
 * SPDX-FileCopyrightText: 2017~2017 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
