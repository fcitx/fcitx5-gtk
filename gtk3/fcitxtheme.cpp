/*
 * SPDX-FileCopyrightText: 2021~2021 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */
#include "fcitxtheme.h"
#include <cassert>
#include <fcntl.h>
#include <fmt/format.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gio/gunixinputstream.h>
#include <pango/pangocairo.h>

namespace fcitx::gtk {

namespace {

// Read string value
std::string getValue(GKeyFile *configFile, const char *group, const char *key,
                     const char *defaultValue) {
    UniqueCPtr<gchar, g_free> value(
        g_key_file_get_value(configFile, group, key, nullptr));
    if (!value) {
        return defaultValue;
    }
    std::string valueStr = value.get();
    if (!unescape(valueStr)) {
        return defaultValue;
    }
    return valueStr;
}

// Read bool value
bool getValue(GKeyFile *configFile, const char *group, const char *key,
              bool defaultValue) {
    return getValue(configFile, group, key, defaultValue ? "True" : "False") ==
           "True";
}

// Read gravity enum value
Gravity getValue(GKeyFile *configFile, const char *group, const char *key,
                 Gravity defaultValue) {
    std::string value = getValue(configFile, group, key, "");
    if (value == "Top Left") {
        return Gravity::TopLeft;
    } else if (value == "Top Center") {
        return Gravity::TopCenter;
    } else if (value == "Top Right") {
        return Gravity::TopRight;
    } else if (value == "Center Left") {
        return Gravity::CenterLeft;
    } else if (value == "Center") {
        return Gravity::Center;
    } else if (value == "Center Right") {
        return Gravity::CenterRight;
    } else if (value == "Bottom Left") {
        return Gravity::BottomRight;
    } else if (value == "Bottom Center") {
        return Gravity::BottomCenter;
    } else if (value == "Bottom Right") {
        return Gravity::BottomRight;
    }
    return defaultValue;
}

// Read int value
int getValue(GKeyFile *configFile, const char *group, const char *key,
             int defaultValue) {
    std::string value = getValue(configFile, group, key, "");
    char *eof_int;
    int result = strtol(value.data(), &eof_int, 10);
    if (value.empty() || (*eof_int != '\0' && !g_ascii_isspace(*eof_int))) {
        return defaultValue;
    }
    return result;
}

unsigned short roundColor(unsigned short c) { return c <= 255 ? c : 255; }

unsigned short extendColor(unsigned short c) {
    c = roundColor(c);
    return c << 8 | c;
}

inline unsigned short toHexDigit(char hi, char lo) {
    hi = g_ascii_tolower(hi);
    lo = g_ascii_tolower(lo);
    int dhi = 0, dlo = 0;
    if (hi >= '0' && hi <= '9') {
        dhi = hi - '0';
    } else {
        dhi = hi - 'a' + 10;
    }
    if (lo >= '0' && lo <= '9') {
        dlo = lo - '0';
    } else {
        dlo = lo - 'a' + 10;
    }

    return dhi * 16 + dlo;
}

GdkRGBA makeGdkRGBA(unsigned short r, unsigned short g, unsigned short b,
                    unsigned short a) {
    GdkRGBA result;
    result.red =
        extendColor(r) / double(std::numeric_limits<unsigned short>::max());
    result.green =
        extendColor(g) / double(std::numeric_limits<unsigned short>::max());
    result.blue =
        extendColor(b) / double(std::numeric_limits<unsigned short>::max());
    result.alpha =
        extendColor(a) / double(std::numeric_limits<unsigned short>::max());
    return result;
}

GdkRGBA getValue(GKeyFile *configFile, const char *group, const char *key,
                 GdkRGBA defaultValue) {
    std::string value = getValue(configFile, group, key, "");

    do {
        size_t idx = 0;

        // skip space
        while (value[idx] && g_ascii_isspace(value[idx])) {
            idx++;
        }

        if (value[idx] == '#') {
            // count the digit length
            size_t len = 0;
            const char *digits = &value[idx + 1];
            while (digits[len] &&
                   (g_ascii_isdigit(digits[len]) ||
                    ('A' <= digits[len] && digits[len] <= 'F') |
                        ('a' <= digits[len] && digits[len] <= 'f'))) {
                len++;
            }
            if (len != 8 && len != 6) {
                break;
            }

            unsigned short r, g, b, a;
            r = toHexDigit(digits[0], digits[1]);
            digits += 2;
            g = toHexDigit(digits[0], digits[1]);
            digits += 2;
            b = toHexDigit(digits[0], digits[1]);
            if (len == 8) {
                digits += 2;
                a = toHexDigit(digits[0], digits[1]);
            } else {
                a = 255;
            }

            return makeGdkRGBA(r, g, b, a);
        } else {
            unsigned short r, g, b;
            if (sscanf(value.data(), "%hu %hu %hu", &r, &g, &b) != 3) {
                break;
            }

            return makeGdkRGBA(r, g, b, 255);
        }
    } while (0);
    return defaultValue;
}

cairo_rectangle_int_t intersect(cairo_rectangle_int_t rect1,
                                cairo_rectangle_int_t rect2) {
    cairo_rectangle_int_t tmp;
    tmp.x = std::max(rect1.x, rect2.x);
    tmp.y = std::max(rect1.y, rect2.y);
    auto x2 = std::min(rect1.x + rect1.width, rect2.x + rect2.width);
    auto y2 = std::min(rect1.y + rect1.height, rect2.y + rect2.height);

    if (tmp.x < x2 && tmp.y < y2) {
        tmp.width = x2 - tmp.x;
        tmp.height = y2 - tmp.y;
    } else {
        tmp.x = 0;
        tmp.y = 0;
        tmp.height = 0;
        tmp.width = 0;
    }
    return tmp;
}

UniqueCPtr<char, g_free>
locateXdgFile(const char *user, const char *const *dirs, const char *file) {
    if (!file) {
        return nullptr;
    }

    if (file[0] == '/') {
        return UniqueCPtr<char, g_free>{g_strdup(file)};
    }
    UniqueCPtr<char, g_free> filename(g_build_filename(user, file, nullptr));
    if (filename && g_file_test(filename.get(), G_FILE_TEST_IS_REGULAR)) {
        return filename;
    }

    for (int i = 0; dirs[i]; i++) {
        filename.reset(g_build_filename(dirs[i], file, nullptr));
        if (filename && g_file_test(filename.get(), G_FILE_TEST_IS_REGULAR)) {
            return filename;
        }
    }
    return nullptr;
}

auto locateXdgConfigFile(const char *file) {
    return locateXdgFile(g_get_user_config_dir(), g_get_system_config_dirs(),
                         file);
}

auto locateXdgDataFile(const char *file) {
    return locateXdgFile(g_get_user_data_dir(), g_get_system_data_dirs(), file);
}

template <typename M, typename K>
decltype(&std::declval<M>().begin()->second) findValue(M &&m, K &&key) {
    auto iter = m.find(key);
    if (iter != m.end()) {
        return &iter->second;
    }
    return nullptr;
}

cairo_surface_t *pixBufToCairoSurface(GdkPixbuf *image) {
    cairo_format_t format;
    cairo_surface_t *surface;

    if (gdk_pixbuf_get_n_channels(image) == 3) {
        format = CAIRO_FORMAT_RGB24;
    } else {
        format = CAIRO_FORMAT_ARGB32;
    }

    surface = cairo_image_surface_create(format, gdk_pixbuf_get_width(image),
                                         gdk_pixbuf_get_height(image));

    gint width, height;
    guchar *gdk_pixels, *cairo_pixels;
    int gdk_rowstride, cairo_stride;
    int n_channels;
    int j;

    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        return nullptr;
    }

    cairo_surface_flush(surface);

    width = gdk_pixbuf_get_width(image);
    height = gdk_pixbuf_get_height(image);
    gdk_pixels = gdk_pixbuf_get_pixels(image);
    gdk_rowstride = gdk_pixbuf_get_rowstride(image);
    n_channels = gdk_pixbuf_get_n_channels(image);
    cairo_stride = cairo_image_surface_get_stride(surface);
    cairo_pixels = cairo_image_surface_get_data(surface);

    for (j = height; j; j--) {
        guchar *p = gdk_pixels;
        guchar *q = cairo_pixels;

        if (n_channels == 3) {
            guchar *end = p + 3 * width;

            while (p < end) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
                q[0] = p[2];
                q[1] = p[1];
                q[2] = p[0];
                q[3] = 0xFF;
#else
                q[0] = 0xFF;
                q[1] = p[0];
                q[2] = p[1];
                q[3] = p[2];
#endif
                p += 3;
                q += 4;
            }
        } else {
            guchar *end = p + 4 * width;
            guint t1, t2, t3;

#define MULT(d, c, a, t)                                                       \
    G_STMT_START {                                                             \
        t = c * a + 0x80;                                                      \
        d = ((t >> 8) + t) >> 8;                                               \
    }                                                                          \
    G_STMT_END

            while (p < end) {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
                MULT(q[0], p[2], p[3], t1);
                MULT(q[1], p[1], p[3], t2);
                MULT(q[2], p[0], p[3], t3);
                q[3] = p[3];
#else
                q[0] = p[3];
                MULT(q[1], p[0], p[3], t1);
                MULT(q[2], p[1], p[3], t2);
                MULT(q[3], p[2], p[3], t3);
#endif

                p += 4;
                q += 4;
            }

#undef MULT
        }

        gdk_pixels += gdk_rowstride;
        cairo_pixels += cairo_stride;
    }

    cairo_surface_mark_dirty(surface);
    return surface;
}

cairo_surface_t *loadImage(const char *filename) {
    if (!filename) {
        return nullptr;
    }

    if (g_str_has_suffix(filename, ".png")) {
        auto *surface = cairo_image_surface_create_from_png(filename);
        if (!surface) {
            return nullptr;
        }
        if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
            g_clear_pointer(&surface, cairo_surface_destroy);
            return nullptr;
        }
        return surface;
    }

    auto *image = gdk_pixbuf_new_from_file(filename, nullptr);
    if (!image) {
        return nullptr;
    }

    auto *surface = pixBufToCairoSurface(image);

    g_object_unref(image);

    return surface;
}

} // namespace

ThemeImage::ThemeImage(const std::string &name,
                       const BackgroundImageConfig &cfg) {
    if (!cfg.image.empty()) {
        UniqueCPtr<gchar, g_free> filename(g_build_filename(
            "fcitx5/themes", name.data(), cfg.image.data(), nullptr));
        auto imageFile = locateXdgDataFile(filename.get());
        image_.reset(loadImage(imageFile.get()));
        if (image_ &&
            cairo_surface_status(image_.get()) != CAIRO_STATUS_SUCCESS) {
            image_.reset();
        }
        valid_ = image_ != nullptr;
    }

    if (!cfg.overlay.empty()) {
        UniqueCPtr<gchar, g_free> filename(g_build_filename(
            "fcitx5/themes", name.data(), cfg.overlay.data(), nullptr));
        auto imageFile = locateXdgDataFile(filename.get());
        overlay_.reset(loadImage(imageFile.get()));
        if (overlay_ &&
            cairo_surface_status(overlay_.get()) != CAIRO_STATUS_SUCCESS) {
            overlay_.reset();
        }
    }

    if (!image_) {
        auto width = cfg.margin.marginLeft + cfg.margin.marginRight + 1;
        auto height = cfg.margin.marginTop + cfg.margin.marginBottom + 1;

        auto borderWidth = std::min(
            {cfg.borderWidth, cfg.margin.marginLeft, cfg.margin.marginRight,
             cfg.margin.marginTop, cfg.margin.marginBottom});
        borderWidth = std::max(0, borderWidth);

        image_.reset(
            cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height));
        auto *cr = cairo_create(image_.get());
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
        if (borderWidth) {
            cairoSetSourceColor(cr, cfg.borderColor);
            cairo_paint(cr);
        }

        cairo_rectangle(cr, borderWidth, borderWidth, width - borderWidth * 2,
                        height - borderWidth * 2);
        cairo_clip(cr);
        cairoSetSourceColor(cr, cfg.color);
        cairo_paint(cr);
        cairo_destroy(cr);
    }
}

ThemeImage::ThemeImage(const std::string &name, const ActionImageConfig &cfg) {
    if (!cfg.image.empty()) {
        UniqueCPtr<gchar, g_free> filename(g_build_filename(
            "fcitx5/themes", name.data(), cfg.image.data(), nullptr));
        auto imageFile = locateXdgDataFile(filename.get());
        image_.reset(loadImage(imageFile.get()));
        if (image_ &&
            cairo_surface_status(image_.get()) != CAIRO_STATUS_SUCCESS) {
            image_.reset();
        }
        valid_ = image_ != nullptr;
    }
}

Theme::Theme() {}

Theme::~Theme() {}

const ThemeImage &Theme::loadBackground(const BackgroundImageConfig &cfg) {
    if (auto *image = findValue(backgroundImageTable_, &cfg)) {
        return *image;
    }

    auto result = backgroundImageTable_.emplace(
        std::piecewise_construct, std::forward_as_tuple(&cfg),
        std::forward_as_tuple(name_, cfg));
    assert(result.second);
    return result.first->second;
}

const ThemeImage &Theme::loadAction(const ActionImageConfig &cfg) {
    if (auto *image = findValue(actionImageTable_, &cfg)) {
        return *image;
    }

    auto result = actionImageTable_.emplace(std::piecewise_construct,
                                            std::forward_as_tuple(&cfg),
                                            std::forward_as_tuple(name_, cfg));
    assert(result.second);
    return result.first->second;
}

void Theme::paint(cairo_t *c, const BackgroundImageConfig &cfg, int width,
                  int height, double alpha) {
    const ThemeImage &image = loadBackground(cfg);
    auto marginTop = cfg.margin.marginTop;
    auto marginBottom = cfg.margin.marginBottom;
    auto marginLeft = cfg.margin.marginLeft;
    auto marginRight = cfg.margin.marginRight;
    int resizeHeight =
        cairo_image_surface_get_height(image) - marginTop - marginBottom;
    int resizeWidth =
        cairo_image_surface_get_width(image) - marginLeft - marginRight;

    if (resizeHeight <= 0) {
        resizeHeight = 1;
    }

    if (resizeWidth <= 0) {
        resizeWidth = 1;
    }

    if (height < 0) {
        height = resizeHeight;
    }

    if (width < 0) {
        width = resizeWidth;
    }

    cairo_save(c);

    /*
     * 7 8 9
     * 4 5 6
     * 1 2 3
     */

    if (marginLeft && marginBottom) {
        /* part 1 */
        cairo_save(c);
        cairo_translate(c, 0, height - marginBottom);
        cairo_set_source_surface(c, image, 0, -marginTop - resizeHeight);
        cairo_rectangle(c, 0, 0, marginLeft, marginBottom);
        cairo_clip(c);
        cairo_paint_with_alpha(c, alpha);
        cairo_restore(c);
    }

    if (marginRight && marginBottom) {
        /* part 3 */
        cairo_save(c);
        cairo_translate(c, width - marginRight, height - marginBottom);
        cairo_set_source_surface(c, image, -marginLeft - resizeWidth,
                                 -marginTop - resizeHeight);
        cairo_rectangle(c, 0, 0, marginRight, marginBottom);
        cairo_clip(c);
        cairo_paint_with_alpha(c, alpha);
        cairo_restore(c);
    }

    if (marginLeft && marginTop) {
        /* part 7 */
        cairo_save(c);
        cairo_set_source_surface(c, image, 0, 0);
        cairo_rectangle(c, 0, 0, marginLeft, marginTop);
        cairo_clip(c);
        cairo_paint_with_alpha(c, alpha);
        cairo_restore(c);
    }

    if (marginRight && marginTop) {
        /* part 9 */
        cairo_save(c);
        cairo_translate(c, width - marginRight, 0);
        cairo_set_source_surface(c, image, -marginLeft - resizeWidth, 0);
        cairo_rectangle(c, 0, 0, marginRight, marginTop);
        cairo_clip(c);
        cairo_paint_with_alpha(c, alpha);
        cairo_restore(c);
    }

    /* part 2 & 8 */
    if (marginTop) {
        cairo_save(c);
        cairo_translate(c, marginLeft, 0);
        cairo_scale(
            c, (double)(width - marginLeft - marginRight) / (double)resizeWidth,
            1);
        cairo_set_source_surface(c, image, -marginLeft, 0);
        cairo_rectangle(c, 0, 0, resizeWidth, marginTop);
        cairo_clip(c);
        cairo_paint_with_alpha(c, alpha);
        cairo_restore(c);
    }

    if (marginBottom) {
        cairo_save(c);
        cairo_translate(c, marginLeft, height - marginBottom);
        cairo_scale(
            c, (double)(width - marginLeft - marginRight) / (double)resizeWidth,
            1);
        cairo_set_source_surface(c, image, -marginLeft,
                                 -marginTop - resizeHeight);
        cairo_rectangle(c, 0, 0, resizeWidth, marginBottom);
        cairo_clip(c);
        cairo_paint_with_alpha(c, alpha);
        cairo_restore(c);
    }

    /* part 4 & 6 */
    if (marginLeft) {
        cairo_save(c);
        cairo_translate(c, 0, marginTop);
        cairo_scale(c, 1,
                    (double)(height - marginTop - marginBottom) /
                        (double)resizeHeight);
        cairo_set_source_surface(c, image, 0, -marginTop);
        cairo_rectangle(c, 0, 0, marginLeft, resizeHeight);
        cairo_clip(c);
        cairo_paint_with_alpha(c, alpha);
        cairo_restore(c);
    }

    if (marginRight) {
        cairo_save(c);
        cairo_translate(c, width - marginRight, marginTop);
        cairo_scale(c, 1,
                    (double)(height - marginTop - marginBottom) /
                        (double)resizeHeight);
        cairo_set_source_surface(c, image, -marginLeft - resizeWidth,
                                 -marginTop);
        cairo_rectangle(c, 0, 0, marginRight, resizeHeight);
        cairo_clip(c);
        cairo_paint_with_alpha(c, alpha);
        cairo_restore(c);
    }

    /* part 5 */
    {
        double scaleX = 1.0, scaleY = 1.0;
        scaleX =
            (double)(width - marginLeft - marginRight) / (double)resizeWidth;

        scaleY =
            (double)(height - marginTop - marginBottom) / (double)resizeHeight;

        cairo_save(c);
        cairo_translate(c, marginLeft, marginTop);
        cairo_scale(c, scaleX, scaleY);
        cairo_set_source_surface(c, image, -marginLeft, -marginTop);
        cairo_pattern_set_filter(cairo_get_source(c), CAIRO_FILTER_NEAREST);
        int w = resizeWidth, h = resizeHeight;

        cairo_rectangle(c, 0, 0, w, h);
        cairo_clip(c);
        cairo_paint_with_alpha(c, alpha);
        cairo_restore(c);
    }
    cairo_restore(c);

    if (!image.overlay()) {
        return;
    }

    auto clipWidth = width - cfg.overlayClipMargin.marginLeft -
                     cfg.overlayClipMargin.marginRight;
    auto clipHeight = height - cfg.overlayClipMargin.marginTop -
                      cfg.overlayClipMargin.marginBottom;
    if (clipWidth <= 0 || clipHeight <= 0) {
        return;
    }
    cairo_rectangle_int_t clipRect;
    clipRect.x = cfg.overlayClipMargin.marginLeft;
    clipRect.y = cfg.overlayClipMargin.marginTop;
    clipRect.width = clipWidth;
    clipRect.height = clipHeight;
    ;

    int x = 0, y = 0;
    switch (cfg.gravity) {
    case Gravity::TopLeft:
    case Gravity::CenterLeft:
    case Gravity::BottomLeft:
        x = cfg.overlayOffsetX;
        break;
    case Gravity::TopCenter:
    case Gravity::Center:
    case Gravity::BottomCenter:
        x = (width - image.overlayWidth()) / 2 + cfg.overlayOffsetX;
        break;
    case Gravity::TopRight:
    case Gravity::CenterRight:
    case Gravity::BottomRight:
        x = width - image.overlayWidth() - cfg.overlayOffsetX;
        break;
    }
    switch (cfg.gravity) {
    case Gravity::TopLeft:
    case Gravity::TopCenter:
    case Gravity::TopRight:
        y = cfg.overlayOffsetY;
        break;
    case Gravity::CenterLeft:
    case Gravity::Center:
    case Gravity::CenterRight:
        y = (height - image.overlayHeight()) / 2 + cfg.overlayOffsetY;
        break;
    case Gravity::BottomLeft:
    case Gravity::BottomCenter:
    case Gravity::BottomRight:
        y = height - image.overlayHeight() - cfg.overlayOffsetY;
        break;
    }

    cairo_rectangle_int_t rect;
    rect.x = x;
    rect.y = y;
    rect.width = image.overlayWidth();
    rect.height = image.overlayHeight();
    auto finalRect = intersect(rect, clipRect);
    if (finalRect.width == 0 || finalRect.height == 0) {
        return;
    }

    if (cfg.hideOverlayIfOversize && !rectContains(clipRect, rect)) {
        return;
    }

    cairo_save(c);
    cairo_set_operator(c, CAIRO_OPERATOR_OVER);
    cairo_translate(c, finalRect.x, finalRect.y);
    cairo_set_source_surface(c, image.overlay(), x - finalRect.x,
                             y - finalRect.y);
    cairo_rectangle(c, 0, 0, finalRect.width, finalRect.height);
    cairo_clip(c);
    cairo_paint_with_alpha(c, alpha);
    cairo_restore(c);
}

void Theme::paint(cairo_t *c, const ActionImageConfig &cfg, double alpha) {
    const ThemeImage &image = loadAction(cfg);
    int height = cairo_image_surface_get_height(image);
    int width = cairo_image_surface_get_width(image);

    cairo_save(c);
    cairo_set_source_surface(c, image, 0, 0);
    cairo_rectangle(c, 0, 0, width, height);
    cairo_clip(c);
    cairo_paint_with_alpha(c, alpha);
    cairo_restore(c);
}

void Theme::load(const std::string &name) {
    backgroundImageTable_.clear();
    actionImageTable_.clear();
    name_ = name;

    UniqueCPtr<GKeyFile, g_key_file_unref> configFile{g_key_file_new()};
    UniqueCPtr<gchar, g_free> filename(
        g_build_filename("fcitx5/themes", name.data(), "theme.conf", nullptr));
    bool result = g_key_file_load_from_data_dirs(
        configFile.get(), filename.get(), nullptr, G_KEY_FILE_NONE, nullptr);
    if (!result) {
        result = g_key_file_load_from_data_dirs(
            configFile.get(), "fcitx5/themes/default/theme.conf", nullptr,
            G_KEY_FILE_NONE, nullptr);
        name_ = "default";
    }

    InputPanelThemeConfig::load(configFile.get());
    if (!result) {
        // Build a default theme like setup, so we have some default value for
        // flatpak.
        contentMargin = MarginConfig{2, 2, 2, 2};
        textMargin = MarginConfig{5, 5, 5, 5};
        highlight.color = highlightBackgroundColor;
        highlight.borderColor = highlightBackgroundColor;
        highlight.margin = textMargin;
        background.borderColor = highlightBackgroundColor;
        background.margin = contentMargin;
        background.borderWidth = 2;
    }
}

void InputPanelThemeConfig::load(GKeyFile *file) {
    normalColor =
        getValue(file, "InputPanel", "NormalColor", makeGdkRGBA(0, 0, 0, 255));
    highlightCandidateColor =
        getValue(file, "InputPanel", "HighlightCandidateColor",
                 makeGdkRGBA(255, 255, 255, 255));
    enableBlur = getValue(file, "InputPanel", "EnableBlur", false);
    fullWidthHighlight =
        getValue(file, "InputPanel", "FullWidthHighlight", false);
    highlightColor = getValue(file, "InputPanel", "HighlightColor",
                              makeGdkRGBA(255, 255, 255, 255));
    highlightBackgroundColor =
        getValue(file, "InputPanel", "HighlightBackgroundColor",
                 makeGdkRGBA(0xa5, 0xa5, 0xa5, 255));
    background.load(file, "InputPanel/Background");
    highlight.load(file, "InputPanel/Highlight");
    contentMargin.load(file, "InputPanel/ContentMargin");
    textMargin.load(file, "InputPanel/TextMargin");
    prev.load(file, "InputPanel/PrevPage");
    next.load(file, "InputPanel/NextPage");
    blurMargin.load(file, "InputPanel/BlurMargin");
}

void MarginConfig::load(GKeyFile *file, const char *group) {
    marginLeft = getValue(file, group, "Left", 0);
    marginRight = getValue(file, group, "Right", 0);
    marginTop = getValue(file, group, "Top", 0);
    marginBottom = getValue(file, group, "Bottom", 0);
}

void HighlightBackgroundImageConfig::load(GKeyFile *file, const char *group) {
    BackgroundImageConfig::load(file, group);
    std::string path = group;
    path.append("/HighlightClickMargin");
    clickMargin.load(file, path.data());
}

void ActionImageConfig::load(GKeyFile *file, const char *group) {
    std::string path = group;
    path.append("/ClickMargin");
    image = getValue(file, group, "Image", "");
    clickMargin.load(file, path.data());
}

void BackgroundImageConfig::load(GKeyFile *file, const char *group) {
    image = getValue(file, group, "Image", "");
    overlay = getValue(file, group, "Overlay", "");
    color = getValue(file, group, "Color", {1, 1, 1, 1});
    borderColor = getValue(file, group, "BorderColor", {1, 1, 1, 0});
    borderWidth = getValue(file, group, "BorderWidth", 0);
    gravity = getValue(file, group, "Gravity", Gravity::TopLeft);
    overlayOffsetX = getValue(file, group, "OverlayOffsetX", 0);
    overlayOffsetY = getValue(file, group, "OverlayOffsetY", 0);
    hideOverlayIfOversize =
        getValue(file, group, "HideOverlayIfOversize", false);
    margin.load(file, (std::string(group) + "/Margin").data());
    overlayClipMargin.load(file,
                           (std::string(group) + "/OverlayClipMargin").data());
}

ClassicUIConfig::ClassicUIConfig() {
    UniqueCPtr<char, g_free> filename(g_build_filename(
        g_get_user_config_dir(), "fcitx5/conf/classicui.conf", nullptr));
    GObjectUniquePtr<GFile> file(g_file_new_for_path(filename.get()));
    monitor_.reset(
        g_file_monitor_file(file.get(), G_FILE_MONITOR_NONE, nullptr, nullptr));

    g_signal_connect(monitor_.get(), "changed",
                     G_CALLBACK(&ClassicUIConfig::configChangedCallback), this);

    load();
}

ClassicUIConfig::~ClassicUIConfig() {
    if (monitor_) {
        g_signal_handlers_disconnect_by_func(
            monitor_.get(),
            reinterpret_cast<gpointer>(
                G_CALLBACK(&ClassicUIConfig::configChangedCallback)),
            this);
    }
}

void ClassicUIConfig::load() {
    UniqueCPtr<GKeyFile, g_key_file_unref> configFile{g_key_file_new()};
    auto file = locateXdgConfigFile("fcitx5/conf/classicui.conf");
    gchar *content = nullptr;
    if (file && g_file_get_contents(file.get(), &content, nullptr, nullptr)) {
        UniqueCPtr<gchar, g_free> ini(g_strdup_printf("[Group]\n%s", content));
        g_free(content);
        g_key_file_load_from_data(configFile.get(), ini.get(), -1,
                                  G_KEY_FILE_NONE, nullptr);
    }

    font_ = getValue(configFile.get(), "Group", "Font", "Sans 10");
    vertical_ = getValue(configFile.get(), "Group", "Vertical Candidate List",
                         "False") == "True";
    wheelForPaging_ =
        getValue(configFile.get(), "Group", "WheelForPaging", "True") == "True";
    themeName_ = getValue(configFile.get(), "Group", "Theme", "default");
    useInputMethodLanguageToDisplayText_ = getValue(
        configFile.get(), "Group", "UseInputMethodLangaugeToDisplayText", true);

    theme_.load(themeName_);
}

} // namespace fcitx::gtk
