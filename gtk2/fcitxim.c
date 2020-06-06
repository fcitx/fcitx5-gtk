/*
 * SPDX-FileCopyrightText: 2010~2020 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"
#include "fcitximcontext.h"
#include <gtk/gtk.h>
#include <gtk/gtkimmodule.h>

static const GtkIMContextInfo fcitx5_im_info = {
    "fcitx5", "Fcitx5 (Flexible Input Method Framework5)", "fcitx5", LOCALEDIR,
    "ja:ko:zh:*"};

static const GtkIMContextInfo fcitx_im_info = {
    "fcitx", "Fcitx5 (Flexible Input Method Framework5)", "fcitx5", LOCALEDIR,
    "ja:ko:zh:*"};

static const GtkIMContextInfo *info_list[] = {&fcitx_im_info, &fcitx5_im_info};

G_MODULE_EXPORT const gchar *
g_module_check_init(G_GNUC_UNUSED GModule *module) {
    return glib_check_version(GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, 0);
}

G_MODULE_EXPORT void im_module_init(GTypeModule *type_module) {
    /* make module resident */
    g_type_module_use(type_module);
    fcitx_im_context_register_type(type_module);
}

G_MODULE_EXPORT void im_module_exit(void) {}

G_MODULE_EXPORT GtkIMContext *im_module_create(const gchar *context_id) {
    if (context_id != NULL && (g_strcmp0(context_id, "fcitx5") == 0 ||
                               g_strcmp0(context_id, "fcitx") == 0)) {
        FcitxIMContext *context;
        context = fcitx_im_context_new();
        return (GtkIMContext *)context;
    }
    return NULL;
}

G_MODULE_EXPORT void im_module_list(const GtkIMContextInfo ***contexts,
                                    gint *n_contexts) {
    *contexts = info_list;
    *n_contexts = G_N_ELEMENTS(info_list);
}

// kate: indent-mode cstyle; space-indent on; indent-width 0;
