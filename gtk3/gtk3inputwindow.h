/*
 * SPDX-FileCopyrightText: 2021~2021 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */
#ifndef _GTK3_GTK3INPUTWINDOW_H_
#define _GTK3_GTK3INPUTWINDOW_H_

#include "inputwindow.h"
#include <gtk/gtk.h>

namespace fcitx::gtk {

class Gtk3InputWindow : public InputWindow {
public:
    Gtk3InputWindow(ClassicUIConfig *config, FcitxGClient *client);

    ~Gtk3InputWindow();

    void setParent(GdkWindow *parent);
    void update() override;
    void setCursorRect(GdkRectangle rect);

private:
    void init();
    void draw(cairo_t *cr);
    void screenChanged();
    void reposition();
    void scroll(GdkEvent *event);
    void motion(GdkEvent *event);
    void release(GdkEvent *event);
    void leave();

    bool supportAlpha = false;
    UniqueCPtr<GtkWidget, gtk_widget_destroy> window_;
    GdkWindow *parent_ = nullptr;
    int width_ = 1;
    int height_ = 1;
    GdkRectangle rect_;
    double scrollDelta_ = 0;
};

} // namespace fcitx::gtk

#endif // _GTK3_GTK3INPUTWINDOW_H_
