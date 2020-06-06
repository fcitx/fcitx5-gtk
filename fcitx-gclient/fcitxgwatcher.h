/*
 * SPDX-FileCopyrightText: 2017~2017 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef _FCITX_GCLIENT_FCITXWATCHER_H_
#define _FCITX_GCLIENT_FCITXWATCHER_H_

#include <gio/gio.h>

G_BEGIN_DECLS

/*
 * Type macros
 */

/* define GOBJECT macros */
#define FCITX_G_TYPE_WATCHER (fcitx_g_watcher_get_type())

G_DECLARE_FINAL_TYPE(FcitxGWatcher, fcitx_g_watcher, FCITX_G, WATCHER, GObject)

FcitxGWatcher *fcitx_g_watcher_new();

void fcitx_g_watcher_watch(FcitxGWatcher *self);
void fcitx_g_watcher_unwatch(FcitxGWatcher *self);

void fcitx_g_watcher_set_watch_portal(FcitxGWatcher *self, gboolean watch);
gboolean fcitx_g_watcher_is_service_available(FcitxGWatcher *self);
const gchar *fcitx_g_watcher_get_service_name(FcitxGWatcher *self);
GDBusConnection *fcitx_g_watcher_get_connection(FcitxGWatcher *self);

G_END_DECLS

#endif // _FCITX_GCLIENT_FCITXWATCHER_H_
