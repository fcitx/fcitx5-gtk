/*
 * SPDX-FileCopyrightText: 2023~2023 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */
#include <gtk/gtk.h>
#include <gtk/gtkimmodule.h>
#include <iostream>

int main(int argc, char *argv[]) {
    GtkIMContext *context;
    char *preedit_string = NULL;
    PangoAttrList *preedit_attrs = NULL;
    const char *context_id;

#if GTK_CHECK_VERSION(4, 0, 0)
    (void)argc;
    (void)argv;
    gtk_init();
#else
    gtk_init(&argc, &argv);
#endif
    context = gtk_im_multicontext_new();
    gtk_im_context_get_preedit_string(context, &preedit_string, &preedit_attrs,
                                      0);
    context_id =
        gtk_im_multicontext_get_context_id(GTK_IM_MULTICONTEXT(context));
    std::cout << "GTK_IM_MODULE=" << context_id << std::endl;
    return 0;
}
