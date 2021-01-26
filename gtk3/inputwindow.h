/*
 * SPDX-FileCopyrightText: 2021~2021 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */
#ifndef _GTK3_INPUTWINDOW_H_
#define _GTK3_INPUTWINDOW_H_

#include "fcitx-gclient/fcitxgclient.h"
#include "fcitxflags.h"
#include "utils.h"
#include <cairo/cairo.h>
#include <pango/pango.h>
#include <string>
#include <utility>
#include <vector>

namespace fcitx::gtk {

class ClassicUIConfig;

using PangoAttrListUniquePtr = UniqueCPtr<PangoAttrList, pango_attr_list_unref>;

class InputWindow {
public:
    InputWindow(ClassicUIConfig *config, FcitxGClient *client);
    virtual ~InputWindow();
    std::pair<unsigned int, unsigned int> sizeHint();
    void paint(cairo_t *cr, unsigned int width, unsigned int height);
    void hide();
    bool visible() const { return visible_; }
    bool hover(int x, int y);
    void click(int x, int y);
    void wheel(bool up);

    virtual void update() = 0;

protected:
    void resizeCandidates(size_t n);
    void appendText(std::string &s, PangoAttrList *attrList,
                    PangoAttrList *highlightAttrList, const GPtrArray *text);
    void appendText(std::string &s, PangoAttrList *attrList,
                    PangoAttrList *highlightAttrList, const gchar *text,
                    int format = 0);
    void insertAttr(PangoAttrList *attrList, FcitxTextFormatFlag format,
                    int start, int end, bool highlight) const;
    void setTextToLayout(PangoLayout *layout, PangoAttrListUniquePtr *attrList,
                         PangoAttrListUniquePtr *highlightAttrList,
                         std::initializer_list<const GPtrArray *> texts);
    void setTextToLayout(PangoLayout *layout, PangoAttrListUniquePtr *attrList,
                         PangoAttrListUniquePtr *highlightAttrList,
                         const gchar *text);

    int highlight() const;

    void prev();
    void next();
    void selectCandidate(int i);
    void updateUI(GPtrArray *preedit, int cursor_pos, GPtrArray *auxUp,
                  GPtrArray *auxDown, GPtrArray *candidates, int highlight,
                  int layoutHint, bool hasPrev, bool hasNext);
    void updateLanguage(const char *language);

    void setLanguageAttr(size_t size, PangoAttrList *attrList,
                         PangoAttrList *highlightAttrList);

    ClassicUIConfig *config_;
    GObjectUniquePtr<FcitxGClient> client_;
    GObjectUniquePtr<PangoContext> context_;
    GObjectUniquePtr<PangoLayout> upperLayout_;
    GObjectUniquePtr<PangoLayout> lowerLayout_;
    std::vector<GObjectUniquePtr<PangoLayout>> labelLayouts_;
    std::vector<GObjectUniquePtr<PangoLayout>> candidateLayouts_;
    std::vector<PangoAttrListUniquePtr> labelAttrLists_;
    std::vector<PangoAttrListUniquePtr> candidateAttrLists_;
    std::vector<PangoAttrListUniquePtr> highlightLabelAttrLists_;
    std::vector<PangoAttrListUniquePtr> highlightCandidateAttrLists_;
    std::vector<cairo_rectangle_int_t> candidateRegions_;
    std::string language_;
    bool visible_ = false;
    int cursor_ = 0;
    int dpi_ = -1;
    size_t nCandidates_ = 0;
    bool hasPrev_ = false;
    bool hasNext_ = false;
    cairo_rectangle_int_t prevRegion_;
    cairo_rectangle_int_t nextRegion_;
    bool prevHovered_ = false;
    bool nextHovered_ = false;
    int candidateIndex_ = -1;
    FcitxCandidateLayoutHint layoutHint_ = FcitxCandidateLayoutHint::NotSet;
    size_t candidatesHeight_ = 0;
    int hoverIndex_ = -1;
};
} // namespace fcitx::gtk

#endif // _GTK3_INPUTWINDOW_H_
