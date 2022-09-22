/*
 * SPDX-FileCopyrightText: 2021~2021 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */
#ifndef _GTK4_FCITXIMCONTEXTPRIVATE_H_
#define _GTK4_FCITXIMCONTEXTPRIVATE_H_

#include "fcitximcontext.h"
#include "gtk4inputwindow.h"

struct _FcitxIMContext {
    GtkIMContext parent;

    GtkWidget *client_widget;
    GdkRectangle area;
    FcitxGClient *client;
    GtkIMContext *slave;
    int has_focus;
    guint32 time;
    guint32 last_key_code;
    bool last_is_release;
    gboolean use_preedit;
    gboolean support_surrounding_text;
    gboolean is_inpreedit;
    gboolean is_wayland;
    char *preedit_string;
    char *surrounding_text;
    int cursor_pos;
    guint64 capability_from_toolkit;
    guint64 last_updated_capability;
    PangoAttrList *attrlist;
    int last_cursor_pos;
    int last_anchor_pos;
    struct xkb_compose_state *xkbComposeState;

    GHashTable *pending_events;
    GHashTable *handled_events;
    GQueue *handled_events_list;

    gboolean ignore_reset;

    fcitx::gtk::Gtk4InputWindow *candidate_window;
};

struct _FcitxIMContextClass {
    GtkIMContextClass parent;
    /* klass members */
};

#endif // _GTK4_FCITXIMCONTEXTPRIVATE_H_
