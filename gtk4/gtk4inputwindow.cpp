/*
 * SPDX-FileCopyrightText: 2021~2021 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */
#include "gtk4inputwindow.h"
#include "inputwindow.h"
#include <cairo.h>
#include <gdk/gdk.h>
#include <glib-object.h>
#include <glib.h>
#include <graphene.h>
#include <gsk/gsk.h>
#include <gtk/gtk.h>
#include <pango/pangocairo.h>
#include <tuple>

G_BEGIN_DECLS

#define FCITX_TYPE_CLIENT_SIDE_POPUP (fcitx_client_side_popup_get_type())
G_DECLARE_FINAL_TYPE(FcitxClientSidePopup, fcitx_client_side_popup, FCITX,
                     CLIENT_SIDE_POPUP, GtkPopover)

GtkWidget *fcitx_client_side_popup_new(void);

G_END_DECLS

struct _FcitxClientSidePopup {
    GtkPopover parent_instance;
    // You can add custom state fields here (e.g., colors, values, etc.)
    fcitx::gtk::Gtk4InputWindow
        *inputWindow; // Back-reference to the input window for interaction
};

G_DEFINE_TYPE(FcitxClientSidePopup, fcitx_client_side_popup, GTK_TYPE_POPOVER)

static void fcitx_client_side_popup_snapshot(GtkWidget *widget,
                                             GtkSnapshot *snapshot) {
    int width = gtk_widget_get_width(widget);
    int height = gtk_widget_get_height(widget);

    if (width <= 0 || height <= 0) {
        return;
    }

    // Create a bounding box for the Cairo context
    graphene_rect_t bounds;
    graphene_rect_init(&bounds, 0, 0, width, height);

    // Request a Cairo context from the GTK snapshot layer
    cairo_t *cr = gtk_snapshot_append_cairo(snapshot, &bounds);

    if (auto *window = FCITX_CLIENT_SIDE_POPUP(widget)->inputWindow) {
        window->draw(cr);
    }

    // Clean up the cairo context
    cairo_destroy(cr);
}

static void fcitx_client_side_popup_set_parent(FcitxClientSidePopup *self,
                                               GtkWidget *parent) {
    if (parent == gtk_widget_get_parent(GTK_WIDGET(self))) {
        return;
    }
    if (auto *oldParent = gtk_widget_get_parent(GTK_WIDGET(self))) {
        g_signal_handlers_disconnect_by_data(oldParent, self);
        gtk_widget_unparent(GTK_WIDGET(self));
    }
    if (parent) {
        gtk_widget_set_parent(GTK_WIDGET(self), parent);
        g_signal_connect(parent, "destroy",
                         G_CALLBACK(+[](GtkWidget *widget, gpointer data) {
                             if (gtk_widget_get_parent(GTK_WIDGET(data)) ==
                                 widget) {
                                 gtk_widget_unparent(GTK_WIDGET(data));
                             }
                         }),
                         self);
    } else {
        gtk_popover_popdown(GTK_POPOVER(self));
    }
}

static void fcitx_client_side_popup_init(FcitxClientSidePopup *self) {
    self->inputWindow = nullptr; // Initialize back-reference
    gtk_widget_add_css_class(GTK_WIDGET(self), "fcitx-popup");
    gtk_widget_set_halign(GTK_WIDGET(self), GTK_ALIGN_START);
    gtk_popover_set_position(GTK_POPOVER(self), GTK_POS_BOTTOM);
    gtk_popover_set_autohide(GTK_POPOVER(self), false);
    gtk_popover_set_has_arrow(GTK_POPOVER(self), false);
    gtk_popover_set_cascade_popdown(GTK_POPOVER(self), false);
    gtk_popover_set_mnemonics_visible(GTK_POPOVER(self), false);

    auto *motionController = gtk_event_controller_motion_new();
    g_signal_connect(motionController, "motion",
                     G_CALLBACK(+[](GtkEventControllerMotion * /*controller*/,
                                    gdouble x, gdouble y, gpointer user_data) {
                         auto *self =
                             static_cast<FcitxClientSidePopup *>(user_data);
                         if (self->inputWindow) {
                             self->inputWindow->motion(x, y);
                         }
                     }),
                     self);
    g_signal_connect(motionController, "leave",
                     G_CALLBACK(+[](GtkEventControllerMotion * /*controller*/,
                                    gpointer user_data) {
                         auto *self =
                             static_cast<FcitxClientSidePopup *>(user_data);
                         if (self->inputWindow) {
                             self->inputWindow->leave();
                         }
                     }),
                     self);
    gtk_widget_add_controller(GTK_WIDGET(self),
                              GTK_EVENT_CONTROLLER(motionController));

    auto *scrollController =
        gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);
    g_signal_connect(
        scrollController, "scroll",
        G_CALLBACK(+[](GtkEventControllerScroll * /*controller*/, gdouble x,
                       gdouble y, gpointer user_data) -> gboolean {
            auto *self = static_cast<FcitxClientSidePopup *>(user_data);
            if (self->inputWindow) {
                return self->inputWindow->scroll(x, y);
            }
            return false;
        }),
        self);
    gtk_widget_add_controller(GTK_WIDGET(self),
                              GTK_EVENT_CONTROLLER(scrollController));

    auto *clickController = gtk_gesture_click_new();
    g_signal_connect(
        clickController, "released",
        G_CALLBACK(+[](GtkGestureClick *controller, gint /*n_press*/, gdouble x,
                       gdouble y, gpointer user_data) {
            auto *self = static_cast<FcitxClientSidePopup *>(user_data);
            if (self->inputWindow) {
                guint button = gtk_gesture_single_get_current_button(
                    GTK_GESTURE_SINGLE(controller));
                if (button == GDK_BUTTON_PRIMARY) {
                    self->inputWindow->click(x, y);
                }
            }
        }),
        self);
    gtk_widget_add_controller(GTK_WIDGET(self),
                              GTK_EVENT_CONTROLLER(clickController));

    g_signal_connect(GTK_WIDGET(self), "notify::visible",
                     G_CALLBACK(+[](GtkWidget *self, GParamSpec * /*pspec*/,
                                    gpointer /*data*/) {
                         auto inputWindow =
                             FCITX_CLIENT_SIDE_POPUP(self)->inputWindow;
                         // Do not become visible if the input window is not
                         // visible. This may happen when gtk_widget_show_all is
                         // called which recursively shows all children,
                         // including our attached popup.
                         if (!inputWindow || !inputWindow->visible()) {
                             gtk_widget_set_visible(self, FALSE);
                             fcitx_client_side_popup_set_parent(
                                 FCITX_CLIENT_SIDE_POPUP(self), nullptr);
                         }
                     }),
                     nullptr);
}

static void fcitx_client_side_popup_dispose(GObject *object) {
    fcitx_client_side_popup_set_parent(FCITX_CLIENT_SIDE_POPUP(object),
                                       nullptr);
    G_OBJECT_CLASS(fcitx_client_side_popup_parent_class)->dispose(object);
}

static void
fcitx_client_side_popup_class_init(FcitxClientSidePopupClass *klass) {
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    // Hook up our overridden functions
    widget_class->snapshot = fcitx_client_side_popup_snapshot;
    G_OBJECT_CLASS(klass)->dispose = fcitx_client_side_popup_dispose;
}

GtkWidget *fcitx_client_side_popup_new(void) {
    return GTK_WIDGET(g_object_new(FCITX_TYPE_CLIENT_SIDE_POPUP, nullptr));
}

namespace fcitx::gtk {

static void ensureFcitxPopupCss() {
    static GtkCssProvider *provider = nullptr;
    auto *display = gdk_display_get_default();
    if (provider || !display) {
        return;
    }

    provider = gtk_css_provider_new();
    const char *css = R"css(
popover.fcitx-popup contents {
    box-shadow: none;
    padding: 0px;
    border-radius: 0px;
    background-color: transparent;
    background-clip: border-box;
    border: none;
}
)css";
    gtk_css_provider_load_from_string(provider, css);
    gtk_style_context_add_provider_for_display(
        display, GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER);
}

Gtk4InputWindow::Gtk4InputWindow(ClassicUIConfig *config, FcitxGClient *client)
    : InputWindow(config, client) {
    rect_.x = rect_.y = rect_.height = rect_.width = 0;
}

Gtk4InputWindow::~Gtk4InputWindow() {
    setParent(nullptr);

    if (!window_) {
        return;
    }
    auto *popup = FCITX_CLIENT_SIDE_POPUP(window_.get());
    popup->inputWindow = nullptr;
    window_.reset();
}

void Gtk4InputWindow::init() {
    if (!window_) {
        ensureFcitxPopupCss();
        // We want to indefinitely keep the window alive through the lifetime of
        // Gtk4InputWindow.
        window_.reset(g_object_ref_sink(fcitx_client_side_popup_new()));
        FCITX_CLIENT_SIDE_POPUP(window_.get())->inputWindow = this;
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
    }
    parent_ = parent;
    if (window_) {
        fcitx_client_side_popup_set_parent(
            FCITX_CLIENT_SIDE_POPUP(window_.get()), parent);
    }
}

void Gtk4InputWindow::setCursorRect(GdkRectangle rect) {
    if (!parent_) {
        return;
    }

    rect_ = rect;
    reposition();
}

void Gtk4InputWindow::update() {
    do {
        if (!visible()) {
            break;
        }

        init();
        syncFontOptions();
        std::tie(width_, height_) = sizeHint();
        if (width_ <= 0 || height_ <= 0) {
            break;
        }

        if (!reposition()) {
            break;
        }
        return;
    } while (false);
    if (window_) {
        gtk_popover_popdown(GTK_POPOVER(window_.get()));
    }
}

bool Gtk4InputWindow::reposition() {
    if (!window_ || !parent_ || width_ <= 0 || height_ <= 0 || !visible()) {
        return false;
    }

    // Check if parent is visible and mapped. If not, we should not pop up the
    // candidate.
    if (!gtk_widget_is_visible(parent_)) {
        return false;
    }

    auto *native = gtk_widget_get_native(parent_);
    if (!native) {
        return false;
    }
    auto *surface = gtk_native_get_surface(GTK_NATIVE(native));
    if (!surface) {
        return false;
    }
    if (!gdk_surface_get_mapped(surface)) {
        return false;
    }

    fcitx_client_side_popup_set_parent(FCITX_CLIENT_SIDE_POPUP(window_.get()),
                                       parent_);
    gtk_widget_set_size_request(window_.get(), width_, height_);
    gtk_popover_set_pointing_to(GTK_POPOVER(window_.get()), &rect_);
    gtk_popover_popup(GTK_POPOVER(window_.get()));
    return true;
}

void Gtk4InputWindow::syncFontOptions() {
    auto *context = gtk_widget_get_pango_context(window_.get());
    if (!context) {
        return;
    }
    pango_cairo_context_set_font_options(
        context_.get(), pango_cairo_context_get_font_options(context));
    dpi_ = pango_cairo_context_get_resolution(context);
    pango_cairo_context_set_resolution(context_.get(), dpi_);
}

void Gtk4InputWindow::motion(double x, double y) {
    if (hover(x, y)) {
        gtk_widget_queue_draw(GTK_WIDGET(window_.get()));
    }
}

void Gtk4InputWindow::leave() {
    auto oldHighlight = highlight();
    hoverIndex_ = -1;
    if (highlight() != oldHighlight) {
        gtk_widget_queue_draw(GTK_WIDGET(window_.get()));
    }
}

bool Gtk4InputWindow::scroll(double /*x*/, double y) {
    if (y == 0) {
        return false;
    }

    scrollDelta_ += y;
    const double delta = 1.0;
    while (scrollDelta_ >= delta) {
        scrollDelta_ -= delta;
        next();
    }
    while (scrollDelta_ <= -delta) {
        scrollDelta_ += delta;
        prev();
    }
    return true;
}

} // namespace fcitx::gtk
