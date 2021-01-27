/*
 * SPDX-FileCopyrightText: 2021~2021 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */
#ifndef _GTK3_FCITXTHEME_H_
#define _GTK3_FCITXTHEME_H_

#include "utils.h"
#include <cairo/cairo.h>
#include <gdk/gdk.h>
#include <memory>
#include <string>
#include <unordered_map>

namespace fcitx::gtk {

enum class Gravity {
    TopLeft,
    TopCenter,
    TopRight,
    CenterLeft,
    Center,
    CenterRight,
    BottomLeft,
    BottomCenter,
    BottomRight
};

struct MarginConfig {
    void load(GKeyFile *file, const char *group);

    int marginLeft, marginRight, marginTop, marginBottom;
};

struct BackgroundImageConfig {
    void load(GKeyFile *file, const char *group);

    std::string image;
    GdkRGBA color;
    GdkRGBA borderColor;
    int borderWidth = 0;
    std::string overlay;
    Gravity gravity;
    int overlayOffsetX = 0;
    int overlayOffsetY = 0;
    bool hideOverlayIfOversize = false;
    MarginConfig margin;
    MarginConfig overlayClipMargin;
};

struct HighlightBackgroundImageConfig : public BackgroundImageConfig {
    void load(GKeyFile *file, const char *group);

    MarginConfig clickMargin;
};

struct ActionImageConfig {
    void load(GKeyFile *file, const char *group);

    std::string image;
    MarginConfig clickMargin;
};

struct InputPanelThemeConfig {
    void load(GKeyFile *file);

    GdkRGBA normalColor;
    GdkRGBA highlightCandidateColor;
    bool enableBlur = false;
    bool fullWidthHighlight = true;
    GdkRGBA highlightColor;
    GdkRGBA highlightBackgroundColor;
    BackgroundImageConfig background;
    HighlightBackgroundImageConfig highlight;
    MarginConfig contentMargin;
    MarginConfig textMargin;
    ActionImageConfig prev;
    ActionImageConfig next;
    MarginConfig blurMargin;
};

class ThemeImage {
public:
    ThemeImage(const std::string &name, const BackgroundImageConfig &cfg);
    ThemeImage(const std::string &name, const ActionImageConfig &cfg);

    operator cairo_surface_t *() const { return image_.get(); }
    auto height() const {
        int height = 1;
        if (image_) {
            height = cairo_image_surface_get_height(image_.get());
        }
        return height <= 0 ? 1 : height;
    }
    auto width() const {
        int width = 1;
        if (image_) {
            width = cairo_image_surface_get_width(image_.get());
        }
        return width <= 0 ? 1 : width;
    }

    auto size() const { return size_; }

    bool valid() const { return valid_; }
    cairo_surface_t *overlay() const { return overlay_.get(); }
    auto overlayWidth() const {
        int width = 1;
        if (overlay_) {
            width = cairo_image_surface_get_width(overlay_.get());
        }
        return width <= 0 ? 1 : width;
    }
    auto overlayHeight() const {
        int height = 1;
        if (overlay_) {
            height = cairo_image_surface_get_height(overlay_.get());
        }
        return height <= 0 ? 1 : height;
    }

private:
    bool valid_ = false;
    std::string currentText_;
    uint32_t size_ = 0;
    UniqueCPtr<cairo_surface_t, cairo_surface_destroy> image_;
    UniqueCPtr<cairo_surface_t, cairo_surface_destroy> overlay_;
};

class Theme : public InputPanelThemeConfig {
public:
    Theme();
    ~Theme();

    void load(const std::string &name);
    const ThemeImage &loadBackground(const BackgroundImageConfig &cfg);
    const ThemeImage &loadAction(const ActionImageConfig &cfg);

    void paint(cairo_t *c, const BackgroundImageConfig &cfg, int width,
               int height, double alpha = 1.0);

    void paint(cairo_t *c, const ActionImageConfig &cfg, double alpha = 1.0);

private:
    std::unordered_map<const BackgroundImageConfig *, ThemeImage>
        backgroundImageTable_;
    std::unordered_map<const ActionImageConfig *, ThemeImage> actionImageTable_;
    std::string name_;
};

class ClassicUIConfig {
public:
    ClassicUIConfig();
    ~ClassicUIConfig();

    void load();

    std::string font_;
    bool vertical_ = false;
    bool wheelForPaging_ = true;
    std::string themeName_ = "default";
    bool useInputMethodLanguageToDisplayText_ = true;
    Theme theme_;

private:
    static void configChangedCallback(GFileMonitor *, GFile *, GFile *,
                                      GFileMonitorEvent event_type,
                                      gpointer user_data) {
        if (event_type != G_FILE_MONITOR_EVENT_CHANGED &&
            event_type != G_FILE_MONITOR_EVENT_CREATED) {
            return;
        }
        static_cast<ClassicUIConfig *>(user_data)->load();
    }

    GObjectUniquePtr<GFileMonitor> monitor_;
};

inline void cairoSetSourceColor(cairo_t *cr, const GdkRGBA &color) {
    cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
}

inline void shrink(cairo_rectangle_int_t &rect, const MarginConfig &margin) {
    int newWidth = rect.width - margin.marginLeft - margin.marginRight;
    int newHeight = rect.height - margin.marginTop - margin.marginBottom;
    if (newWidth < 0) {
        newWidth = 0;
    }
    if (newHeight < 0) {
        newHeight = 0;
    }
    rect.x = rect.x + margin.marginLeft;
    rect.y = rect.y + margin.marginTop;
    rect.width = newWidth;
    rect.height = newHeight;
}

} // namespace fcitx::gtk

#endif // _GTK3_FCITXTHEME_H_
