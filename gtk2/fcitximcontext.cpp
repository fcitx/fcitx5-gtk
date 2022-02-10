/*
 * SPDX-FileCopyrightText: 2010~2020 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

/**
 * @file fcitximcontext.c
 *
 * This is a gtk im module for fcitx, using DBus as a protocol.
 *        This is compromise to gtk and firefox, users are being sucked by them
 *        again and again.
 */
#include "fcitxflags.h"

#include "config.h"
#include "fcitx-gclient/fcitxgclient.h"
#include "fcitx-gclient/fcitxgwatcher.h"
#include "fcitximcontext.h"
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <xkbcommon/xkbcommon-compose.h>

#if GTK_CHECK_VERSION(2, 24, 0)
#define NEW_GDK_WINDOW_GET_DISPLAY
#endif

static constexpr uint32_t HandledMask = (1 << 24);
static constexpr uint32_t IgnoredMask = (1 << 25);
static constexpr unsigned int MAX_CACHED_EVENTS = 30;

extern "C" {

static bool get_boolean_env(const char *name, bool defval) {
    const char *value = getenv(name);

    if (value == nullptr) {
        return defval;
    }

    if (g_strcmp0(value, "") == 0 || g_strcmp0(value, "0") == 0 ||
        g_strcmp0(value, "false") == 0 || g_strcmp0(value, "False") == 0 ||
        g_strcmp0(value, "FALSE") == 0) {
        return false;
    }

    return true;
}

struct _FcitxIMContext {
    GtkIMContext parent;

    GdkWindow *client_window;
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
    gchar *preedit_string;
    gchar *surrounding_text;
    int cursor_pos;
    guint64 capability_from_toolkit;
    guint64 last_updated_capability;
    PangoAttrList *attrlist;
    gint last_cursor_pos;
    gint last_anchor_pos;
    struct xkb_compose_state *xkbComposeState;

    GQueue gdk_events;
};

struct _FcitxIMContextClass {
    GtkIMContextClass parent;
    /* klass members */
};

/* functions prototype */
static void fcitx_im_context_class_init(FcitxIMContextClass *klass, gpointer);
static void fcitx_im_context_class_fini(FcitxIMContextClass *klass, gpointer);
static void fcitx_im_context_init(FcitxIMContext *im_context, gpointer);
static void fcitx_im_context_finalize(GObject *obj);
static void fcitx_im_context_set_client_window(GtkIMContext *context,
                                               GdkWindow *client_window);
static gboolean fcitx_im_context_filter_keypress(GtkIMContext *context,
                                                 GdkEventKey *key);
static void fcitx_im_context_reset(GtkIMContext *context);
static void fcitx_im_context_focus_in(GtkIMContext *context);
static void fcitx_im_context_focus_out(GtkIMContext *context);
static void fcitx_im_context_set_cursor_location(GtkIMContext *context,
                                                 GdkRectangle *area);
static void fcitx_im_context_set_use_preedit(GtkIMContext *context,
                                             gboolean use_preedit);
static void fcitx_im_context_set_surrounding(GtkIMContext *context,
                                             const gchar *text, gint len,
                                             gint cursor_index);
static void fcitx_im_context_get_preedit_string(GtkIMContext *context,
                                                gchar **str,
                                                PangoAttrList **attrs,
                                                gint *cursor_pos);

static gboolean _set_cursor_location_internal(FcitxIMContext *fcitxcontext);
static gboolean _defer_request_surrounding_text(FcitxIMContext *fcitxcontext);
static void _slave_commit_cb(GtkIMContext *slave, gchar *string,
                             FcitxIMContext *context);
static void _slave_preedit_changed_cb(GtkIMContext *slave,
                                      FcitxIMContext *context);
static void _slave_preedit_start_cb(GtkIMContext *slave,
                                    FcitxIMContext *context);
static void _slave_preedit_end_cb(GtkIMContext *slave, FcitxIMContext *context);
static gboolean _slave_retrieve_surrounding_cb(GtkIMContext *slave,
                                               FcitxIMContext *context);
static gboolean _slave_delete_surrounding_cb(GtkIMContext *slave,
                                             gint offset_from_cursor,
                                             guint nchars,
                                             FcitxIMContext *context);
static void _fcitx_im_context_commit_string_cb(FcitxGClient *client, char *str,
                                               void *user_data);
static void _fcitx_im_context_forward_key_cb(FcitxGClient *client, guint keyval,
                                             guint state, gint type,
                                             void *user_data);
static void
_fcitx_im_context_delete_surrounding_text_cb(FcitxGClient *client,
                                             gint offset_from_cursor,
                                             guint nchars, void *user_data);
static void _fcitx_im_context_connect_cb(FcitxGClient *client, void *user_data);
static void _fcitx_im_context_update_formatted_preedit_cb(FcitxGClient *im,
                                                          GPtrArray *array,
                                                          int cursor_pos,
                                                          void *user_data);
static void _fcitx_im_context_process_key_cb(GObject *source_object,
                                             GAsyncResult *res,
                                             gpointer user_data);
static void _fcitx_im_context_set_capability(FcitxIMContext *fcitxcontext,
                                             gboolean force);
static void _fcitx_im_context_push_event(FcitxIMContext *fcitxcontext,
                                         GdkEventKey *event);

static GdkEventKey *_create_gdk_event(FcitxIMContext *fcitxcontext,
                                      guint keyval, guint state,
                                      gboolean isRelease);

static gboolean _key_is_modifier(guint keyval);

static void _request_surrounding_text(FcitxIMContext **context);

static gint _key_snooper_cb(GtkWidget *widget, GdkEventKey *event,
                            gpointer user_data);

guint _update_auto_repeat_state(FcitxIMContext *context, GdkEventKey *event);

static GType _fcitx_type_im_context = 0;
static GtkIMContextClass *parent_class = NULL;

static guint _signal_commit_id = 0;
static guint _signal_preedit_changed_id = 0;
static guint _signal_preedit_start_id = 0;
static guint _signal_preedit_end_id = 0;
static guint _signal_delete_surrounding_id = 0;
static guint _signal_retrieve_surrounding_id = 0;
static gboolean _use_preedit = TRUE;
static gboolean _use_sync_mode = 0;

static GtkIMContext *_focus_im_context = NULL;
static const gchar *_no_snooper_apps = NO_SNOOPER_APPS;
static const gchar *_no_preedit_apps = NO_PREEDIT_APPS;
static const gchar *_sync_mode_apps = SYNC_MODE_APPS;
static gboolean _use_key_snooper = _ENABLE_SNOOPER;
static guint _key_snooper_id = 0;
static FcitxGWatcher *_watcher = NULL;
static struct xkb_context *xkbContext = NULL;
static struct xkb_compose_table *xkbComposeTable = NULL;

void fcitx_im_context_register_type(GTypeModule *type_module) {
    static const GTypeInfo fcitx_im_context_info = {
        sizeof(FcitxIMContextClass),
        (GBaseInitFunc)NULL,
        (GBaseFinalizeFunc)NULL,
        (GClassInitFunc)fcitx_im_context_class_init,
        (GClassFinalizeFunc)fcitx_im_context_class_fini,
        NULL, /* klass data */
        sizeof(FcitxIMContext),
        0,
        (GInstanceInitFunc)fcitx_im_context_init,
        0};

    if (_fcitx_type_im_context) {
        return;
    }
    if (type_module) {
        _fcitx_type_im_context = g_type_module_register_type(
            type_module, GTK_TYPE_IM_CONTEXT, "FcitxIMContext",
            &fcitx_im_context_info, (GTypeFlags)0);
    } else {
        _fcitx_type_im_context =
            g_type_register_static(GTK_TYPE_IM_CONTEXT, "FcitxIMContext",
                                   &fcitx_im_context_info, (GTypeFlags)0);
    }
}

GType fcitx_im_context_get_type(void) {
    if (_fcitx_type_im_context == 0) {
        fcitx_im_context_register_type(NULL);
    }

    g_assert(_fcitx_type_im_context != 0);
    return _fcitx_type_im_context;
}

FcitxIMContext *fcitx_im_context_new(void) {
    GObject *obj = (GObject *)g_object_new(FCITX_TYPE_IM_CONTEXT, NULL);
    return FCITX_IM_CONTEXT(obj);
}

static gboolean check_app_name(const gchar *pattern) {
    bool result = FALSE;
    const gchar *prgname = g_get_prgname();
    if (!prgname) {
        return FALSE;
    }
    gchar **p;
    gchar **apps = g_strsplit(pattern, ",", 0);
    for (p = apps; *p != NULL; p++) {
        if (g_regex_match_simple(*p, prgname, (GRegexCompileFlags)0,
                                 (GRegexMatchFlags)0)) {
            result = TRUE;
            break;
        }
    }
    g_strfreev(apps);
    return result;
}

///
static void fcitx_im_context_class_init(FcitxIMContextClass *klass, gpointer) {
    GtkIMContextClass *im_context_class = GTK_IM_CONTEXT_CLASS(klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    parent_class = (GtkIMContextClass *)g_type_class_peek_parent(klass);

    im_context_class->set_client_window = fcitx_im_context_set_client_window;
    im_context_class->filter_keypress = fcitx_im_context_filter_keypress;
    im_context_class->reset = fcitx_im_context_reset;
    im_context_class->get_preedit_string = fcitx_im_context_get_preedit_string;
    im_context_class->focus_in = fcitx_im_context_focus_in;
    im_context_class->focus_out = fcitx_im_context_focus_out;
    im_context_class->set_cursor_location =
        fcitx_im_context_set_cursor_location;
    im_context_class->set_use_preedit = fcitx_im_context_set_use_preedit;
    im_context_class->set_surrounding = fcitx_im_context_set_surrounding;
    gobject_class->finalize = fcitx_im_context_finalize;

    _signal_commit_id = g_signal_lookup("commit", G_TYPE_FROM_CLASS(klass));
    g_assert(_signal_commit_id != 0);

    _signal_preedit_changed_id =
        g_signal_lookup("preedit-changed", G_TYPE_FROM_CLASS(klass));
    g_assert(_signal_preedit_changed_id != 0);

    _signal_preedit_start_id =
        g_signal_lookup("preedit-start", G_TYPE_FROM_CLASS(klass));
    g_assert(_signal_preedit_start_id != 0);

    _signal_preedit_end_id =
        g_signal_lookup("preedit-end", G_TYPE_FROM_CLASS(klass));
    g_assert(_signal_preedit_end_id != 0);

    _signal_delete_surrounding_id =
        g_signal_lookup("delete-surrounding", G_TYPE_FROM_CLASS(klass));
    g_assert(_signal_delete_surrounding_id != 0);

    _signal_retrieve_surrounding_id =
        g_signal_lookup("retrieve-surrounding", G_TYPE_FROM_CLASS(klass));
    g_assert(_signal_retrieve_surrounding_id != 0);

    _use_key_snooper =
        !get_boolean_env("IBUS_DISABLE_SNOOPER", !(_ENABLE_SNOOPER)) &&
        !get_boolean_env("FCITX_DISABLE_SNOOPER", !(_ENABLE_SNOOPER));
    /* env IBUS_DISABLE_SNOOPER does not exist */
    if (_use_key_snooper) {
        /* disable snooper if app is in _no_snooper_apps */
        if (g_getenv("IBUS_NO_SNOOPER_APPS")) {
            _no_snooper_apps = g_getenv("IBUS_NO_SNOOPER_APPS");
        }
        if (g_getenv("FCITX_NO_SNOOPER_APPS")) {
            _no_snooper_apps = g_getenv("FCITX_NO_SNOOPER_APPS");
        }
        _use_key_snooper = !check_app_name(_no_snooper_apps);
    }

    // Check preedit blacklist
    if (g_getenv("FCITX_NO_PREEDIT_APPS")) {
        _no_preedit_apps = g_getenv("FCITX_NO_PREEDIT_APPS");
    }
    _use_preedit = !check_app_name(_no_preedit_apps);

    // Check sync mode
    if (g_getenv("FCITX_SYNC_MODE_APPS")) {
        _sync_mode_apps = g_getenv("FCITX_SYNC_MODE_APPS");
    }
    _use_sync_mode = check_app_name(_sync_mode_apps);
    if (g_getenv("IBUS_ENABLE_SYNC_MODE") ||
        g_getenv("FCITX_ENABLE_SYNC_MODE")) {
        /* make ibus fix benefits us */
        _use_sync_mode = get_boolean_env("IBUS_ENABLE_SYNC_MODE", FALSE) ||
                         get_boolean_env("FCITX_ENABLE_SYNC_MODE", FALSE);
    }

    /* always install snooper */
    if (_key_snooper_id == 0)
        _key_snooper_id = gtk_key_snooper_install(_key_snooper_cb, NULL);
}

static void fcitx_im_context_class_fini(FcitxIMContextClass *, gpointer) {
    if (_key_snooper_id != 0) {
        gtk_key_snooper_remove(_key_snooper_id);
        _key_snooper_id = 0;
    }
}

static void fcitx_im_context_init(FcitxIMContext *context, gpointer) {
    context->client = NULL;
    context->area.x = -1;
    context->area.y = -1;
    context->area.width = 0;
    context->area.height = 0;
    context->use_preedit = _use_preedit;
    context->cursor_pos = 0;
    context->last_anchor_pos = -1;
    context->last_cursor_pos = -1;
    context->preedit_string = NULL;
    context->attrlist = NULL;
    context->last_updated_capability =
        (guint64)fcitx::FcitxCapabilityFlag_SurroundingText;
    context->slave = gtk_im_context_simple_new();

    g_signal_connect(context->slave, "commit", G_CALLBACK(_slave_commit_cb),
                     context);
    g_signal_connect(context->slave, "preedit-start",
                     G_CALLBACK(_slave_preedit_start_cb), context);
    g_signal_connect(context->slave, "preedit-end",
                     G_CALLBACK(_slave_preedit_end_cb), context);
    g_signal_connect(context->slave, "preedit-changed",
                     G_CALLBACK(_slave_preedit_changed_cb), context);
    g_signal_connect(context->slave, "retrieve-surrounding",
                     G_CALLBACK(_slave_retrieve_surrounding_cb), context);
    g_signal_connect(context->slave, "delete-surrounding",
                     G_CALLBACK(_slave_delete_surrounding_cb), context);

    context->time = GDK_CURRENT_TIME;

    static gsize has_info = 0;
    if (g_once_init_enter(&has_info)) {
        _watcher = fcitx_g_watcher_new();
        fcitx_g_watcher_set_watch_portal(_watcher, TRUE);
        fcitx_g_watcher_watch(_watcher);
        g_object_ref_sink(_watcher);

        xkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

        if (xkbContext) {
            xkb_context_set_log_level(xkbContext, XKB_LOG_LEVEL_CRITICAL);
        }

        const char *locale = getenv("LC_ALL");
        if (!locale)
            locale = getenv("LC_CTYPE");
        if (!locale)
            locale = getenv("LANG");
        if (!locale)
            locale = "C";

        xkbComposeTable =
            xkbContext ? xkb_compose_table_new_from_locale(
                             xkbContext, locale, XKB_COMPOSE_COMPILE_NO_FLAGS)
                       : NULL;

        g_once_init_leave(&has_info, 1);
    }

    context->client = fcitx_g_client_new_with_watcher(_watcher);
    fcitx_g_client_set_program(context->client, g_get_prgname());
    fcitx_g_client_set_display(context->client, "x11:");
    g_signal_connect(context->client, "connected",
                     G_CALLBACK(_fcitx_im_context_connect_cb), context);
    g_signal_connect(context->client, "forward-key",
                     G_CALLBACK(_fcitx_im_context_forward_key_cb), context);
    g_signal_connect(context->client, "commit-string",
                     G_CALLBACK(_fcitx_im_context_commit_string_cb), context);
    g_signal_connect(context->client, "delete-surrounding-text",
                     G_CALLBACK(_fcitx_im_context_delete_surrounding_text_cb),
                     context);
    g_signal_connect(context->client, "update-formatted-preedit",
                     G_CALLBACK(_fcitx_im_context_update_formatted_preedit_cb),
                     context);

    context->xkbComposeState =
        xkbComposeTable
            ? xkb_compose_state_new(xkbComposeTable, XKB_COMPOSE_STATE_NO_FLAGS)
            : NULL;

    g_queue_init(&context->gdk_events);
}

static void fcitx_im_context_finalize(GObject *obj) {
    FcitxIMContext *context = FCITX_IM_CONTEXT(obj);

    fcitx_im_context_set_client_window(GTK_IM_CONTEXT(context), NULL);

#ifndef g_signal_handlers_disconnect_by_data
#define g_signal_handlers_disconnect_by_data(instance, data)                   \
    g_signal_handlers_disconnect_matched((instance), G_SIGNAL_MATCH_DATA, 0,   \
                                         0, NULL, NULL, (data))
#endif

    g_clear_pointer(&context->xkbComposeState, xkb_compose_state_unref);
    if (context->client) {
        g_signal_handlers_disconnect_by_data(context->client, context);
    }
    g_clear_object(&context->client);

    g_clear_pointer(&context->preedit_string, g_free);
    g_clear_pointer(&context->surrounding_text, g_free);
    g_clear_pointer(&context->attrlist, pango_attr_list_unref);
    g_queue_clear_full(&context->gdk_events, (GDestroyNotify)gdk_event_free);

    G_OBJECT_CLASS(parent_class)->finalize(obj);
}

///
static void fcitx_im_context_set_client_window(GtkIMContext *context,
                                               GdkWindow *client_window) {
    FcitxIMContext *fcitxcontext = FCITX_IM_CONTEXT(context);
    if (!client_window)
        return;

    g_clear_object(&fcitxcontext->client_window);
    fcitxcontext->client_window = GDK_WINDOW(g_object_ref(client_window));
}

static gboolean
fcitx_im_context_filter_keypress_fallback(FcitxIMContext *context,
                                          GdkEventKey *event) {
    if (!context->xkbComposeState || event->type == GDK_KEY_RELEASE) {
        return gtk_im_context_filter_keypress(context->slave, event);
    }

    struct xkb_compose_state *xkbComposeState = context->xkbComposeState;

    enum xkb_compose_feed_result result =
        xkb_compose_state_feed(xkbComposeState, event->keyval);
    if (result == XKB_COMPOSE_FEED_IGNORED) {
        return gtk_im_context_filter_keypress(context->slave, event);
    }

    enum xkb_compose_status status =
        xkb_compose_state_get_status(xkbComposeState);
    if (status == XKB_COMPOSE_NOTHING) {
        return gtk_im_context_filter_keypress(context->slave, event);
    } else if (status == XKB_COMPOSE_COMPOSED) {
        char buffer[] = {'\0', '\0', '\0', '\0', '\0', '\0', '\0'};
        int length =
            xkb_compose_state_get_utf8(xkbComposeState, buffer, sizeof(buffer));
        xkb_compose_state_reset(xkbComposeState);
        if (length != 0) {
            g_signal_emit(context, _signal_commit_id, 0, buffer);
        }

    } else if (status == XKB_COMPOSE_CANCELLED) {
        xkb_compose_state_reset(xkbComposeState);
    }

    return TRUE;
}

///
static gboolean fcitx_im_context_filter_keypress(GtkIMContext *context,
                                                 GdkEventKey *event) {
    FcitxIMContext *fcitxcontext = FCITX_IM_CONTEXT(context);

    /* check this first, since we use key snooper, most key will be handled. */
    if (fcitx_g_client_is_valid(fcitxcontext->client)) {
        /* XXX it is a workaround for some applications do not set client
         * window. */
        if (fcitxcontext->client_window == NULL && event->window != NULL) {
            gtk_im_context_set_client_window((GtkIMContext *)fcitxcontext,
                                             event->window);

            /* set_cursor_location_internal() will get origin from X server,
             * it blocks UI. So delay it to idle callback. */
            gdk_threads_add_idle_full(
                G_PRIORITY_DEFAULT_IDLE,
                (GSourceFunc)_set_cursor_location_internal,
                g_object_ref(fcitxcontext), (GDestroyNotify)g_object_unref);
        }
    }

    if (event->state & (guint64)HandledMask) {
        return TRUE;
    }

    if (event->state & (guint64)IgnoredMask) {
        return fcitx_im_context_filter_keypress_fallback(fcitxcontext, event);
    }

    if (fcitx_g_client_is_valid(fcitxcontext->client) &&
        fcitxcontext->has_focus) {
        _request_surrounding_text(&fcitxcontext);
        if (G_UNLIKELY(!fcitxcontext))
            return FALSE;

        auto state = _update_auto_repeat_state(fcitxcontext, event);

        _fcitx_im_context_push_event(fcitxcontext, event);
        if (_use_sync_mode) {
            gboolean ret = fcitx_g_client_process_key_sync(
                fcitxcontext->client, event->keyval, event->hardware_keycode,
                state, (event->type != GDK_KEY_PRESS), event->time);
            if (ret) {
                event->state |= (guint32)HandledMask;
                return TRUE;
            } else {
                event->state |= (guint32)IgnoredMask;
                return fcitx_im_context_filter_keypress_fallback(fcitxcontext,
                                                                 event);
            }
        } else {
            fcitx_g_client_process_key(
                fcitxcontext->client, event->keyval, event->hardware_keycode,
                state, (event->type != GDK_KEY_PRESS), event->time, -1, NULL,
                _fcitx_im_context_process_key_cb,
                gdk_event_copy((GdkEvent *)event));
            event->state |= (guint32)HandledMask;
            return TRUE;
        }
    } else {
        return fcitx_im_context_filter_keypress_fallback(fcitxcontext, event);
    }
    return FALSE;
}

static void _fcitx_im_context_process_key_cb(GObject *source_object,
                                             GAsyncResult *res,
                                             gpointer user_data) {
    GdkEventKey *event = (GdkEventKey *)user_data;
    gboolean ret =
        fcitx_g_client_process_key_finish(FCITX_G_CLIENT(source_object), res);
    if (!ret) {
        event->state |= (guint32)IgnoredMask;
        gdk_event_put((GdkEvent *)event);
    }
    gdk_event_free((GdkEvent *)event);
}

static void _fcitx_im_context_update_preedit(FcitxIMContext *context,
                                             GPtrArray *array, int cursor_pos) {
    context->attrlist = pango_attr_list_new();

    GString *gstr = g_string_new(NULL);

    unsigned int i = 0;
    for (i = 0; i < array->len; i++) {
        size_t bytelen = strlen(gstr->str);
        FcitxGPreeditItem *preedit =
            (FcitxGPreeditItem *)g_ptr_array_index(array, i);
        const gchar *s = preedit->string;
        gint type = preedit->type;

        PangoAttribute *pango_attr = NULL;
        if ((type & (guint32)fcitx::FcitxTextFormatFlag_Underline)) {
            pango_attr = pango_attr_underline_new(PANGO_UNDERLINE_SINGLE);
            pango_attr->start_index = bytelen;
            pango_attr->end_index = bytelen + strlen(s);
            pango_attr_list_insert(context->attrlist, pango_attr);
        }
        if ((type & (guint32)fcitx::FcitxTextFormatFlag_Strike)) {
            pango_attr = pango_attr_strikethrough_new(true);
            pango_attr->start_index = bytelen;
            pango_attr->end_index = bytelen + strlen(s);
            pango_attr_list_insert(context->attrlist, pango_attr);
        }
        if ((type & (guint32)fcitx::FcitxTextFormatFlag_Bold)) {
            pango_attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
            pango_attr->start_index = bytelen;
            pango_attr->end_index = bytelen + strlen(s);
            pango_attr_list_insert(context->attrlist, pango_attr);
        }
        if ((type & (guint32)fcitx::FcitxTextFormatFlag_Italic)) {
            pango_attr = pango_attr_style_new(PANGO_STYLE_ITALIC);
            pango_attr->start_index = bytelen;
            pango_attr->end_index = bytelen + strlen(s);
            pango_attr_list_insert(context->attrlist, pango_attr);
        }

        if (type & (guint32)fcitx::FcitxTextFormatFlag_HighLight) {
            gboolean hasColor = false;
            GdkColor fg;
            GdkColor bg;
            memset(&fg, 0, sizeof(GdkColor));
            memset(&bg, 0, sizeof(GdkColor));

            if (context->client_window) {
                GtkWidget *widget;
                gdk_window_get_user_data(context->client_window,
                                         (gpointer *)&widget);
                if (GTK_IS_WIDGET(widget)) {
                    hasColor = true;
                    GtkStyle *style = gtk_widget_get_style(widget);
                    fg = style->text[GTK_STATE_SELECTED];
                    bg = style->base[GTK_STATE_SELECTED];
                }
            }

            if (!hasColor) {
                fg.red = 0xffff;
                fg.green = 0xffff;
                fg.blue = 0xffff;
                bg.red = 0x43ff;
                bg.green = 0xacff;
                bg.blue = 0xe8ff;
            }

            pango_attr = pango_attr_foreground_new(fg.red, fg.green, fg.blue);
            pango_attr->start_index = bytelen;
            pango_attr->end_index = bytelen + strlen(s);
            pango_attr_list_insert(context->attrlist, pango_attr);
            pango_attr = pango_attr_background_new(bg.red, bg.green, bg.blue);
            pango_attr->start_index = bytelen;
            pango_attr->end_index = bytelen + strlen(s);
            pango_attr_list_insert(context->attrlist, pango_attr);
        }
        gstr = g_string_append(gstr, s);
    }

    gchar *str = g_string_free(gstr, FALSE);

    context->preedit_string = str;
    context->cursor_pos = g_utf8_pointer_to_offset(str, str + cursor_pos);

    if (context->preedit_string != NULL && context->preedit_string[0] == 0) {
        g_clear_pointer(&context->preedit_string, g_free);
    }
}

static void _fcitx_im_context_update_formatted_preedit_cb(FcitxGClient *,
                                                          GPtrArray *array,
                                                          int cursor_pos,
                                                          void *user_data) {
    FcitxIMContext *context = FCITX_IM_CONTEXT(user_data);

    gboolean visible = false;

    if (cursor_pos < 0) {
        cursor_pos = 0;
    }

    if (context->preedit_string != NULL) {
        if (strlen(context->preedit_string) != 0) {
            visible = true;
        }

        g_clear_pointer(&context->preedit_string, g_free);
    }
    g_clear_pointer(&context->attrlist, pango_attr_list_unref);

    if (context->use_preedit) {
        _fcitx_im_context_update_preedit(context, array, cursor_pos);
    }

    gboolean new_visible = context->preedit_string != NULL;

    gboolean flag = new_visible != visible;

    if (new_visible) {
        if (flag) {
            /* invisible => visible */
            g_signal_emit(context, _signal_preedit_start_id, 0);
        }
        g_signal_emit(context, _signal_preedit_changed_id, 0);
    } else {
        if (flag) {
            /* visible => invisible */
            g_signal_emit(context, _signal_preedit_changed_id, 0);
            g_signal_emit(context, _signal_preedit_end_id, 0);
        } else {
            /* still invisible */
            /* do nothing */
        }
    }
}

///
static void fcitx_im_context_focus_in(GtkIMContext *context) {
    FcitxIMContext *fcitxcontext = FCITX_IM_CONTEXT(context);

    if (fcitxcontext->has_focus) {
        return;
    }

    _fcitx_im_context_set_capability(fcitxcontext, FALSE);

    fcitxcontext->has_focus = true;

/*
 * Do not call gtk_im_context_focus_out() here.
 * This might workaround some chrome issue
 */
#if 0
    if (_focus_im_context != NULL) {
        g_assert (_focus_im_context != context);
        gtk_im_context_focus_out (_focus_im_context);
        g_assert (_focus_im_context == NULL);
    }
#endif

    if (fcitx_g_client_is_valid(fcitxcontext->client)) {
        fcitx_g_client_focus_in(fcitxcontext->client);
    }

    gtk_im_context_focus_in(fcitxcontext->slave);

    /* set_cursor_location_internal() will get origin from X server,
     * it blocks UI. So delay it to idle callback. */
    gdk_threads_add_idle_full(
        G_PRIORITY_DEFAULT_IDLE, (GSourceFunc)_set_cursor_location_internal,
        g_object_ref(fcitxcontext), (GDestroyNotify)g_object_unref);

    /* _request_surrounding_text may trigger freeze in Libreoffice. After
     * focus in, the request is not as urgent as key event. Delay it to main
     * idle callback. */
    gdk_threads_add_idle_full(
        G_PRIORITY_DEFAULT_IDLE, (GSourceFunc)_defer_request_surrounding_text,
        g_object_ref(fcitxcontext), (GDestroyNotify)g_object_unref);

    g_object_add_weak_pointer((GObject *)context,
                              (gpointer *)&_focus_im_context);
    _focus_im_context = context;

    return;
}

static void fcitx_im_context_focus_out(GtkIMContext *context) {
    FcitxIMContext *fcitxcontext = FCITX_IM_CONTEXT(context);

    if (!fcitxcontext->has_focus) {
        return;
    }

    g_object_remove_weak_pointer((GObject *)context,
                                 (gpointer *)&_focus_im_context);
    _focus_im_context = NULL;

    fcitxcontext->has_focus = false;
    fcitxcontext->last_key_code = 0;
    fcitxcontext->last_is_release = false;

    if (fcitx_g_client_is_valid(fcitxcontext->client)) {
        fcitx_g_client_focus_out(fcitxcontext->client);
    }

    fcitxcontext->cursor_pos = 0;
    if (fcitxcontext->preedit_string != NULL) {
        g_clear_pointer(&fcitxcontext->preedit_string, g_free);
        g_signal_emit(fcitxcontext, _signal_preedit_changed_id, 0);
        g_signal_emit(fcitxcontext, _signal_preedit_end_id, 0);
    }

    gtk_im_context_focus_out(fcitxcontext->slave);

    return;
}

///
static void fcitx_im_context_set_cursor_location(GtkIMContext *context,
                                                 GdkRectangle *area) {
    FcitxIMContext *fcitxcontext = FCITX_IM_CONTEXT(context);

    if (fcitxcontext->area.x == area->x && fcitxcontext->area.y == area->y &&
        fcitxcontext->area.width == area->width &&
        fcitxcontext->area.height == area->height) {
        return;
    }
    fcitxcontext->area = *area;

    if (fcitx_g_client_is_valid(fcitxcontext->client)) {
        _set_cursor_location_internal(fcitxcontext);
    }
    gtk_im_context_set_cursor_location(fcitxcontext->slave, area);

    return;
}

static gboolean _set_cursor_location_internal(FcitxIMContext *fcitxcontext) {
    GdkRectangle area;

    if (fcitxcontext->client_window == NULL ||
        !fcitx_g_client_is_valid(fcitxcontext->client)) {
        return FALSE;
    }

    area = fcitxcontext->area;
    {
        if (area.x == -1 && area.y == -1 && area.width == 0 &&
            area.height == 0) {
            gint w, h;
            gdk_drawable_get_size(fcitxcontext->client_window, &w, &h);
            area.y += h;
            area.x = 0;
        }

#if GTK_CHECK_VERSION(2, 18, 0)
        gdk_window_get_root_coords(fcitxcontext->client_window, area.x, area.y,
                                   &area.x, &area.y);
#else
        {
            int rootx, rooty;
            gdk_window_get_origin(fcitxcontext->client_window, &rootx, &rooty);
            area.x += rootx;
            area.y += rooty;
        }
#endif
    }
    int scale = 1;
    area.x *= scale;
    area.y *= scale;
    area.width *= scale;
    area.height *= scale;

    // We don't really need this check, but we can keep certain level of
    // compatibility for fcitx 4.
    fcitx_g_client_set_cursor_rect(fcitxcontext->client, area.x, area.y,
                                   area.width, area.height);
    return FALSE;
}

static gboolean _defer_request_surrounding_text(FcitxIMContext *fcitxcontext) {
    _request_surrounding_text(&fcitxcontext);
    return FALSE;
}

///
static void fcitx_im_context_set_use_preedit(GtkIMContext *context,
                                             gboolean use_preedit) {
    FcitxIMContext *fcitxcontext = FCITX_IM_CONTEXT(context);

    fcitxcontext->use_preedit = _use_preedit && use_preedit;
    _fcitx_im_context_set_capability(fcitxcontext, FALSE);

    gtk_im_context_set_use_preedit(fcitxcontext->slave, use_preedit);
}

static guint get_selection_anchor_point(FcitxIMContext *fcitxcontext,
                                        guint cursor_pos,
                                        guint surrounding_text_len) {
    GtkWidget *widget;
    if (fcitxcontext->client_window == NULL) {
        return cursor_pos;
    }
    gdk_window_get_user_data(fcitxcontext->client_window, (gpointer *)&widget);

    if (!GTK_IS_TEXT_VIEW(widget)) {
        return cursor_pos;
    }

    GtkTextView *text_view = GTK_TEXT_VIEW(widget);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(text_view);

    if (!gtk_text_buffer_get_has_selection(buffer)) {
        return cursor_pos;
    }

    GtkTextIter start_iter, end_iter, cursor_iter;
    if (!gtk_text_buffer_get_selection_bounds(buffer, &start_iter, &end_iter)) {
        return cursor_pos;
    }

    gtk_text_buffer_get_iter_at_mark(buffer, &cursor_iter,
                                     gtk_text_buffer_get_insert(buffer));

    guint start_index = gtk_text_iter_get_offset(&start_iter);
    guint end_index = gtk_text_iter_get_offset(&end_iter);
    guint cursor_index = gtk_text_iter_get_offset(&cursor_iter);

    guint anchor;

    if (start_index == cursor_index) {
        anchor = end_index;
    } else if (end_index == cursor_index) {
        anchor = start_index;
    } else {
        return cursor_pos;
    }

    // Change absolute index to relative position.
    guint relative_origin = cursor_index - cursor_pos;

    if (anchor < relative_origin) {
        return cursor_pos;
    }
    anchor -= relative_origin;

    if (anchor > surrounding_text_len) {
        return cursor_pos;
    }

    return anchor;
}

static void fcitx_im_context_set_surrounding(GtkIMContext *context,
                                             const gchar *text, gint l,
                                             gint cursor_index) {
    g_return_if_fail(context != NULL);
    g_return_if_fail(FCITX_IS_IM_CONTEXT(context));
    g_return_if_fail(text != NULL);

    gint len = l;
    if (len < 0) {
        len = strlen(text);
    }

    g_return_if_fail(0 <= cursor_index && cursor_index <= len);

    FcitxIMContext *fcitxcontext = FCITX_IM_CONTEXT(context);

    if (fcitx_g_client_is_valid(fcitxcontext->client) &&
        !(fcitxcontext->last_updated_capability &
          (guint64)fcitx::FcitxCapabilityFlag_Password)) {
        gint cursor_pos;
        guint utf8_len;
        gchar *p;

        p = g_strndup(text, len);
        cursor_pos = g_utf8_strlen(p, cursor_index);
        utf8_len = g_utf8_strlen(p, len);

        gint anchor_pos =
            get_selection_anchor_point(fcitxcontext, cursor_pos, utf8_len);
        if (g_strcmp0(fcitxcontext->surrounding_text, p) == 0) {
            g_clear_pointer(&p, g_free);
        } else {
            g_free(fcitxcontext->surrounding_text);
            fcitxcontext->surrounding_text = p;
        }

        if (p || fcitxcontext->last_cursor_pos != cursor_pos ||
            fcitxcontext->last_anchor_pos != anchor_pos) {
            fcitxcontext->last_cursor_pos = cursor_pos;
            fcitxcontext->last_anchor_pos = anchor_pos;
            fcitx_g_client_set_surrounding_text(fcitxcontext->client, p,
                                                cursor_pos, anchor_pos);
        }
    }
    gtk_im_context_set_surrounding(fcitxcontext->slave, text, l, cursor_index);
}

void _fcitx_im_context_set_capability(FcitxIMContext *fcitxcontext,
                                      gboolean force) {
    if (fcitx_g_client_is_valid(fcitxcontext->client)) {
        guint64 flags = fcitxcontext->capability_from_toolkit;
        // toolkit hint always not have preedit / surrounding hint
        // no need to check them
        if (fcitxcontext->use_preedit) {
            flags |= (guint64)fcitx::FcitxCapabilityFlag_Preedit |
                     (guint64)fcitx::FcitxCapabilityFlag_FormattedPreedit;
        }
        if (fcitxcontext->support_surrounding_text) {
            flags |= (guint64)fcitx::FcitxCapabilityFlag_SurroundingText;
        }
        flags |= (guint64)fcitx::FcitxCapabilityFlag_KeyEventOrderFix;
        flags |= (guint64)fcitx::FcitxCapabilityFlag_ReportKeyRepeat;

        // always run this code against all gtk version
        // seems visibility != PASSWORD hint
        if (fcitxcontext->client_window != NULL) {
            GtkWidget *widget;
            gdk_window_get_user_data(fcitxcontext->client_window,
                                     (gpointer *)&widget);
            if (GTK_IS_ENTRY(widget) &&
                !gtk_entry_get_visibility(GTK_ENTRY(widget))) {
                flags |= (guint64)fcitx::FcitxCapabilityFlag_Password;
            }
        }

        gboolean update = FALSE;
        if (G_UNLIKELY(fcitxcontext->last_updated_capability != flags)) {
            fcitxcontext->last_updated_capability = flags;
            update = TRUE;
        }
        if (G_UNLIKELY(update || force))
            fcitx_g_client_set_capability(
                fcitxcontext->client, fcitxcontext->last_updated_capability);
    }
}

static void _fcitx_im_context_push_event(FcitxIMContext *fcitxcontext,
                                         GdkEventKey *event) {
    // Keep a copy of latest event.
    g_queue_push_head(&fcitxcontext->gdk_events,
                      gdk_event_copy((GdkEvent *)event));
    while (g_queue_get_length(&fcitxcontext->gdk_events) > MAX_CACHED_EVENTS) {
        gdk_event_free(static_cast<GdkEvent *>(
            g_queue_pop_tail(&fcitxcontext->gdk_events)));
    }
}

///
static void fcitx_im_context_reset(GtkIMContext *context) {
    FcitxIMContext *fcitxcontext = FCITX_IM_CONTEXT(context);

    if (fcitx_g_client_is_valid(fcitxcontext->client)) {
        fcitx_g_client_reset(fcitxcontext->client);
    }

    if (fcitxcontext->xkbComposeState) {
        xkb_compose_state_reset(fcitxcontext->xkbComposeState);
    }

    gtk_im_context_reset(fcitxcontext->slave);
}

static void fcitx_im_context_get_preedit_string(GtkIMContext *context,
                                                gchar **str,
                                                PangoAttrList **attrs,
                                                gint *cursor_pos) {
    FcitxIMContext *fcitxcontext = FCITX_IM_CONTEXT(context);

    if (fcitx_g_client_is_valid(fcitxcontext->client)) {
        if (str) {
            *str = g_strdup(fcitxcontext->preedit_string
                                ? fcitxcontext->preedit_string
                                : "");
        }
        if (attrs) {
            if (fcitxcontext->attrlist == NULL) {
                *attrs = pango_attr_list_new();

                if (str) {
                    PangoAttribute *pango_attr;
                    pango_attr =
                        pango_attr_underline_new(PANGO_UNDERLINE_SINGLE);
                    pango_attr->start_index = 0;
                    pango_attr->end_index = strlen(*str);
                    pango_attr_list_insert(*attrs, pango_attr);
                }
            } else {
                *attrs = pango_attr_list_ref(fcitxcontext->attrlist);
            }
        }
        if (cursor_pos)
            *cursor_pos = fcitxcontext->cursor_pos;

    } else {
        gtk_im_context_get_preedit_string(fcitxcontext->slave, str, attrs,
                                          cursor_pos);
    }
    return;
}

/* Callback functions for slave context */
static void _slave_commit_cb(GtkIMContext *, gchar *string,
                             FcitxIMContext *context) {
    g_signal_emit(context, _signal_commit_id, 0, string);
}
static void _slave_preedit_changed_cb(GtkIMContext *, FcitxIMContext *context) {
    if (context->client) {
        return;
    }

    g_signal_emit(context, _signal_preedit_changed_id, 0);
}
static void _slave_preedit_start_cb(GtkIMContext *, FcitxIMContext *context) {
    if (context->client) {
        return;
    }

    g_signal_emit(context, _signal_preedit_start_id, 0);
}

static void _slave_preedit_end_cb(GtkIMContext *, FcitxIMContext *context) {
    if (context->client) {
        return;
    }
    g_signal_emit(context, _signal_preedit_end_id, 0);
}

static gboolean _slave_retrieve_surrounding_cb(GtkIMContext *,
                                               FcitxIMContext *context) {
    gboolean return_value;

    if (context->client) {
        return FALSE;
    }
    g_signal_emit(context, _signal_retrieve_surrounding_id, 0, &return_value);
    return return_value;
}

static gboolean _slave_delete_surrounding_cb(GtkIMContext *,
                                             gint offset_from_cursor,
                                             guint nchars,
                                             FcitxIMContext *context) {
    gboolean return_value;

    if (context->client) {
        return FALSE;
    }
    g_signal_emit(context, _signal_delete_surrounding_id, 0, offset_from_cursor,
                  nchars, &return_value);
    return return_value;
}

void _fcitx_im_context_commit_string_cb(FcitxGClient *, char *str,
                                        void *user_data) {
    FcitxIMContext *context = FCITX_IM_CONTEXT(user_data);
    g_signal_emit(context, _signal_commit_id, 0, str);

    // Better request surrounding after commit.
    gdk_threads_add_idle_full(
        G_PRIORITY_DEFAULT_IDLE, (GSourceFunc)_defer_request_surrounding_text,
        g_object_ref(context), (GDestroyNotify)g_object_unref);
}

void _fcitx_im_context_forward_key_cb(FcitxGClient *, guint keyval, guint state,
                                      gboolean isRelease, void *user_data) {
    FcitxIMContext *context = FCITX_IM_CONTEXT(user_data);
    GdkEventKey *event = _create_gdk_event(context, keyval, state, isRelease);
    event->state |= (guint32)IgnoredMask;
    gdk_event_put((GdkEvent *)event);
    gdk_event_free((GdkEvent *)event);
}

static void _fcitx_im_context_delete_surrounding_text_cb(
    FcitxGClient *, gint offset_from_cursor, guint nchars, void *user_data) {
    FcitxIMContext *context = FCITX_IM_CONTEXT(user_data);
    gboolean return_value;
    g_signal_emit(context, _signal_delete_surrounding_id, 0, offset_from_cursor,
                  nchars, &return_value);
}

/* Copy from gdk */
static GdkEventKey *_create_gdk_event(FcitxIMContext *fcitxcontext,
                                      guint keyval, guint state,
                                      gboolean isRelease) {
    if (fcitxcontext) {
        struct FindKey {
            guint keyval;
            guint state;
            gboolean isRelease;
        } data = {keyval, state, isRelease};
        // Unset auto repeat state.
        data.state &= (~(1u << 31));
        auto *result = g_queue_find_custom(
            &fcitxcontext->gdk_events, &data,
            (GCompareFunc)(+[](GdkEventKey *event, FindKey *data) {
                if (event->keyval == data->keyval &&
                    event->state == data->state &&
                    data->isRelease == (event->type == GDK_KEY_RELEASE)) {
                    return 0;
                }
                return 1;
            }));
        if (result) {
            return reinterpret_cast<GdkEventKey *>(
                gdk_event_copy(static_cast<const GdkEvent *>(result->data)));
        }
    }

    gunichar c = 0;
    gchar buf[8];

    GdkEventKey *event = (GdkEventKey *)gdk_event_new(
        isRelease ? GDK_KEY_RELEASE : GDK_KEY_PRESS);

    if (fcitxcontext && fcitxcontext->client_window) {
        event->window = (GdkWindow *)g_object_ref(fcitxcontext->client_window);
    }

    /* The time is copied the latest value from the previous
     * GdkKeyEvent in filter_keypress().
     *
     * We understand the best way would be to pass the all time value
     * to Fcitx functions process_key_event() and Fcitx DBus functions
     * ProcessKeyEvent() in IM clients and IM engines so that the
     * _create_gdk_event() could get the correct time values.
     * However it would causes to change many functions and the time value
     * would not provide the useful meanings for each Fcitx engines but just
     * pass the original value to ForwardKeyEvent().
     * We use the saved value at the moment.
     *
     * Another idea might be to have the time implementation in X servers
     * but some Xorg uses clock_gettime() and others use gettimeofday()
     * and the values would be different in each implementation and
     * locale/remote X server. So probably that idea would not work. */
    if (fcitxcontext) {
        event->time = fcitxcontext->time;
    } else {
        event->time = GDK_CURRENT_TIME;
    }

    event->send_event = FALSE;
    event->state = state;
    event->keyval = keyval;
    event->string = NULL;
    event->length = 0;
    event->hardware_keycode = 0;
    if (event->window) {
#ifndef NEW_GDK_WINDOW_GET_DISPLAY
        GdkDisplay *display = gdk_display_get_default();
#else
        GdkDisplay *display = gdk_window_get_display(event->window);
#endif
        GdkKeymap *keymap = gdk_keymap_get_for_display(display);
        GdkKeymapKey *keys;
        gint n_keys = 0;

        if (gdk_keymap_get_entries_for_keyval(keymap, keyval, &keys, &n_keys)) {
            if (n_keys)
                event->hardware_keycode = keys[0].keycode;
            g_free(keys);
        }
    }

    event->group = 0;
    event->is_modifier = _key_is_modifier(keyval);

    if (keyval != GDK_VoidSymbol)
        c = gdk_keyval_to_unicode(keyval);

    if (c) {
        gsize bytes_written;
        gint len;

        /* Apply the control key - Taken from Xlib
         */
        if (event->state & GDK_CONTROL_MASK) {
            if ((c >= '@' && c < '\177') || c == ' ')
                c &= 0x1F;
            else if (c == '2') {
#if GLIB_CHECK_VERSION(2, 68, 0)
                event->string = (gchar *)g_memdup2("\0\0", 2);
#else
                event->string = (gchar *)g_memdup("\0\0", 2);
#endif
                event->length = 1;
                buf[0] = '\0';
                goto out;
            } else if (c >= '3' && c <= '7')
                c -= ('3' - '\033');
            else if (c == '8')
                c = '\177';
            else if (c == '/')
                c = '_' & 0x1F;
        }

        len = g_unichar_to_utf8(c, buf);
        buf[len] = '\0';

        event->string =
            g_locale_from_utf8(buf, len, NULL, &bytes_written, NULL);
        if (event->string)
            event->length = bytes_written;
    } else if (keyval == GDK_Escape) {
        event->length = 1;
        event->string = g_strdup("\033");
    } else if (keyval == GDK_Return || keyval == GDK_KP_Enter) {
        event->length = 1;
        event->string = g_strdup("\r");
    }

    if (!event->string) {
        event->length = 0;
        event->string = g_strdup("");
    }
out:
    return event;
}

static gboolean _key_is_modifier(guint keyval) {
    /* See gdkkeys-x11.c:_gdk_keymap_key_is_modifier() for how this
     * really should be implemented */

    switch (keyval) {
    case GDK_Shift_L:
    case GDK_Shift_R:
    case GDK_Control_L:
    case GDK_Control_R:
    case GDK_Caps_Lock:
    case GDK_Shift_Lock:
    case GDK_Meta_L:
    case GDK_Meta_R:
    case GDK_Alt_L:
    case GDK_Alt_R:
    case GDK_Super_L:
    case GDK_Super_R:
    case GDK_Hyper_L:
    case GDK_Hyper_R:
    case GDK_ISO_Lock:
    case GDK_ISO_Level2_Latch:
    case GDK_ISO_Level3_Shift:
    case GDK_ISO_Level3_Latch:
    case GDK_ISO_Level3_Lock:
    case GDK_ISO_Group_Shift:
    case GDK_ISO_Group_Latch:
    case GDK_ISO_Group_Lock:
        return TRUE;
    default:
        return FALSE;
    }
}

void send_uuid_to_x11(Display *xdisplay, const guint8 *uuid) {
    Atom atom = XInternAtom(xdisplay, "_FCITX_SERVER", False);
    if (!atom) {
        return;
    }
    Window window = XGetSelectionOwner(xdisplay, atom);
    if (!window) {
        return;
    }
    XEvent ev;

    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.window = window;
    ev.xclient.message_type = atom;
    ev.xclient.format = 8;
    memcpy(ev.xclient.data.b, uuid, 16);

    XSendEvent(xdisplay, window, False, NoEventMask, &ev);
    XSync(xdisplay, False);
}

void _fcitx_im_context_connect_cb(FcitxGClient *im, void *user_data) {
    FcitxIMContext *context = FCITX_IM_CONTEXT(user_data);
    Display *display = NULL;
    if (context->client_window) {
        if (GDK_IS_WINDOW(context->client_window)) {
            auto gdkDisplay = gdk_window_get_display(context->client_window);
            if (gdkDisplay) {
                auto x11DisplayType = g_type_from_name("GdkDisplayX11");
                if (x11DisplayType &&
                    G_TYPE_CHECK_INSTANCE_TYPE(gdkDisplay, x11DisplayType)) {
                    display = GDK_DISPLAY_XDISPLAY(gdkDisplay);
                }
            }
        }
    }
    if (!display) {
        GdkDisplay *gdkDisplay = gdk_display_get_default();
        display = GDK_DISPLAY_XDISPLAY(gdkDisplay);
    }

    if (display) {
        send_uuid_to_x11(display, fcitx_g_client_get_uuid(im));
    }

    _fcitx_im_context_set_capability(context, TRUE);
    if (context->has_focus && _focus_im_context == (GtkIMContext *)context &&
        fcitx_g_client_is_valid(context->client))
        fcitx_g_client_focus_in(context->client);
    /* set_cursor_location_internal() will get origin from X server,
     * it blocks UI. So delay it to idle callback. */
    gdk_threads_add_idle_full(
        G_PRIORITY_DEFAULT_IDLE, (GSourceFunc)_set_cursor_location_internal,
        g_object_ref(context), (GDestroyNotify)g_object_unref);
}

static void _request_surrounding_text(FcitxIMContext **context) {
    if (*context && fcitx_g_client_is_valid((*context)->client) &&
        (*context)->has_focus) {
        gboolean return_value;

        /* according to RH#859879, something bad could happen here. */
        g_object_add_weak_pointer((GObject *)*context, (gpointer *)context);
        /* some unref can happen here */
        g_signal_emit(*context, _signal_retrieve_surrounding_id, 0,
                      &return_value);
        if (*context)
            g_object_remove_weak_pointer((GObject *)*context,
                                         (gpointer *)context);
        else
            return;
        if (return_value) {
            (*context)->support_surrounding_text = TRUE;
            _fcitx_im_context_set_capability(*context, FALSE);
        } else {
            (*context)->support_surrounding_text = FALSE;
            _fcitx_im_context_set_capability(*context, FALSE);
        }
    }
}

static gint _key_snooper_cb(GtkWidget *, GdkEventKey *event, gpointer) {
    gboolean retval = FALSE;

    FcitxIMContext *fcitxcontext = (FcitxIMContext *)_focus_im_context;

    if (G_UNLIKELY(!_use_key_snooper))
        return FALSE;

    if (fcitxcontext == NULL || !fcitxcontext->has_focus)
        return FALSE;

    if (G_UNLIKELY(event->state & (guint32)HandledMask))
        return TRUE;

    if (G_UNLIKELY(event->state & (guint32)IgnoredMask))
        return FALSE;

    do {
        if (!fcitx_g_client_is_valid(fcitxcontext->client)) {
            break;
        }

        _request_surrounding_text(&fcitxcontext);
        if (G_UNLIKELY(!fcitxcontext))
            return FALSE;

        auto state = _update_auto_repeat_state(fcitxcontext, event);

        _fcitx_im_context_push_event(fcitxcontext, event);
        if (_use_sync_mode) {
            retval = fcitx_g_client_process_key_sync(
                fcitxcontext->client, event->keyval, event->hardware_keycode,
                state, (event->type == GDK_KEY_RELEASE), event->time);
        } else {
            fcitx_g_client_process_key(
                fcitxcontext->client, event->keyval, event->hardware_keycode,
                state, (event->type == GDK_KEY_RELEASE), event->time, -1, NULL,
                _fcitx_im_context_process_key_cb,
                gdk_event_copy((GdkEvent *)event));
            retval = TRUE;
        }
    } while (0);

    if (!retval) {
        event->state |= (guint32)IgnoredMask;
        return FALSE;
    } else {
        event->state |= (guint32)HandledMask;
        return TRUE;
    }

    return retval;
}

guint _update_auto_repeat_state(FcitxIMContext *context, GdkEventKey *event) {
    // GDK calls to XkbSetDetectableAutoRepeat by default, so normal there will
    // be no key release. But it might be also override by the application
    // itself.
    bool is_auto_repeat = false;
    if (event->type == GDK_KEY_RELEASE) {
        // Always mark key release as non auto repeat, because we don't know if
        // it is real release.
        is_auto_repeat = false;
    } else {
        // If timestamp is same as last release
        if (context->last_is_release) {
            if (context->time && context->time == event->time &&
                context->last_key_code == event->hardware_keycode) {
                is_auto_repeat = true;
            }
        } else {
            if (context->last_key_code == event->hardware_keycode) {
                is_auto_repeat = true;
            }
        }
    }

    context->last_key_code = event->hardware_keycode;
    context->last_is_release = event->type == GDK_KEY_RELEASE;
    context->time = event->time;
    auto state = event->state;
    if (is_auto_repeat) {
        // KeyState::Repeat
        state |= (1u << 31);
    }
    return state;
}
}

// kate: indent-mode cstyle; replace-tabs on;
