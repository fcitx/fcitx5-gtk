/*
 * SPDX-FileCopyrightText: 2017-2017 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */
#include "inputwindow.h"
#include "fcitxtheme.h"
#include <fcitx-gclient/fcitxgclient.h>
#include <functional>
#include <initializer_list>
#include <limits>
#include <pango/pangocairo.h>

namespace fcitx::gtk {

size_t textLength(GPtrArray *array) {
    size_t length = 0;
    for (unsigned int i = 0; i < array->len; i++) {
        auto *preedit =
            static_cast<FcitxGPreeditItem *>(g_ptr_array_index(array, i));
        length += strlen(preedit->string);
    }
    return length;
}

auto newPangoLayout(PangoContext *context) {
    GObjectUniquePtr<PangoLayout> ptr(pango_layout_new(context));
    pango_layout_set_single_paragraph_mode(ptr.get(), false);
    return ptr;
}

InputWindow::InputWindow(ClassicUIConfig *config, FcitxGClient *client)
    : config_(config), client_(FCITX_G_CLIENT(g_object_ref(client))) {
    auto *fontMap = pango_cairo_font_map_get_default();
    context_.reset(pango_font_map_create_context(fontMap));
    upperLayout_ = newPangoLayout(context_.get());
    lowerLayout_ = newPangoLayout(context_.get());

    auto update_ui_callback =
        [](FcitxGClient *, GPtrArray *preedit, int cursor_pos, GPtrArray *auxUp,
           GPtrArray *auxDown, GPtrArray *candidates, int highlight,
           int layoutHint, gboolean hasPrev, gboolean hasNext,
           void *user_data) {
            auto that = static_cast<InputWindow *>(user_data);
            that->updateUI(preedit, cursor_pos, auxUp, auxDown, candidates,
                           highlight, layoutHint, hasPrev, hasNext);
        };

    auto update_im_callback = [](FcitxGClient *, gchar *, gchar *,
                                 gchar *langCode, void *user_data) {
        auto that = static_cast<InputWindow *>(user_data);
        that->updateLanguage(langCode);
    };

    g_signal_connect(client_.get(), "update-client-side-ui",
                     G_CALLBACK(+update_ui_callback), this);

    g_signal_connect(client_.get(), "current-im",
                     G_CALLBACK(+update_im_callback), this);
}

InputWindow::~InputWindow() {
    g_signal_handlers_disconnect_by_data(client_.get(), this);
}

void InputWindow::insertAttr(PangoAttrList *attrList,
                             FcitxTextFormatFlag format, int start, int end,
                             bool highlight) const {
    if (format & FcitxTextFormatFlag_Underline) {
        auto *attr = pango_attr_underline_new(PANGO_UNDERLINE_SINGLE);
        attr->start_index = start;
        attr->end_index = end;
        pango_attr_list_insert(attrList, attr);
    }
    if (format & FcitxTextFormatFlag_Italic) {
        auto *attr = pango_attr_style_new(PANGO_STYLE_ITALIC);
        attr->start_index = start;
        attr->end_index = end;
        pango_attr_list_insert(attrList, attr);
    }
    if (format & FcitxTextFormatFlag_Strike) {
        auto *attr = pango_attr_strikethrough_new(true);
        attr->start_index = start;
        attr->end_index = end;
        pango_attr_list_insert(attrList, attr);
    }
    if (format & FcitxTextFormatFlag_Bold) {
        auto *attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
        attr->start_index = start;
        attr->end_index = end;
        pango_attr_list_insert(attrList, attr);
    }
    GdkRGBA color = (format & FcitxTextFormatFlag_HighLight)
                        ? config_->theme_.highlightColor
                        : (highlight ? config_->theme_.highlightCandidateColor
                                     : config_->theme_.normalColor);
    const auto scale = std::numeric_limits<uint16_t>::max();
    auto *attr = pango_attr_foreground_new(
        color.red * scale, color.green * scale, color.blue * scale);
    attr->start_index = start;
    attr->end_index = end;
    pango_attr_list_insert(attrList, attr);

    if (color.alpha != 1.0) {
        auto *alphaAttr = pango_attr_foreground_alpha_new(color.alpha * scale);
        alphaAttr->start_index = start;
        alphaAttr->end_index = end;
        pango_attr_list_insert(attrList, alphaAttr);
    }

    auto background = config_->theme_.highlightBackgroundColor;
    if ((format & FcitxTextFormatFlag_HighLight) && background.alpha > 0) {
        attr = pango_attr_background_new(background.red * scale,
                                         background.green * scale,
                                         background.blue * scale);
        attr->start_index = start;
        attr->end_index = end;
        pango_attr_list_insert(attrList, attr);

        if (background.alpha != 1.0) {
            auto *alphaAttr =
                pango_attr_background_alpha_new(background.alpha * scale);
            alphaAttr->start_index = start;
            alphaAttr->end_index = end;
            pango_attr_list_insert(attrList, alphaAttr);
        }
    }
}

void InputWindow::appendText(std::string &s, PangoAttrList *attrList,
                             PangoAttrList *highlightAttrList,
                             const GPtrArray *text) {
    for (size_t i = 0, e = text->len; i < e; i++) {
        auto *item =
            static_cast<FcitxGPreeditItem *>(g_ptr_array_index(text, i));
        appendText(s, attrList, highlightAttrList, item->string, item->type);
    }
}

void InputWindow::appendText(std::string &s, PangoAttrList *attrList,
                             PangoAttrList *highlightAttrList,
                             const gchar *text, int format) {
    auto start = s.size();
    s.append(text);
    auto end = s.size();
    if (start == end) {
        return;
    }
    const auto formatFlags = static_cast<FcitxTextFormatFlag>(format);
    insertAttr(attrList, formatFlags, start, end, false);
    if (highlightAttrList) {
        insertAttr(highlightAttrList, formatFlags, start, end, true);
    }
}

void InputWindow::resizeCandidates(size_t n) {
    while (labelLayouts_.size() < n) {
        labelLayouts_.emplace_back(newPangoLayout(context_.get()));
    }
    while (candidateLayouts_.size() < n) {
        candidateLayouts_.emplace_back(newPangoLayout(context_.get()));
    }
    for (auto *attrLists :
         {&labelAttrLists_, &candidateAttrLists_, &highlightLabelAttrLists_,
          &highlightCandidateAttrLists_}) {
        while (attrLists->size() < n) {
            attrLists->emplace_back(pango_attr_list_new());
        }
    }

    nCandidates_ = n;
}
void InputWindow::setLanguageAttr(size_t size, PangoAttrList *attrList,
                                  PangoAttrList *highlightAttrList) {
    if (!config_->useInputMethodLanguageToDisplayText_ || language_.empty()) {
        return;
    }
    if (auto language = pango_language_from_string(language_.c_str())) {
        if (attrList) {
            auto attr = pango_attr_language_new(language);
            attr->start_index = 0;
            attr->end_index = size;
            pango_attr_list_insert(attrList, attr);
        }
        if (highlightAttrList) {
            auto attr = pango_attr_language_new(language);
            attr->start_index = 0;
            attr->end_index = size;
            pango_attr_list_insert(highlightAttrList, attr);
        }
    }
}

void InputWindow::setTextToLayout(
    PangoLayout *layout, PangoAttrListUniquePtr *attrList,
    PangoAttrListUniquePtr *highlightAttrList,
    std::initializer_list<const GPtrArray *> texts) {
    auto *newAttrList = pango_attr_list_new();
    if (attrList) {
        // PangoAttrList does not have "clear()". So when we set new text,
        // we need to create a new one and get rid of old one.
        // We keep a ref to the attrList.
        attrList->reset(pango_attr_list_ref(newAttrList));
    }
    PangoAttrList *newHighlightAttrList = nullptr;
    if (highlightAttrList) {
        newHighlightAttrList = pango_attr_list_new();
        highlightAttrList->reset(newHighlightAttrList);
    }
    std::string line;
    for (const auto &text : texts) {
        appendText(line, newAttrList, newHighlightAttrList, text);
    }

    setLanguageAttr(line.size(), newAttrList, newHighlightAttrList);

    pango_layout_set_text(layout, line.c_str(), line.size());
    pango_layout_set_attributes(layout, newAttrList);
    pango_attr_list_unref(newAttrList);
}

void InputWindow::setTextToLayout(PangoLayout *layout,
                                  PangoAttrListUniquePtr *attrList,
                                  PangoAttrListUniquePtr *highlightAttrList,
                                  const gchar *text) {
    auto *newAttrList = pango_attr_list_new();
    if (attrList) {
        // PangoAttrList does not have "clear()". So when we set new text,
        // we need to create a new one and get rid of old one.
        // We keep a ref to the attrList.
        attrList->reset(pango_attr_list_ref(newAttrList));
    }
    PangoAttrList *newHighlightAttrList = nullptr;
    if (highlightAttrList) {
        newHighlightAttrList = pango_attr_list_new();
        highlightAttrList->reset(newHighlightAttrList);
    }
    std::string line;
    appendText(line, newAttrList, newHighlightAttrList, text);

    pango_layout_set_text(layout, line.c_str(), line.size());
    pango_layout_set_attributes(layout, newAttrList);
    pango_attr_list_unref(newAttrList);
}

void InputWindow::updateUI(GPtrArray *preedit, int cursor_pos, GPtrArray *auxUp,
                           GPtrArray *auxDown, GPtrArray *candidates,
                           int highlight, int layoutHint, bool hasPrev,
                           bool hasNext) {
    // | aux up | preedit
    // | aux down
    // | 1 candidate | 2 ...
    // or
    // | aux up | preedit
    // | aux down
    // | candidate 1
    // | candidate 2
    // | candidate 3

    cursor_ = -1;
    pango_layout_set_single_paragraph_mode(upperLayout_.get(), true);
    setTextToLayout(upperLayout_.get(), nullptr, nullptr, {auxUp, preedit});
    if (cursor_pos >= 0 &&
        static_cast<size_t>(cursor_pos) <= textLength(preedit)) {

        cursor_ = cursor_pos + textLength(auxUp);
    }

    setTextToLayout(lowerLayout_.get(), nullptr, nullptr, {auxDown});

    // Count non-placeholder candidates.
    resizeCandidates(candidates->len);

    candidateIndex_ = highlight;
    for (int i = 0, e = candidates->len; i < e; i++) {
        auto *candidate = static_cast<FcitxGCandidateItem *>(
            g_ptr_array_index(candidates, i));
        setTextToLayout(labelLayouts_[i].get(), &labelAttrLists_[i],
                        &highlightLabelAttrLists_[i], candidate->label);
        setTextToLayout(candidateLayouts_[i].get(), &candidateAttrLists_[i],
                        &highlightCandidateAttrLists_[i], candidate->candidate);
    }

    layoutHint_ = static_cast<FcitxCandidateLayoutHint>(layoutHint);
    hasPrev_ = hasPrev;
    hasNext_ = hasNext;

    visible_ = nCandidates_ ||
               pango_layout_get_character_count(upperLayout_.get()) ||
               pango_layout_get_character_count(lowerLayout_.get());

    update();
}

void InputWindow::updateLanguage(const char *language) { language_ = language; }

std::pair<unsigned int, unsigned int> InputWindow::sizeHint() {
    auto *fontDesc = pango_font_description_from_string(config_->font_.data());
    pango_context_set_font_description(context_.get(), fontDesc);
    pango_cairo_context_set_resolution(context_.get(), dpi_);
    pango_font_description_free(fontDesc);
    pango_layout_context_changed(upperLayout_.get());
    pango_layout_context_changed(lowerLayout_.get());
    for (size_t i = 0; i < nCandidates_; i++) {
        pango_layout_context_changed(labelLayouts_[i].get());
        pango_layout_context_changed(candidateLayouts_[i].get());
    }
    auto *metrics = pango_context_get_metrics(
        context_.get(), pango_context_get_font_description(context_.get()),
        pango_context_get_language(context_.get()));
    auto minH = pango_font_metrics_get_ascent(metrics) +
                pango_font_metrics_get_descent(metrics);
    pango_font_metrics_unref(metrics);
    minH = PANGO_PIXELS(minH);

    size_t width = 0;
    size_t height = 0;
    auto updateIfLarger = [](size_t &m, size_t n) {
        if (n > m) {
            m = n;
        }
    };
    int w, h;

    const auto &textMargin = config_->theme_.textMargin;
    auto extraW = textMargin.marginLeft + textMargin.marginRight;
    auto extraH = textMargin.marginTop + textMargin.marginBottom;
    if (pango_layout_get_character_count(upperLayout_.get())) {
        pango_layout_get_pixel_size(upperLayout_.get(), &w, &h);
        height += std::max(minH, h) + extraH;
        updateIfLarger(width, w + extraW);
    }
    if (pango_layout_get_character_count(lowerLayout_.get())) {
        pango_layout_get_pixel_size(lowerLayout_.get(), &w, &h);
        height += std::max(minH, h) + extraH;
        updateIfLarger(width, w + extraW);
    }

    bool vertical = config_->vertical_;
    if (layoutHint_ == FcitxCandidateLayoutHint::Vertical) {
        vertical = true;
    } else if (layoutHint_ == FcitxCandidateLayoutHint::Horizontal) {
        vertical = false;
    }

    size_t wholeH = 0, wholeW = 0;
    for (size_t i = 0; i < nCandidates_; i++) {
        size_t candidateW = 0, candidateH = 0;
        if (pango_layout_get_character_count(labelLayouts_[i].get())) {
            pango_layout_get_pixel_size(labelLayouts_[i].get(), &w, &h);
            candidateW += w;
            updateIfLarger(candidateH, std::max(minH, h) + extraH);
        }
        if (pango_layout_get_character_count(candidateLayouts_[i].get())) {
            pango_layout_get_pixel_size(candidateLayouts_[i].get(), &w, &h);
            candidateW += w;
            updateIfLarger(candidateH, std::max(minH, h) + extraH);
        }
        candidateW += extraW;

        if (vertical) {
            wholeH += candidateH;
            updateIfLarger(wholeW, candidateW);
        } else {
            wholeW += candidateW;
            updateIfLarger(wholeH, candidateH);
        }
    }
    updateIfLarger(width, wholeW);
    candidatesHeight_ = wholeH;
    height += wholeH;
    const auto &margin = config_->theme_.contentMargin;
    width += margin.marginLeft + margin.marginRight;
    height += margin.marginTop + margin.marginBottom;

    if (nCandidates_ && (hasPrev_ || hasNext_)) {
        const auto &prev = config_->theme_.loadAction(config_->theme_.prev);
        const auto &next = config_->theme_.loadAction(config_->theme_.next);
        if (prev.valid() && next.valid()) {
            width += prev.width() + next.width();
        }
    }

    return {width, height};
}

static void prepareLayout(cairo_t *cr, PangoLayout *layout) {
    const PangoMatrix *matrix;

    matrix = pango_context_get_matrix(pango_layout_get_context(layout));

    if (matrix) {
        cairo_matrix_t cairo_matrix;

        cairo_matrix_init(&cairo_matrix, matrix->xx, matrix->yx, matrix->xy,
                          matrix->yy, matrix->x0, matrix->y0);

        cairo_transform(cr, &cairo_matrix);
    }
}

static void renderLayout(cairo_t *cr, PangoLayout *layout, int x, int y) {
    auto context = pango_layout_get_context(layout);
    auto *metrics = pango_context_get_metrics(
        context, pango_context_get_font_description(context),
        pango_context_get_language(context));
    auto ascent = pango_font_metrics_get_ascent(metrics);
    pango_font_metrics_unref(metrics);
    auto baseline = pango_layout_get_baseline(layout);
    auto yOffset = PANGO_PIXELS(ascent - baseline);
    cairo_save(cr);

    cairo_move_to(cr, x, y + yOffset);
    prepareLayout(cr, layout);
    pango_cairo_show_layout(cr, layout);

    cairo_restore(cr);
}

void InputWindow::paint(cairo_t *cr, unsigned int width, unsigned int height) {
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    config_->theme_.paint(cr, config_->theme_.background, width, height);
    const auto &margin = config_->theme_.contentMargin;
    const auto &textMargin = config_->theme_.textMargin;
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_save(cr);

    prevRegion_ = cairo_rectangle_int_t{0, 0, 0, 0};
    nextRegion_ = cairo_rectangle_int_t{0, 0, 0, 0};
    if (nCandidates_ && (hasPrev_ || hasNext_)) {
        const auto &prev = config_->theme_.loadAction(config_->theme_.prev);
        const auto &next = config_->theme_.loadAction(config_->theme_.next);
        if (prev.valid() && next.valid()) {
            cairo_save(cr);
            nextRegion_.x = width - margin.marginRight - next.width();
            nextRegion_.y = height - margin.marginBottom - next.height();
            nextRegion_.width = next.width();
            nextRegion_.height = next.height();
            cairo_translate(cr, nextRegion_.x, nextRegion_.y);
            shrink(nextRegion_, config_->theme_.next.clickMargin);
            double alpha = 1.0;
            if (!hasNext_) {
                alpha = 0.3;
            } else if (nextHovered_) {
                alpha = 0.7;
            }
            config_->theme_.paint(cr, config_->theme_.next, alpha);
            cairo_restore(cr);
            cairo_save(cr);
            prevRegion_.x =
                width - margin.marginRight - next.width() - prev.width();
            prevRegion_.y = height - margin.marginBottom - prev.height();
            prevRegion_.width = prev.width();
            prevRegion_.height = prev.height();
            cairo_translate(cr, prevRegion_.x, prevRegion_.y);
            shrink(prevRegion_, config_->theme_.prev.clickMargin);
            alpha = 1.0;
            if (!hasPrev_) {
                alpha = 0.3;
            } else if (prevHovered_) {
                alpha = 0.7;
            }
            config_->theme_.paint(cr, config_->theme_.prev, alpha);
            cairo_restore(cr);
        }
    }

    // Move position to the right place.
    cairo_translate(cr, margin.marginLeft, margin.marginTop);

    cairo_save(cr);
    cairoSetSourceColor(cr, config_->theme_.normalColor);
    // CLASSICUI_DEBUG() << theme.inputPanel->normalColor->toString();
    auto *metrics = pango_context_get_metrics(
        context_.get(), pango_context_get_font_description(context_.get()),
        pango_context_get_language(context_.get()));
    auto minH = pango_font_metrics_get_ascent(metrics) +
                pango_font_metrics_get_descent(metrics);
    pango_font_metrics_unref(metrics);
    minH = PANGO_PIXELS(minH);

    size_t currentHeight = 0;
    int w, h;
    auto extraW = textMargin.marginLeft + textMargin.marginRight;
    auto extraH = textMargin.marginTop + textMargin.marginBottom;
    if (pango_layout_get_character_count(upperLayout_.get())) {
        renderLayout(cr, upperLayout_.get(), textMargin.marginLeft,
                     textMargin.marginTop);
        pango_layout_get_pixel_size(upperLayout_.get(), &w, &h);
        PangoRectangle pos;
        if (cursor_ >= 0) {
            pango_layout_get_cursor_pos(upperLayout_.get(), cursor_, &pos,
                                        nullptr);

            cairo_save(cr);
            cairo_set_line_width(cr, 2);
            auto offsetX = pango_units_to_double(pos.x);
            cairo_move_to(cr, textMargin.marginLeft + offsetX + 1,
                          textMargin.marginTop);
            cairo_line_to(cr, textMargin.marginLeft + offsetX + 1,
                          textMargin.marginTop + minH);
            cairo_stroke(cr);
            cairo_restore(cr);
        }
        currentHeight += std::max(minH, h) + extraH;
    }
    if (pango_layout_get_character_count(lowerLayout_.get())) {
        renderLayout(cr, lowerLayout_.get(), textMargin.marginLeft,
                     textMargin.marginTop + currentHeight);
        pango_layout_get_pixel_size(lowerLayout_.get(), &w, nullptr);
        currentHeight += std::max(minH, h) + extraH;
    }

    bool vertical = config_->vertical_;
    if (layoutHint_ == FcitxCandidateLayoutHint::Vertical) {
        vertical = true;
    } else if (layoutHint_ == FcitxCandidateLayoutHint::Horizontal) {
        vertical = false;
    }

    candidateRegions_.clear();
    candidateRegions_.reserve(nCandidates_);
    size_t wholeW = 0, wholeH = 0;

    // size of text = textMargin + actual text size.
    // HighLight = HighLight margin + TEXT.
    // Click region = HighLight - click

    for (size_t i = 0; i < nCandidates_; i++) {
        int x, y;
        if (vertical) {
            x = 0;
            y = currentHeight + wholeH;
        } else {
            x = wholeW;
            y = currentHeight;
        }
        x += textMargin.marginLeft;
        y += textMargin.marginTop;
        int labelW = 0, labelH = 0, candidateW = 0, candidateH = 0;
        if (pango_layout_get_character_count(labelLayouts_[i].get())) {
            pango_layout_get_pixel_size(labelLayouts_[i].get(), &labelW,
                                        &labelH);
        }
        if (pango_layout_get_character_count(candidateLayouts_[i].get())) {
            pango_layout_get_pixel_size(candidateLayouts_[i].get(), &candidateW,
                                        &candidateH);
        }
        int vheight;
        if (vertical) {
            vheight = std::max({minH, labelH, candidateH});
            wholeH += vheight + extraH;
        } else {
            vheight = candidatesHeight_ - extraH;
            wholeW += candidateW + labelW + extraW;
        }
        const auto &highlightMargin = config_->theme_.highlight.margin;
        const auto &clickMargin = config_->theme_.highlight.clickMargin;
        auto highlightWidth = labelW + candidateW;
        if (config_->theme_.fullWidthHighlight && vertical) {
            // Last candidate, fill.
            highlightWidth = width - margin.marginLeft - margin.marginRight -
                             textMargin.marginRight - textMargin.marginLeft;
        }
        const int highlightIndex = highlight();
        if (highlightIndex >= 0 && i == static_cast<size_t>(highlightIndex)) {
            cairo_save(cr);
            cairo_translate(cr, x - highlightMargin.marginLeft,
                            y - highlightMargin.marginTop);
            config_->theme_.paint(cr, config_->theme_.highlight,
                                  highlightWidth + highlightMargin.marginLeft +
                                      highlightMargin.marginRight,
                                  vheight + highlightMargin.marginTop +
                                      highlightMargin.marginBottom);
            cairo_restore(cr);
            pango_layout_set_attributes(labelLayouts_[i].get(),
                                        highlightLabelAttrLists_[i].get());
            pango_layout_set_attributes(candidateLayouts_[i].get(),
                                        highlightCandidateAttrLists_[i].get());
        } else {
            pango_layout_set_attributes(labelLayouts_[i].get(),
                                        labelAttrLists_[i].get());

            pango_layout_set_attributes(candidateLayouts_[i].get(),
                                        candidateAttrLists_[i].get());
        }
        cairo_rectangle_int_t candidateRegion;
        candidateRegion.x = margin.marginLeft + x - highlightMargin.marginLeft +
                            clickMargin.marginLeft;
        candidateRegion.y = margin.marginTop + y - highlightMargin.marginTop +
                            clickMargin.marginTop;
        candidateRegion.width = highlightWidth + highlightMargin.marginLeft +
                                highlightMargin.marginRight -
                                clickMargin.marginLeft -
                                clickMargin.marginRight;
        candidateRegion.height =
            vheight + highlightMargin.marginTop + highlightMargin.marginBottom -
            clickMargin.marginTop - clickMargin.marginBottom;
        candidateRegions_.push_back(candidateRegion);
        if (pango_layout_get_character_count(labelLayouts_[i].get())) {
            renderLayout(cr, labelLayouts_[i].get(), x, y);
        }
        if (pango_layout_get_character_count(candidateLayouts_[i].get())) {
            renderLayout(cr, candidateLayouts_[i].get(), x + labelW, y);
        }
    }
    cairo_restore(cr);
}

void InputWindow::click(int x, int y) {
    for (size_t idx = 0, e = candidateRegions_.size(); idx < e; idx++) {
        if (rectContains(candidateRegions_[idx], x, y)) {
            selectCandidate(idx);
            return;
        }
    }
    if (hasPrev_ && rectContains(prevRegion_, x, y)) {
        prev();
        return;
    }
    if (hasNext_ && rectContains(nextRegion_, x, y)) {
        next();
        return;
    }
}

void InputWindow::wheel(bool up) {
    if (!config_->wheelForPaging_) {
        return;
    }
    if (nCandidates_ == 0) {
        return;
    }
    if (up) {
        if (hasPrev_) {
            prev();
        }
    } else {
        if (hasNext_) {
            next();
        }
    }
}

int InputWindow::highlight() const {
    int highlightIndex = (hoverIndex_ >= 0) ? hoverIndex_ : candidateIndex_;
    return highlightIndex;
}

bool InputWindow::hover(int x, int y) {
    bool needRepaint = false;
    auto oldHighlight = highlight();
    hoverIndex_ = -1;
    for (int idx = 0, e = candidateRegions_.size(); idx < e; idx++) {
        if (rectContains(candidateRegions_[idx], x, y)) {
            hoverIndex_ = idx;
            break;
        }
    }

    needRepaint = needRepaint || oldHighlight != highlight();

    auto prevHovered = rectContains(prevRegion_, x, y);
    auto nextHovered = rectContains(nextRegion_, x, y);
    needRepaint = needRepaint || prevHovered_ != prevHovered;
    needRepaint = needRepaint || nextHovered_ != nextHovered;
    prevHovered_ = prevHovered;
    nextHovered_ = nextHovered;
    return needRepaint;
}

void InputWindow::prev() {
    if (hasPrev_) {
        fcitx_g_client_prev_page(client_.get());
    }
}

void InputWindow::next() {
    if (hasNext_) {
        fcitx_g_client_next_page(client_.get());
    }
}

void InputWindow::selectCandidate(int i) {
    fcitx_g_client_select_candidate(client_.get(), i);
}

} // namespace fcitx::gtk
