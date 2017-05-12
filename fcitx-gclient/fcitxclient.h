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


#include <gio/gio.h>
#include "fcitxconnection.h"

G_BEGIN_DECLS

/*
 * Type macros
 */

/* define GOBJECT macros */
#define FCITX_TYPE_CLIENT         (fcitx_client_get_type ())
#define FCITX_CLIENT(o) \
        (G_TYPE_CHECK_INSTANCE_CAST ((o), FCITX_TYPE_CLIENT, FcitxClient))
#define FCITX_IS_CLIENT(object) \
        (G_TYPE_CHECK_INSTANCE_TYPE ((object), FCITX_TYPE_CLIENT))
#define FCITX_CLIENT_CLASS(k) \
        (G_TYPE_CHECK_CLASS_CAST((k), FCITX_TYPE_CLIENT, FcitxClientClass))
#define FCITX_CLIENT_GET_CLASS(o) \
        (G_TYPE_INSTANCE_GET_CLASS ((o), FCITX_TYPE_CLIENT, FcitxClientClass))


typedef struct _FcitxClient        FcitxClient;
typedef struct _FcitxClientClass   FcitxClientClass;
typedef struct _FcitxClientPrivate FcitxClientPrivate;
typedef struct _FcitxPreeditItem   FcitxPreeditItem;

struct _FcitxClient {
    GObject parent_instance;
    /* instance member */
    FcitxClientPrivate* priv;
};

struct _FcitxClientClass {
    GObjectClass parent_class;
    /* signals */

    /*< private >*/
    /* padding */
};

struct _FcitxPreeditItem {
    gchar* string;
    gint32 type;
};

GType        fcitx_client_get_type(void) G_GNUC_CONST;
FcitxClient* fcitx_client_new();
FcitxClient* fcitx_client_new_with_connection(FcitxConnection* connection);
gboolean fcitx_client_is_valid(FcitxClient* self);
gboolean fcitx_client_process_key_sync(FcitxClient* self, guint32 keyval, guint32 keycode, guint32 state, gboolean isRelease, guint32 t);
void fcitx_client_process_key(FcitxClient* self,
                                    guint32 keyval, guint32 keycode,
                                    guint32 state, gboolean isRelease, guint32 t,
                                    gint timeout_msec,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data);
gboolean fcitx_client_process_key_finish(FcitxClient* self, GAsyncResult* res);
const guint8 *fcitx_client_get_uuid(FcitxClient* self);
void fcitx_client_focus_in(FcitxClient* self);
void fcitx_client_focus_out(FcitxClient* self);
void fcitx_client_set_cusor_rect(FcitxClient* self, gint x, gint y, gint w, gint h);
void fcitx_client_set_cursor_rect(FcitxClient* self, gint x, gint y, gint w, gint h);
void fcitx_client_set_surrounding_text(FcitxClient* self, gchar* text, guint cursor, guint anchor);
void fcitx_client_set_capability(FcitxClient* self, guint64 flags);

void fcitx_client_reset(FcitxClient* self);

G_END_DECLS

#endif //CLIENT_IM_H
