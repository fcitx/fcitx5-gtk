/*
 * SPDX-FileCopyrightText: 2012~2012 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef CLIENT_IM_H
#define CLIENT_IM_H

#include "fcitxgwatcher.h"
#include <glib-object.h>

G_BEGIN_DECLS

/*
 * Type macros
 */

/* define GOBJECT macros */
#define FCITX_G_TYPE_CLIENT (fcitx_g_client_get_type())

G_DECLARE_FINAL_TYPE(FcitxGClient, fcitx_g_client, FCITX_G, CLIENT, GObject)

typedef struct _FcitxGClientPrivate FcitxGClientPrivate;
typedef struct _FcitxGPreeditItem FcitxGPreeditItem;
typedef struct _FcitxGCandidateItem FcitxGCandidateItem;

struct _FcitxGPreeditItem {
    gchar *string;
    gint32 type;
};

struct _FcitxGCandidateItem {
    gchar *label;
    gchar *candidate;
};

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
void fcitx_g_client_set_program(FcitxGClient *self, const gchar *program);
void fcitx_g_client_set_cursor_rect(FcitxGClient *self, gint x, gint y, gint w,
                                    gint h);
void fcitx_g_client_set_cursor_rect_with_scale_factor(FcitxGClient *self,
                                                      gint x, gint y, gint w,
                                                      gint h, gdouble scale);
void fcitx_g_client_set_surrounding_text(FcitxGClient *self, gchar *text,
                                         guint cursor, guint anchor);
void fcitx_g_client_set_capability(FcitxGClient *self, guint64 flags);
void fcitx_g_client_prev_page(FcitxGClient *self);
void fcitx_g_client_next_page(FcitxGClient *self);
void fcitx_g_client_select_candidate(FcitxGClient *self, int index);

void fcitx_g_client_reset(FcitxGClient *self);

G_END_DECLS

#endif // CLIENT_IM_H
