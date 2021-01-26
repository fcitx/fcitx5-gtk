/*
 * SPDX-FileCopyrightText: 2021~2021 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */
#include "gtk4inputwindow.h"
#include <gtk/gtk.h>

namespace fcitx::gtk {

Gtk4InputWindow::Gtk4InputWindow(ClassicUIConfig *config, FcitxGClient *client)
    : InputWindow(config, client) {}

Gtk4InputWindow::~Gtk4InputWindow() {
    if (window_) {
        g_signal_handlers_disconnect_by_data(window_.get(), this);
        gdk_surface_destroy(window_.release());
    }
}

void Gtk4InputWindow::draw(cairo_t *cr) { paint(cr, width_, height_); }

void Gtk4InputWindow::setParent(GtkWidget *parent) {
    if (parent_ == parent) {
        return;
    }
    if (parent_) {
        g_object_remove_weak_pointer(G_OBJECT(parent_),
                                     reinterpret_cast<gpointer *>(&parent_));
    }
    if (parent) {
        g_object_add_weak_pointer(G_OBJECT(parent),
                                  reinterpret_cast<gpointer *>(&parent_));
        resetWindow();
    }
    parent_ = parent;
}

void Gtk4InputWindow::setCursorRect(GdkRectangle rect) {
    if (!parent_) {
        return;
    }
    auto *root = gtk_widget_get_root(parent_);
    if (!root) {
        return;
    }

    double px, py;
    gtk_widget_translate_coordinates(parent_, GTK_WIDGET(root), rect.x, rect.y,
                                     &px, &py);
    double offsetX = 0, offsetY = 0;

    if (auto native = gtk_widget_get_native(GTK_WIDGET(root))) {
        gtk_native_get_surface_transform(native, &offsetX, &offsetY);
    }
    rect.x = px + offsetX;
    rect.y = py + offsetX;
    rect_ = rect;
    if (window_) {
        reposition();
    }
}

void Gtk4InputWindow::update() {
    if (!visible() || !parent_) {
        resetWindow();
        return;
    }

    auto [width, height] = sizeHint();
    if (width_ != width || height_ != height) {
        width_ = width;
        height_ = height;
    }
    auto native = gtk_widget_get_native(parent_);
    if (!native) {
        return;
    }

    auto surface = gtk_native_get_surface(native);
    if (!surface) {
        return;
    }

    if (window_) {
        if (surface == gdk_popup_get_parent(GDK_POPUP(window_.get()))) {
            gdk_surface_queue_render(window_.get());
            reposition();
            return;
        }
    }

    resetWindow();
    window_.reset(gdk_surface_new_popup(surface, false));

    auto mapped = [](GdkSurface *surface, GParamSpec *, gpointer user_data) {
        Gtk4InputWindow *that = static_cast<Gtk4InputWindow *>(user_data);
        that->surfaceNotifyMapped(surface);
    };
    g_signal_connect(surface, "notify::mapped", G_CALLBACK(+mapped), this);

    auto surface_render = [](GdkSurface *surface, cairo_region_t *region,
                             gpointer user_data) {
        auto cairo_context = gdk_surface_create_cairo_context(surface);

        gdk_draw_context_begin_frame(GDK_DRAW_CONTEXT(cairo_context), region);
        auto cr = gdk_cairo_context_cairo_create(cairo_context);
        static_cast<Gtk4InputWindow *>(user_data)->draw(cr);

        cairo_destroy(cr);

        gdk_draw_context_end_frame(GDK_DRAW_CONTEXT(cairo_context));
        return TRUE;
    };
    auto event = [](GdkSurface *, gpointer event, gpointer user_data) {
        return static_cast<Gtk4InputWindow *>(user_data)->event(
            static_cast<GdkEvent *>(event));
    };
    g_signal_connect(window_.get(), "render", G_CALLBACK(+surface_render),
                     this);
    g_signal_connect(window_.get(), "event", G_CALLBACK(+event), this);

    surfaceNotifyMapped(surface);
}

void Gtk4InputWindow::reposition() {
    if (!window_) {
        return;
    }

    auto popupLayout = gdk_popup_layout_new(&rect_, GDK_GRAVITY_SOUTH_WEST,
                                            GDK_GRAVITY_NORTH_WEST);
    gdk_popup_layout_set_anchor_hints(
        popupLayout,
        static_cast<GdkAnchorHints>(GDK_ANCHOR_SLIDE_X | GDK_ANCHOR_FLIP_Y));
    gdk_popup_present(GDK_POPUP(window_.get()), width_, height_, popupLayout);
    gdk_popup_layout_unref(popupLayout);
}

void Gtk4InputWindow::surfaceNotifyMapped(GdkSurface *surface) {
    if (surface != gdk_popup_get_parent(GDK_POPUP(window_.get()))) {
        return;
    }
    if (!window_) {
        return;
    }
    if (!gdk_surface_get_mapped(surface)) {
        resetWindow();
        return;
    } else if (visible()) {
        reposition();
    }
}

void Gtk4InputWindow::resetWindow() {
    if (!window_) {
        return;
    }
    if (auto parent = gdk_popup_get_parent(GDK_POPUP(window_.get()))) {
        g_signal_handlers_disconnect_by_data(parent, this);
        g_signal_handlers_disconnect_by_data(window_.get(), this);
        window_.reset();
    }
}

gboolean Gtk4InputWindow::event(GdkEvent *event) {
    auto eventType = gdk_event_get_event_type(event);
    if (eventType == GDK_MOTION_NOTIFY) {
        double x = 0, y = 0;
        gdk_event_get_position(event, &x, &y);
        if (hover(x, y)) {
            gdk_surface_queue_render(window_.get());
        }
    } else if (eventType == GDK_LEAVE_NOTIFY) {
        auto oldHighlight = highlight();
        hoverIndex_ = -1;
        if (highlight() != oldHighlight) {
            gdk_surface_queue_render(window_.get());
        }
        return true;
    } else if (eventType == GDK_SCROLL) {
        double vscroll_factor = 0.0;
        double x_scroll, y_scroll;
        // Handle discrete scrolling with a known constant delta;
        const double delta = 1.0;

        // In Gtk 4, there will be either discrete or axis event.
        auto direction = gdk_scroll_event_get_direction(event);
        switch (direction) {
        case GDK_SCROLL_UP:
            vscroll_factor = -delta;
            break;
        case GDK_SCROLL_DOWN:
            vscroll_factor = delta;
            break;
        case GDK_SCROLL_SMOOTH:
            gdk_scroll_event_get_deltas(event, &x_scroll, &y_scroll);
            // Handle smooth scrolling directly
            vscroll_factor = y_scroll;
            break;
        default:
            // no scrolling
            break;
        }
        if (vscroll_factor != 0) {
            scrollDelta_ += vscroll_factor;
            while (scrollDelta_ >= delta) {
                scrollDelta_ -= delta;
                next();
            }
            while (scrollDelta_ <= -delta) {
                scrollDelta_ += delta;
                prev();
            }
        }
        return true;
    } else if (eventType == GDK_BUTTON_RELEASE) {
        guint button = gdk_button_event_get_button(event);
        if (button == 1) {
            double x = 0, y = 0;
            gdk_event_get_position(event, &x, &y);
            click(x, y);
        }
    }
    return false;
}

} // namespace fcitx::gtk
