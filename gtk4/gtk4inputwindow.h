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
    void syncFontOptions();

    bool supportAlpha = false;
    // Dummy widget to track font options.
    UniqueCPtr<GtkWindow, gtk_window_destroy> dummyWidget_;
    UniqueCPtr<GdkSurface, gdk_surface_destroy> window_;
    UniqueCPtr<GdkCairoContext, g_object_unref> cairoCcontext_;
    GtkWidget *parent_ = nullptr;
    // Parent surface we connected "notify::mapped" to. Tracked via a weak
    // pointer so it is cleared automatically if the surface is finalized
    // before us, and so resetWindow() can always disconnect the handler
    // (avoids a use-after-free when the parent window is destroyed while a
    // stale handler still points at this object).
    GdkSurface *notifySurface_ = nullptr;
    size_t width_ = 1;
    size_t height_ = 1;
    GdkRectangle rect_;
    double scrollDelta_ = 0;
};

} // namespace fcitx::gtk

#endif // _GTK3_GTK3INPUTWINDOW_H_
