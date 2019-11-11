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

#ifndef CLIENT_IM_H
#define CLIENT_IM_H

#include "fcitxgwatcher.h"
#include <gio/gio.h>

G_BEGIN_DECLS

/*
 * Type macros
 */

/* define GOBJECT macros */
#define FCITX_G_TYPE_CLIENT (fcitx_g_client_get_type())
#define FCITX_G_CLIENT(o)                                                      \
  (G_TYPE_CHECK_INSTANCE_CAST((o), FCITX_G_TYPE_CLIENT, FcitxGClient))
#define FCITX_G_IS_CLIENT(object)                                              \
  (G_TYPE_CHECK_INSTANCE_TYPE((object), FCITX_G_TYPE_CLIENT))
#define FCITX_G_CLIENT_CLASS(k)                                                \
  (G_TYPE_CHECK_CLASS_CAST((k), FCITX_G_TYPE_CLIENT, FcitxGClientClass))
#define FCITX_G_CLIENT_GET_CLASS(o)                                            \
  (G_TYPE_INSTANCE_GET_CLASS((o), FCITX_G_TYPE_CLIENT, FcitxGClientClass))

typedef struct _FcitxGClient FcitxGClient;
typedef struct _FcitxGClientClass FcitxGClientClass;
typedef struct _FcitxGClientPrivate FcitxGClientPrivate;
typedef struct _FcitxGPreeditItem FcitxGPreeditItem;

struct _FcitxGClient {
  GObject parent_instance;
  /* instance member */
  FcitxGClientPrivate *priv;
};

struct _FcitxGClientClass {
  GObjectClass parent_class;
  /* signals */

  /*< private >*/
  /* padding */
};

struct _FcitxGPreeditItem {
  gchar *string;
  gint32 type;
};

GType fcitx_g_client_get_type(void) G_GNUC_CONST;
FcitxGClient *fcitx_g_client_new();
FcitxGClient *fcitx_g_client_new_with_watcher(FcitxGWatcher *watcher);
gboolean fcitx_g_client_is_valid(FcitxGClient *self);
gboolean fcitx_g_client_process_key_sync(FcitxGClient *self, guint32 keyval,
                                         guint32 keycode, guint32 state,
                                         gboolean isRelease, guint32 t);
void fcitx_g_client_process_key(FcitxGClient *self, guint32 keyval,
                                guint32 keycode, guint32 state,
                                gboolean isRelease, guint32 t,
                                gint timeout_msec, GCancellable *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer user_data);
gboolean fcitx_g_client_process_key_finish(FcitxGClient *self,
                                           GAsyncResult *res);
const guint8 *fcitx_g_client_get_uuid(FcitxGClient *self);
void fcitx_g_client_focus_in(FcitxGClient *self);
void fcitx_g_client_focus_out(FcitxGClient *self);
void fcitx_g_client_set_display(FcitxGClient *self, const gchar *display);
void fcitx_g_client_set_cursor_rect(FcitxGClient *self, gint x, gint y, gint w,
                                    gint h);
void fcitx_g_client_set_surrounding_text(FcitxGClient *self, gchar *text,
                                         guint cursor, guint anchor);
void fcitx_g_client_set_capability(FcitxGClient *self, guint64 flags);

void fcitx_g_client_reset(FcitxGClient *self);

G_END_DECLS

#endif // CLIENT_IM_H
