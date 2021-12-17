/*
 * SPDX-FileCopyrightText: 2021~2021 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */
#ifndef _GTK4_FCITXIMCONTEXT5_H_
#define _GTK4_FCITXIMCONTEXT5_H_

#include <gtk/gtk.h>

/*
 * Create a alias type for GTK_IM_MODULE=fcitx5
 */
#define FCITX_TYPE_IM_CONTEXT5 (fcitx_im_context5_get_type())
#define FCITX_IM_CONTEXT5(obj)                                                 \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), FCITX_TYPE_IM_CONTEXT5, FcitxIMContext5))
#define FCITX_IM_CONTEXT5_CLASS(klass)                                         \
    (G_TYPE_CHECK_CLASS_CAST((klass), FCITX_TYPE_IM_CONTEXT5,                  \
                             FcitxIMContextClass))
#define FCITX_IS_IM_CONTEXT5(obj)                                              \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), FCITX_TYPE_IM_CONTEXT5))
#define FCITX_IS_IM_CONTEXT5_CLASS(klass)                                      \
    (G_TYPE_CHECK_CLASS_TYPE((klass), FCITX_TYPE_IM_CONTEXT5))
#define FCITX_IM_CONTEXT5_GET_CLASS(obj)                                       \
    (G_TYPE_CHECK_GET_CLASS((obj), FCITX_TYPE_IM_CONTEXT5, FcitxIMContextClass))

G_BEGIN_DECLS

typedef struct _FcitxIMContext5 FcitxIMContext5;
typedef struct _FcitxIMContext5Class FcitxIMContext5Class;

GType fcitx_im_context5_get_type(void);
void fcitx_im_context5_register_type(GTypeModule *type_module);

G_END_DECLS

#endif // _GTK4_FCITXIMCONTEXT5_H_
