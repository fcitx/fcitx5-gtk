/*
 * SPDX-FileCopyrightText: 2021~2021 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */
#include "gtk3inputwindow.h"
#include "fcitxtheme.h"
#include <gtk/gtk.h>

namespace fcitx::gtk {

Gtk3InputWindow::Gtk3InputWindow(ClassicUIConfig *config, FcitxGClient *client,
                                 bool isWayland)
    : InputWindow(config, client), isWayland_(isWayland) {}

Gtk3InputWindow::~Gtk3InputWindow() {
    if (window_) {
        g_signal_handlers_disconnect_by_data(window_.get(), this);
        window_.reset();
    }
    // Clean up weak pointer reference.
    setParent(nullptr);
}

void Gtk3InputWindow::draw(cairo_t *cr) { paint(cr, width_, height_); }

void Gtk3InputWindow::screenChanged() {
    GdkScreen *screen = gtk_widget_get_screen(window_.get());
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);

    if (!visual) {
        visual = gdk_screen_get_system_visual(screen);
        supportAlpha = false;
    } else {
        supportAlpha = true;
    }

    gtk_widget_set_visual(window_.get(), visual);
}

void Gtk3InputWindow::setParent(GdkWindow *parent) {
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
        if (window_) {
            gtk_window_set_screen(GTK_WINDOW(window_.get()),
                                  gdk_window_get_screen(parent));
            gtk_widget_realize(window_.get());
            auto window = gtk_widget_get_window(window_.get());
            if (window) {
                gdk_window_set_transient_for(window, parent);
            }
        }
    }
    parent_ = parent;
}

void Gtk3InputWindow::setCursorRect(GdkRectangle rect) {
    if (!parent_) {
        return;
    }
    if (rect.height <= 1) {
        rect.y = rect.y - 20 + rect.height;
        rect.height = 20;
    }

    if (rect_.x != rect.x || rect_.y != rect.y || rect_.height != rect.height ||
        rect_.width != rect.width) {
        rect_ = rect;
        if (window_) {
            reposition();
        }
    }
}

void Gtk3InputWindow::update() {
    if (visible() && parent_) {
        init();
        pango_cairo_context_set_font_options(
            context_.get(),
            gdk_screen_get_font_options(gtk_widget_get_screen(window_.get())));
        dpi_ = gdk_screen_get_resolution(gtk_widget_get_screen(window_.get()));
        pango_cairo_context_set_resolution(context_.get(), dpi_);
        std::tie(width_, height_) = sizeHint();
        if (width_ <= 0 || height_ <= 0) {
            gtk_widget_hide(window_.get());
            return;
        }

        if (auto gdkWindow = gtk_widget_get_window(window_.get())) {
            gdk_window_set_shadow_width(
                gdkWindow, config_->theme_.shadowMargin.marginLeft,
                config_->theme_.shadowMargin.marginRight,
                config_->theme_.shadowMargin.marginTop,
                config_->theme_.shadowMargin.marginBottom);
        }

        gtk_widget_realize(window_.get());
        gtk_window_resize(GTK_WINDOW(window_.get()), width_, height_);
        gtk_widget_queue_draw(window_.get());
        reposition();
        gtk_widget_show_all(window_.get());
    } else if (window_) {
        gtk_widget_hide(window_.get());
    }
}

void Gtk3InputWindow::init() {
    if (window_) {
        return;
    }
    if (!parent_) {
        return;
    }
    window_.reset(gtk_window_new(GTK_WINDOW_POPUP));
    auto window = window_.get();
    gtk_window_set_screen(GTK_WINDOW(window), gdk_window_get_screen(parent_));
    gtk_container_set_border_width(GTK_CONTAINER(window), 0);
    gtk_window_set_decorated(GTK_WINDOW(window), false);

    gtk_window_set_type_hint(GTK_WINDOW(window),
                             GDK_WINDOW_TYPE_HINT_POPUP_MENU);
    gtk_widget_set_app_paintable(window, TRUE);
    gtk_widget_set_events(window, GDK_LEAVE_NOTIFY_MASK | GDK_SCROLL_MASK |
                                      GDK_BUTTON_RELEASE_MASK |
                                      GDK_POINTER_MOTION_MASK);

    auto draw = [](GtkWidget *, cairo_t *cr, gpointer userdata) {
        static_cast<Gtk3InputWindow *>(userdata)->draw(cr);
    };

    auto screen_changed = [](GtkWidget *, GdkScreen *, gpointer userdata) {
        static_cast<Gtk3InputWindow *>(userdata)->screenChanged();
    };

    auto leave = [](GtkWidget *, GdkEvent *, gpointer userdata) -> gboolean {
        static_cast<Gtk3InputWindow *>(userdata)->leave();
        return TRUE;
    };
    auto motion = [](GtkWidget *, GdkEvent *event,
                     gpointer userdata) -> gboolean {
        static_cast<Gtk3InputWindow *>(userdata)->motion(event);
        return TRUE;
    };
    auto scroll = [](GtkWidget *, GdkEvent *event,
                     gpointer userdata) -> gboolean {
        static_cast<Gtk3InputWindow *>(userdata)->scroll(event);
        return TRUE;
    };
    auto release = [](GtkWidget *, GdkEvent *event,
                      gpointer userdata) -> gboolean {
        static_cast<Gtk3InputWindow *>(userdata)->release(event);
        return TRUE;
    };
    g_signal_connect(G_OBJECT(window), "draw", G_CALLBACK(+draw), this);
    g_signal_connect(G_OBJECT(window), "screen-changed",
                     G_CALLBACK(+screen_changed), this);
    g_signal_connect(G_OBJECT(window), "motion-notify-event",
                     G_CALLBACK(+motion), this);
    g_signal_connect(G_OBJECT(window), "leave-notify-event", G_CALLBACK(+leave),
                     this);
    g_signal_connect(G_OBJECT(window), "scroll-event", G_CALLBACK(+scroll),
                     this);
    g_signal_connect(G_OBJECT(window), "button-release-event",
                     G_CALLBACK(+release), this);
    gtk_widget_realize(window_.get());
    if (auto gdkWindow = gtk_widget_get_window(window_.get())) {
        gdk_window_set_transient_for(gdkWindow, parent_);
    }

    screenChanged();
}

void Gtk3InputWindow::reposition() {
    if (!parent_ || !visible()) {
        return;
    }

    if (auto gdkWindow = gtk_widget_get_window(window_.get())) {
        if (!isWayland_) {
            gdk_window_move_to_rect(
                gdkWindow, &rect_, GDK_GRAVITY_SOUTH_WEST,
                GDK_GRAVITY_NORTH_WEST,
                (GdkAnchorHints)(GDK_ANCHOR_SLIDE_X | GDK_ANCHOR_FLIP_Y), 0, 0);
            return;
        }

        GdkWindow *parent;
        GdkWindow *window = parent_;
        int posX = rect_.x, posY = rect_.y;

        // Find the top level window.
        while ((parent = gdk_window_get_effective_parent(window)) != NULL) {
            double dx, dy;
            gdk_window_coords_to_parent(window, posX, posY, &dx, &dy);
            posX = dx;
            posY = dy;
            if (gdk_window_get_window_type(parent) == GDK_WINDOW_ROOT) {
                break;
            }
            window = parent;
        }
        int x, y, w, h;
        gdk_window_get_geometry(window, &x, &y, &w, &h);
        posY += rect_.height;

        if (posX + width_ > x + w || posY + height_ > y + h ||
            lastRect_.height != rect_.height ||
            lastRect_.width != rect_.width || lastRect_.x != rect_.x ||
            lastRect_.y != rect_.y) {
            gtk_widget_hide(window_.get());
            lastRect_ = rect_;
            gdk_window_move_to_rect(
                gdkWindow, &rect_, GDK_GRAVITY_SOUTH_WEST,
                GDK_GRAVITY_NORTH_WEST,
                (GdkAnchorHints)(GDK_ANCHOR_SLIDE_X | GDK_ANCHOR_FLIP_Y), 0, 0);
            gtk_widget_show_all(window_.get());
        }
    }
}

void Gtk3InputWindow::leave() {
    auto oldHighlight = highlight();
    hoverIndex_ = -1;
    if (highlight() != oldHighlight) {
        gtk_widget_queue_draw(window_.get());
    }
}

void Gtk3InputWindow::release(GdkEvent *event) {
    guint button;
    gdk_event_get_button(event, &button);
    if (button == 1) {
        double x = 0, y = 0;
        gdk_event_get_coords(event, &x, &y);
        click(x, y);
    }
}

void Gtk3InputWindow::scroll(GdkEvent *event) {
    double vscroll_factor = 0.0;
    double x_scroll, y_scroll;
    // Handle discrete scrolling with a known constant delta;
    const double delta = 1.0;

    // In Gtk 3, axis and axis discrete will be emitted at the same time.
    if (gdk_event_get_scroll_deltas(event, &x_scroll, &y_scroll)) {
        // Handle smooth scrolling directly
        vscroll_factor = y_scroll;
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
    }
}

void Gtk3InputWindow::motion(GdkEvent *event) {
    double x = 0, y = 0;
    gdk_event_get_coords(event, &x, &y);
    if (hover(x, y)) {
        gtk_widget_queue_draw(window_.get());
    }
}

} // namespace fcitx::gtk
