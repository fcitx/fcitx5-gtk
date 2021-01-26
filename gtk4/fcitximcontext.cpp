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
#include "fcitxtheme.h"
#include "gtk4inputwindow.h"
#include <gdk/gdk.h>
#include <gdk/gdkevents.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xkbcommon/xkbcommon-compose.h>

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif

#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

using namespace fcitx::gtk;

constexpr int MAX_CACHED_HANDLED_EVENT = 40;

static const uint64_t purpose_related_capability =
    fcitx::FcitxCapabilityFlag_Alpha | fcitx::FcitxCapabilityFlag_Digit |
    fcitx::FcitxCapabilityFlag_Number | fcitx::FcitxCapabilityFlag_Dialable |
    fcitx::FcitxCapabilityFlag_Url | fcitx::FcitxCapabilityFlag_Email |
    fcitx::FcitxCapabilityFlag_Password;

static const uint64_t hints_related_capability =
    fcitx::FcitxCapabilityFlag_SpellCheck |
    fcitx::FcitxCapabilityFlag_NoSpellCheck |
    fcitx::FcitxCapabilityFlag_WordCompletion |
    fcitx::FcitxCapabilityFlag_Lowercase |
    fcitx::FcitxCapabilityFlag_Uppercase |
    fcitx::FcitxCapabilityFlag_UppercaseWords |
    fcitx::FcitxCapabilityFlag_UppwercaseSentences |
    fcitx::FcitxCapabilityFlag_NoOnScreenKeyboard;

struct KeyPressCallbackData {
    KeyPressCallbackData(FcitxIMContext *context, GdkEvent *event)
        : context_(FCITX_IM_CONTEXT(g_object_ref(context))),
          event_(gdk_event_ref(event)) {}

    ~KeyPressCallbackData() {
        gdk_event_unref(event_);
        g_object_unref(context_);
    }

    FcitxIMContext *context_;
    GdkEvent *event_;
};

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

    Gtk4InputWindow *candidate_window;
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
static void fcitx_im_context_set_client_widget(GtkIMContext *context,
                                               GtkWidget *client_widget);
static gboolean fcitx_im_context_filter_keypress(GtkIMContext *context,
                                                 GdkEvent *key);
static void fcitx_im_context_reset(GtkIMContext *context);
static void fcitx_im_context_focus_in(GtkIMContext *context);
static void fcitx_im_context_focus_out(GtkIMContext *context);
static void fcitx_im_context_set_cursor_location(GtkIMContext *context,
                                                 GdkRectangle *area);
static void fcitx_im_context_set_use_preedit(GtkIMContext *context,
                                             gboolean use_preedit);
static void fcitx_im_context_set_surrounding(GtkIMContext *context,
                                             const char *text, int len,
                                             int cursor_index);
static void fcitx_im_context_get_preedit_string(GtkIMContext *context,
                                                char **str,
                                                PangoAttrList **attrs,
                                                int *cursor_pos);

static gboolean _set_cursor_location_internal(FcitxIMContext *fcitxcontext);
static gboolean _defer_request_surrounding_text(FcitxIMContext *fcitxcontext);
static void _slave_commit_cb(GtkIMContext *slave, char *string,
                             FcitxIMContext *context);
static void _slave_preedit_changed_cb(GtkIMContext *slave,
                                      FcitxIMContext *context);
static void _slave_preedit_start_cb(GtkIMContext *slave,
                                    FcitxIMContext *context);
static void _slave_preedit_end_cb(GtkIMContext *slave, FcitxIMContext *context);
static gboolean _slave_retrieve_surrounding_cb(GtkIMContext *slave,
                                               FcitxIMContext *context);
static gboolean _slave_delete_surrounding_cb(GtkIMContext *slave,
                                             int offset_from_cursor,
                                             guint nchars,
                                             FcitxIMContext *context);
static void _fcitx_im_context_commit_string_cb(FcitxGClient *client, char *str,
                                               void *user_data);
static void _fcitx_im_context_forward_key_cb(FcitxGClient *client, guint keyval,
                                             guint state, int type,
                                             void *user_data);
static void _fcitx_im_context_delete_surrounding_text_cb(FcitxGClient *client,
                                                         int offset_from_cursor,
                                                         guint nchars,
                                                         void *user_data);
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

static void _fcitx_im_context_input_hints_changed_cb(GObject *gobject,
                                                     GParamSpec *pspec,
                                                     gpointer user_data);
static void _fcitx_im_context_input_purpose_changed_cb(GObject *gobject,
                                                       GParamSpec *pspec,
                                                       gpointer user_data);

static void _request_surrounding_text(FcitxIMContext **context);

guint _update_auto_repeat_state(FcitxIMContext *context, GdkEvent *event);

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
static const char *_no_preedit_apps = NO_PREEDIT_APPS;
static const char *_sync_mode_apps = SYNC_MODE_APPS;
static FcitxGWatcher *_watcher = NULL;
static struct xkb_context *xkbContext = NULL;
static struct xkb_compose_table *xkbComposeTable = NULL;
static ClassicUIConfig *_uiconfig = nullptr;

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

static gboolean check_app_name(const char *pattern) {
    bool result = FALSE;
    const char *prgname = g_get_prgname();
    char **p;
    char **apps = g_strsplit(pattern, ",", 0);
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

    im_context_class->set_client_widget = fcitx_im_context_set_client_widget;
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
}

static void fcitx_im_context_class_fini(FcitxIMContextClass *, gpointer) {}

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

#ifdef GDK_WINDOWING_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY(gdk_display_get_default())) {
        context->is_wayland = TRUE;
    }
#endif
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

    g_signal_connect(context, "notify::input-hints",
                     G_CALLBACK(_fcitx_im_context_input_hints_changed_cb),
                     NULL);
    g_signal_connect(context, "notify::input-purpose",
                     G_CALLBACK(_fcitx_im_context_input_purpose_changed_cb),
                     NULL);

    context->time = GDK_CURRENT_TIME;
    context->pending_events =
        g_hash_table_new_full(g_direct_hash, g_direct_equal,
                              (GDestroyNotify)gdk_event_unref, nullptr);
    context->handled_events =
        g_hash_table_new_full(g_direct_hash, g_direct_equal,
                              (GDestroyNotify)gdk_event_unref, nullptr);
    context->handled_events_list = g_queue_new();

    static gsize has_info = 0;
    if (g_once_init_enter(&has_info)) {
        _watcher = fcitx_g_watcher_new();
        _uiconfig = new ClassicUIConfig;
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
    if (context->is_wayland) {
        fcitx_g_client_set_display(context->client, "wayland:");
    } else {
#ifdef GDK_WINDOWING_X11
        if (GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
            fcitx_g_client_set_display(context->client, "x11:");
        }
#endif
    }
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
}

static void fcitx_im_context_finalize(GObject *obj) {
    FcitxIMContext *context = FCITX_IM_CONTEXT(obj);

    g_clear_pointer(&context->handled_events_list, g_queue_free);
    g_clear_pointer(&context->pending_events, g_hash_table_unref);
    g_clear_pointer(&context->handled_events, g_hash_table_unref);
    fcitx_im_context_set_client_widget(GTK_IM_CONTEXT(context), NULL);

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

    G_OBJECT_CLASS(parent_class)->finalize(obj);
}

///
static void fcitx_im_context_set_client_widget(GtkIMContext *context,
                                               GtkWidget *client_widget) {
    FcitxIMContext *fcitxcontext = FCITX_IM_CONTEXT(context);
    if (!client_widget)
        return;

    g_clear_object(&fcitxcontext->client_widget);
    fcitxcontext->client_widget = GTK_WIDGET(g_object_ref(client_widget));
    if (!fcitxcontext->candidate_window) {
        fcitxcontext->candidate_window =
            new Gtk4InputWindow(_uiconfig, fcitxcontext->client);
        fcitxcontext->candidate_window->setParent(fcitxcontext->client_widget);
        fcitxcontext->candidate_window->setCursorRect(fcitxcontext->area);
    }
}

static gboolean
fcitx_im_context_filter_keypress_fallback(FcitxIMContext *context,
                                          GdkEvent *event) {
    if (!context->xkbComposeState ||
        gdk_event_get_event_type(event) == GDK_KEY_RELEASE) {
        return gtk_im_context_filter_keypress(context->slave, event);
    }

    struct xkb_compose_state *xkbComposeState = context->xkbComposeState;

    enum xkb_compose_feed_result result = xkb_compose_state_feed(
        xkbComposeState, gdk_key_event_get_keyval(event));
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

void fcitx_im_context_mark_event_handled(FcitxIMContext *fcitxcontext,
                                         GdkEvent *event) {
    g_hash_table_add(fcitxcontext->handled_events,
                     gdk_event_ref(GDK_EVENT(event)));
    g_hash_table_remove(fcitxcontext->pending_events, event);
    g_queue_push_tail(fcitxcontext->handled_events_list, event);

    while (g_hash_table_size(fcitxcontext->handled_events) >
           MAX_CACHED_HANDLED_EVENT) {
        g_hash_table_remove(
            fcitxcontext->handled_events,
            g_queue_pop_head(fcitxcontext->handled_events_list));
    }
}

///
static gboolean fcitx_im_context_filter_keypress(GtkIMContext *context,
                                                 GdkEvent *event) {
    FcitxIMContext *fcitxcontext = FCITX_IM_CONTEXT(context);
    if (g_hash_table_contains(fcitxcontext->handled_events, event)) {
        return TRUE;
    }

    if (g_hash_table_contains(fcitxcontext->pending_events, event)) {
        fcitx_im_context_mark_event_handled(fcitxcontext, event);
        return gtk_im_context_filter_keypress(fcitxcontext->slave, event);
    }

    if (fcitx_g_client_is_valid(fcitxcontext->client) &&
        fcitxcontext->has_focus) {
        _request_surrounding_text(&fcitxcontext);
        if (G_UNLIKELY(!fcitxcontext))
            return FALSE;

        auto state = _update_auto_repeat_state(fcitxcontext, event);

        if (_use_sync_mode) {
            gboolean ret = fcitx_g_client_process_key_sync(
                fcitxcontext->client, gdk_key_event_get_keyval(event),
                gdk_key_event_get_keycode(event), state,
                (gdk_event_get_event_type(event) != GDK_KEY_PRESS),
                gdk_event_get_time(event));
            if (ret) {
                return TRUE;
            } else {
                return fcitx_im_context_filter_keypress_fallback(fcitxcontext,
                                                                 event);
            }
        } else {
            g_hash_table_add(fcitxcontext->pending_events,
                             gdk_event_ref(GDK_EVENT(event)));
            fcitx_g_client_process_key(
                fcitxcontext->client, gdk_key_event_get_keyval(event),
                gdk_key_event_get_keycode(event), state,
                (gdk_event_get_event_type(event) != GDK_KEY_PRESS),
                gdk_event_get_time(event), -1, NULL,
                _fcitx_im_context_process_key_cb,
                new KeyPressCallbackData(fcitxcontext, event));
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
    KeyPressCallbackData *data = (KeyPressCallbackData *)user_data;
    gboolean ret =
        fcitx_g_client_process_key_finish(FCITX_G_CLIENT(source_object), res);
    if (!ret) {
        gdk_display_put_event(gdk_event_get_display(data->event_),
                              data->event_);
    } else {
        fcitx_im_context_mark_event_handled(data->context_, data->event_);
    }
    delete data;
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
        const char *s = preedit->string;
        int type = preedit->type;

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
            GdkRGBA fg;
            GdkRGBA bg;
            memset(&fg, 0, sizeof(fg));
            memset(&bg, 0, sizeof(fg));

            if (context->client_widget) {
                hasColor = true;
                GtkStyleContext *styleContext =
                    gtk_widget_get_style_context(context->client_widget);
                GdkRGBA fg_rgba, bg_rgba;
                hasColor =
                    gtk_style_context_lookup_color(
                        styleContext, "theme_selected_bg_color", &bg_rgba) &&
                    gtk_style_context_lookup_color(
                        styleContext, "theme_selected_fg_color", &fg_rgba);

                if (hasColor) {
                    fg = fg_rgba;
                    bg = bg_rgba;
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

    char *str = g_string_free(gstr, FALSE);

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
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                    (GSourceFunc)_set_cursor_location_internal,
                    g_object_ref(fcitxcontext), (GDestroyNotify)g_object_unref);

    /* _request_surrounding_text may trigger freeze in Libreoffice. After
     * focus in, the request is not as urgent as key event. Delay it to main
     * idle callback. */
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                    (GSourceFunc)_defer_request_surrounding_text,
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
    if (fcitxcontext->candidate_window) {
        fcitxcontext->candidate_window->setCursorRect(fcitxcontext->area);
    }

    if (fcitx_g_client_is_valid(fcitxcontext->client)) {
        _set_cursor_location_internal(fcitxcontext);
    }
    gtk_im_context_set_cursor_location(fcitxcontext->slave, area);

    return;
}

static gboolean _set_cursor_location_internal(FcitxIMContext *fcitxcontext) {
    GdkRectangle area;

    if (fcitxcontext->client_widget == NULL ||
        !fcitx_g_client_is_valid(fcitxcontext->client)) {
        return FALSE;
    }

    auto *root = gtk_widget_get_root(fcitxcontext->client_widget);
    if (!root) {
        return FALSE;
    }

    area = fcitxcontext->area;

    int scale = gtk_widget_get_scale_factor(fcitxcontext->client_widget);
    GdkDisplay *display = gtk_widget_get_display(fcitxcontext->client_widget);
    double px, py;
    gtk_widget_translate_coordinates(fcitxcontext->client_widget,
                                     GTK_WIDGET(root), area.x, area.y, &px,
                                     &py);
    // Add frame.
    double offsetX = 0, offsetY = 0;
    if (auto native = gtk_widget_get_native(GTK_WIDGET(root))) {
        gtk_native_get_surface_transform(native, &offsetX, &offsetY);
    }
    area.x = px + offsetX;
    area.y = py + offsetY;
#ifdef GDK_WINDOWING_X11
    if (GDK_IS_X11_DISPLAY(display)) {
        if (auto *native = gtk_widget_get_native(GTK_WIDGET(root))) {
            if (auto *surface = gtk_native_get_surface(native);
                surface && GDK_IS_X11_SURFACE(surface)) {
                if (area.x == -1 && area.y == -1 && area.width == 0 &&
                    area.height == 0) {
                    area.x = 0;
                    area.y += gdk_surface_get_height(surface);
                }
                int rootX, rootY;
                Window child;
                XTranslateCoordinates(
                    GDK_SURFACE_XDISPLAY(surface), GDK_SURFACE_XID(surface),
                    gdk_x11_display_get_xrootwindow(display), area.x * scale,
                    area.y * scale, &rootX, &rootY, &child);

                rootX /= scale;
                rootY /= scale;
                area.x = rootX;
                area.y = rootY;
            }
        }
    }
#endif
    area.x *= scale;
    area.y *= scale;
    area.width *= scale;
    area.height *= scale;

    // We don't really need this check, but we can keep certain level of
    // compatibility for fcitx 4.
    if (fcitxcontext->is_wayland) {
        fcitx_g_client_set_cursor_rect_with_scale_factor(
            fcitxcontext->client, area.x, area.y, area.width, area.height,
            scale);
    } else {
        fcitx_g_client_set_cursor_rect(fcitxcontext->client, area.x, area.y,
                                       area.width, area.height);
    }
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
    if (fcitxcontext->client_widget == NULL) {
        return cursor_pos;
    }

    if (!GTK_IS_TEXT_VIEW(fcitxcontext->client_widget)) {
        return cursor_pos;
    }

    GtkTextView *text_view = GTK_TEXT_VIEW(fcitxcontext->client_widget);
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
                                             const char *text, int l,
                                             int cursor_index) {
    g_return_if_fail(context != NULL);
    g_return_if_fail(FCITX_IS_IM_CONTEXT(context));
    g_return_if_fail(text != NULL);

    int len = l;
    if (len < 0) {
        len = strlen(text);
    }

    g_return_if_fail(0 <= cursor_index && cursor_index <= len);

    FcitxIMContext *fcitxcontext = FCITX_IM_CONTEXT(context);

    if (fcitx_g_client_is_valid(fcitxcontext->client) &&
        !(fcitxcontext->last_updated_capability &
          (guint64)fcitx::FcitxCapabilityFlag_Password)) {
        int cursor_pos;
        guint utf8_len;
        char *p;

        p = g_strndup(text, len);
        cursor_pos = g_utf8_strlen(p, cursor_index);
        utf8_len = g_utf8_strlen(p, len);

        int anchor_pos =
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
        if (fcitxcontext->is_wayland) {
            flags |= (guint64)fcitx::FcitxCapabilityFlag_RelativeRect;
            flags |= (guint64)fcitx::FcitxCapabilityFlag_ClientSideInputPanel;
        }
        flags |= (guint64)fcitx::FcitxCapabilityFlag_ReportKeyRepeat;

        // always run this code against all gtk version
        // seems visibility != PASSWORD hint
        if (fcitxcontext->client_widget != NULL) {
            if (GTK_IS_TEXT(fcitxcontext->client_widget) &&
                !gtk_text_get_visibility(
                    GTK_TEXT(fcitxcontext->client_widget))) {
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
                                                char **str,
                                                PangoAttrList **attrs,
                                                int *cursor_pos) {
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
static void _slave_commit_cb(GtkIMContext *, char *string,
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
                                             int offset_from_cursor,
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
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                    (GSourceFunc)_defer_request_surrounding_text,
                    g_object_ref(context), (GDestroyNotify)g_object_unref);
}

void _fcitx_im_context_forward_key_cb(FcitxGClient *, guint, guint, gboolean,
                                      void *) {}

static void _fcitx_im_context_delete_surrounding_text_cb(FcitxGClient *,
                                                         int offset_from_cursor,
                                                         guint nchars,
                                                         void *user_data) {
    FcitxIMContext *context = FCITX_IM_CONTEXT(user_data);
    gboolean return_value;
    g_signal_emit(context, _signal_delete_surrounding_id, 0, offset_from_cursor,
                  nchars, &return_value);
}

#ifdef GDK_WINDOWING_X11
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
#endif

void _fcitx_im_context_connect_cb(FcitxGClient *im G_GNUC_UNUSED,
                                  void *user_data) {
    FcitxIMContext *context = FCITX_IM_CONTEXT(user_data);
#ifdef GDK_WINDOWING_X11
    Display *display = NULL;
    if (context->client_widget) {
        auto gdkDisplay = gtk_widget_get_display(context->client_widget);
        if (gdkDisplay) {
            auto x11DisplayType = g_type_from_name("GdkDisplayX11");
            if (x11DisplayType &&
                G_TYPE_CHECK_INSTANCE_TYPE(gdkDisplay, x11DisplayType)) {
                display = GDK_DISPLAY_XDISPLAY(gdkDisplay);
            }
        }
    }
    if (!display) {
        GdkDisplay *gdkDisplay = gdk_display_get_default();
        if (GDK_IS_X11_DISPLAY(gdkDisplay)) {
            display = GDK_DISPLAY_XDISPLAY(gdkDisplay);
        }
    }

    if (display) {
        send_uuid_to_x11(display, fcitx_g_client_get_uuid(im));
    }
#endif

    _fcitx_im_context_set_capability(context, TRUE);
    if (context->has_focus && _focus_im_context == (GtkIMContext *)context &&
        fcitx_g_client_is_valid(context->client))
        fcitx_g_client_focus_in(context->client);
    /* set_cursor_location_internal() will get origin from X server,
     * it blocks UI. So delay it to idle callback. */
    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                    (GSourceFunc)_set_cursor_location_internal,
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

guint _update_auto_repeat_state(FcitxIMContext *context, GdkEvent *event) {
    // GDK calls to XkbSetDetectableAutoRepeat by default, so normal there will
    // be no key release. But it might be also override by the application
    // itself.
    bool is_auto_repeat = false;
    if (gdk_event_get_event_type(event) == GDK_KEY_RELEASE) {
        // Always mark key release as non auto repeat, because we don't know if
        // it is real release.
        is_auto_repeat = false;
    } else {
        // If timestamp is same as last release
        if (context->last_is_release) {
            if (context->time && context->time == gdk_event_get_time(event) &&
                context->last_key_code == gdk_key_event_get_keycode(event)) {
                is_auto_repeat = true;
            }
        } else {
            if (context->last_key_code == gdk_key_event_get_keycode(event)) {
                is_auto_repeat = true;
            }
        }
    }

    context->last_key_code = gdk_key_event_get_keycode(event);
    context->last_is_release =
        gdk_event_get_event_type(event) == GDK_KEY_RELEASE;
    context->time = gdk_event_get_time(event);
    auto state = static_cast<uint32_t>(gdk_event_get_modifier_state(event));
    if (is_auto_repeat) {
        // KeyState::Repeat
        state |= (1u << 31);
    }
    return state;
}

void _fcitx_im_context_input_purpose_changed_cb(GObject *gobject, GParamSpec *,
                                                gpointer) {
    FcitxIMContext *fcitxcontext = FCITX_IM_CONTEXT(gobject);

    GtkInputPurpose purpose;
    g_object_get(gobject, "input-purpose", &purpose, NULL);

    fcitxcontext->capability_from_toolkit &= ~purpose_related_capability;

#define CASE_PURPOSE(_PURPOSE, _CAPABILITY)                                    \
    case _PURPOSE:                                                             \
        fcitxcontext->capability_from_toolkit |= (guint64)_CAPABILITY;         \
        break;

    switch (purpose) {
        CASE_PURPOSE(GTK_INPUT_PURPOSE_ALPHA, fcitx::FcitxCapabilityFlag_Alpha)
        CASE_PURPOSE(GTK_INPUT_PURPOSE_DIGITS,
                     fcitx::FcitxCapabilityFlag_Digit);
        CASE_PURPOSE(GTK_INPUT_PURPOSE_NUMBER,
                     fcitx::FcitxCapabilityFlag_Number)
        CASE_PURPOSE(GTK_INPUT_PURPOSE_PHONE,
                     fcitx::FcitxCapabilityFlag_Dialable)
        CASE_PURPOSE(GTK_INPUT_PURPOSE_URL, fcitx::FcitxCapabilityFlag_Url)
        CASE_PURPOSE(GTK_INPUT_PURPOSE_EMAIL, fcitx::FcitxCapabilityFlag_Email)
        CASE_PURPOSE(GTK_INPUT_PURPOSE_NAME, fcitx::FcitxCapabilityFlag_Name)
        CASE_PURPOSE(GTK_INPUT_PURPOSE_PASSWORD,
                     fcitx::FcitxCapabilityFlag_Password)
        CASE_PURPOSE(GTK_INPUT_PURPOSE_PIN,
                     (guint64)fcitx::FcitxCapabilityFlag_Password |
                         (guint64)fcitx::FcitxCapabilityFlag_Digit)
    case GTK_INPUT_PURPOSE_FREE_FORM:
    default:
        break;
    }

    _fcitx_im_context_set_capability(fcitxcontext, FALSE);
}

void _fcitx_im_context_input_hints_changed_cb(GObject *gobject, GParamSpec *,
                                              gpointer) {
    FcitxIMContext *fcitxcontext = FCITX_IM_CONTEXT(gobject);

    GtkInputHints hints;
    g_object_get(gobject, "input-hints", &hints, NULL);

    fcitxcontext->capability_from_toolkit &= ~hints_related_capability;

#define CHECK_HINTS(_HINTS, _CAPABILITY)                                       \
    if (hints & _HINTS)                                                        \
        fcitxcontext->capability_from_toolkit |= (guint64)_CAPABILITY;

    CHECK_HINTS(GTK_INPUT_HINT_SPELLCHECK,
                fcitx::FcitxCapabilityFlag_SpellCheck)
    CHECK_HINTS(GTK_INPUT_HINT_NO_SPELLCHECK,
                fcitx::FcitxCapabilityFlag_NoSpellCheck);
    CHECK_HINTS(GTK_INPUT_HINT_WORD_COMPLETION,
                fcitx::FcitxCapabilityFlag_WordCompletion)
    CHECK_HINTS(GTK_INPUT_HINT_LOWERCASE, fcitx::FcitxCapabilityFlag_Lowercase)
    CHECK_HINTS(GTK_INPUT_HINT_UPPERCASE_CHARS,
                fcitx::FcitxCapabilityFlag_Uppercase)
    CHECK_HINTS(GTK_INPUT_HINT_UPPERCASE_WORDS,
                fcitx::FcitxCapabilityFlag_UppercaseWords)
    CHECK_HINTS(GTK_INPUT_HINT_UPPERCASE_SENTENCES,
                fcitx::FcitxCapabilityFlag_UppwercaseSentences)
    CHECK_HINTS(GTK_INPUT_HINT_INHIBIT_OSK,
                fcitx::FcitxCapabilityFlag_NoOnScreenKeyboard)

    _fcitx_im_context_set_capability(fcitxcontext, FALSE);
}
}

// kate: indent-mode cstyle; replace-tabs on;
