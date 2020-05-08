/*
 * SPDX-FileCopyrightText: 2010~2020 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef __FCITX_IM_CONTEXT_H_
#define __FCITX_IM_CONTEXT_H_

#include <gtk/gtk.h>

/*
 * Type macros.
 */
#define FCITX_TYPE_IM_CONTEXT (fcitx_im_context_get_type())
#define FCITX_IM_CONTEXT(obj)                                                  \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), FCITX_TYPE_IM_CONTEXT, FcitxIMContext))
#define FCITX_IM_CONTEXT_CLASS(klass)                                          \
    (G_TYPE_CHECK_CLASS_CAST((klass), FCITX_TYPE_IM_CONTEXT,                   \
                             FcitxIMContextClass))
#define FCITX_IS_IM_CONTEXT(obj)                                               \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), FCITX_TYPE_IM_CONTEXT))
#define FCITX_IS_IM_CONTEXT_CLASS(klass)                                       \
    (G_TYPE_CHECK_CLASS_TYPE((klass), FCITX_TYPE_IM_CONTEXT))
#define FCITX_IM_CONTEXT_GET_CLASS(obj)                                        \
    (G_TYPE_CHECK_GET_CLASS((obj), FCITX_TYPE_IM_CONTEXT, FcitxIMContextClass))

G_BEGIN_DECLS

typedef struct _FcitxIMContext FcitxIMContext;
typedef struct _FcitxIMContextClass FcitxIMContextClass;

GType fcitx_im_context_get_type(void);
FcitxIMContext *fcitx_im_context_new(void);
void fcitx_im_context_register_type(GTypeModule *type_module);

G_END_DECLS
#endif
// kate: indent-mode cstyle; space-indent on; indent-width 0;
