/*
 * SPDX-FileCopyrightText: 2021~2021 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */
#ifndef _GTK3_UTILS_H_
#define _GTK3_UTILS_H_

#include "fcitxflags.h"
#include <cairo.h>
#include <glib-object.h>
#include <memory>
#include <utility>

namespace fcitx::gtk {

bool unescape(std::string &str);

template <auto FreeFunction>
struct FunctionDeleter {
    template <typename T>
    void operator()(T *p) const {
        if (p) {
            FreeFunction(const_cast<std::remove_const_t<T> *>(p));
        }
    }
};
template <typename T, auto FreeFunction = std::free>
using UniqueCPtr = std::unique_ptr<T, FunctionDeleter<FreeFunction>>;

template <typename T>
using GObjectUniquePtr = UniqueCPtr<T, g_object_unref>;

static inline bool rectContains(cairo_rectangle_int_t rect1,
                                cairo_rectangle_int_t rect2) {
    return (rect1.x <= rect2.x && rect1.y <= rect2.y &&
            rect1.x + rect1.width >= rect2.x + rect2.width &&
            rect1.y + rect1.height >= rect2.y + rect2.height);
}

static inline bool rectContains(cairo_rectangle_int_t rect, int x, int y) {
    return x >= rect.x && y >= rect.y && x <= rect.x + rect.width &&
           y <= rect.y + rect.height;
}

static inline gboolean check_app_name(const gchar *pattern) {
    bool result = FALSE;
    const gchar *prgname = g_get_prgname();
    if (!prgname) {
        return FALSE;
    }
    gchar **p;
    gchar **apps = g_strsplit(pattern, ",", 0);
    for (p = apps; *p != NULL; p++) {
        if (g_regex_match_simple(*p, prgname, (GRegexCompileFlags)0,
                                 (GRegexMatchFlags)0)) {
            result = TRUE;
            break;
        }
    }
    g_strfreev(apps);
    return result;
}

static inline bool get_boolean_env(const char *name, bool defval) {
    const char *value = getenv(name);

    if (value == nullptr) {
        return defval;
    }

    if (g_strcmp0(value, "") == 0 || g_strcmp0(value, "0") == 0 ||
        g_strcmp0(value, "false") == 0 || g_strcmp0(value, "False") == 0 ||
        g_strcmp0(value, "FALSE") == 0) {
        return false;
    }

    return true;
}

constexpr int MAX_CACHED_HANDLED_EVENT = 40;

constexpr uint64_t purpose_related_capability =
    fcitx::FcitxCapabilityFlag_Alpha | fcitx::FcitxCapabilityFlag_Digit |
    fcitx::FcitxCapabilityFlag_Number | fcitx::FcitxCapabilityFlag_Dialable |
    fcitx::FcitxCapabilityFlag_Url | fcitx::FcitxCapabilityFlag_Email |
    fcitx::FcitxCapabilityFlag_Password;

constexpr uint64_t hints_related_capability =
    fcitx::FcitxCapabilityFlag_SpellCheck |
    fcitx::FcitxCapabilityFlag_NoSpellCheck |
    fcitx::FcitxCapabilityFlag_WordCompletion |
    fcitx::FcitxCapabilityFlag_Lowercase |
    fcitx::FcitxCapabilityFlag_Uppercase |
    fcitx::FcitxCapabilityFlag_UppercaseWords |
    fcitx::FcitxCapabilityFlag_UppwercaseSentences |
    fcitx::FcitxCapabilityFlag_NoOnScreenKeyboard;

constexpr uint32_t HandledMask = (1 << 24);
constexpr uint32_t IgnoredMask = (1 << 25);
constexpr unsigned int MAX_CACHED_EVENTS = 30;

} // namespace fcitx::gtk

#endif // _GTK3_UTILS_H_
