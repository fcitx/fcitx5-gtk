/*
 * SPDX-FileCopyrightText: 2021~2021 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */
#include "fcitximcontext5.h"
#include "fcitximcontextprivate.h"

static GType _fcitx_type_im_context5 = 0;

struct _FcitxIMContext5Class {
    FcitxIMContextClass parent;
    /* klass members */
};

struct _FcitxIMContext5 {
    FcitxIMContext parent;
};

void fcitx_im_context5_register_type(GTypeModule *type_module) {
    static const GTypeInfo fcitx_im_context5_info = {
        sizeof(FcitxIMContext5Class), (GBaseInitFunc)NULL,
        (GBaseFinalizeFunc)NULL,      (GClassInitFunc)NULL,
        (GClassFinalizeFunc)NULL,     NULL, /* klass data */
        sizeof(FcitxIMContext5),      0,
        (GInstanceInitFunc)NULL,      0};

    if (_fcitx_type_im_context5) {
        return;
    }
    if (type_module) {
        _fcitx_type_im_context5 = g_type_module_register_type(
            type_module, FCITX_TYPE_IM_CONTEXT, "FcitxIMContext5",
            &fcitx_im_context5_info, (GTypeFlags)0);
    } else {
        _fcitx_type_im_context5 =
            g_type_register_static(FCITX_TYPE_IM_CONTEXT, "FcitxIMContext5",
                                   &fcitx_im_context5_info, (GTypeFlags)0);
    }
}

GType fcitx_im_context5_get_type(void) {
    if (_fcitx_type_im_context5 == 0) {
        fcitx_im_context5_register_type(NULL);
    }

    g_assert(_fcitx_type_im_context5 != 0);
    return _fcitx_type_im_context5;
}
