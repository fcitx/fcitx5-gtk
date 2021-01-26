/*
 * SPDX-FileCopyrightText: 2021~2021 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */
#ifndef _GTK3_UTILS_H_
#define _GTK3_UTILS_H_

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

} // namespace fcitx::gtk

#endif // _GTK3_UTILS_H_
