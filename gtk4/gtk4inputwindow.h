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

class Gtk4InputWindow : public InputWindow {
public:
    Gtk4InputWindow(ClassicUIConfig *config, FcitxGClient *client);

    ~Gtk4InputWindow();

    void setParent(GtkWidget *parent);
    void update() override;
    void setCursorRect(GdkRectangle rect);

private:
    void draw(cairo_t *cr);
    gboolean event(GdkEvent *event);
    void reposition();
    void surfaceNotifyMapped(GdkSurface *surface);
    void resetWindow();

    bool supportAlpha = false;
    UniqueCPtr<GdkSurface, gdk_surface_destroy> window_;
    GtkWidget *parent_ = nullptr;
    size_t width_ = 1;
    size_t height_ = 1;
    GdkRectangle rect_;
    double scrollDelta_ = 0;
};

} // namespace fcitx::gtk

#endif // _GTK3_GTK3INPUTWINDOW_H_
