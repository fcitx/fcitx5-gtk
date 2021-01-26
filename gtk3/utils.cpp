/*
 * SPDX-FileCopyrightText: 2021~2021 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */
#include <string>

namespace fcitx::gtk {

enum class UnescapeState { NORMAL, ESCAPE };

bool unescape(std::string &str) {
    if (str.empty()) {
        return true;
    }

    bool unescapeQuote = false;
    // having quote at beginning and end, escape
    if (str.size() >= 2 && str.front() == '"' && str.back() == '"') {
        unescapeQuote = true;
        str.pop_back();
        str.erase(0, 1);
    }

    size_t i = 0;
    size_t j = 0;
    UnescapeState state = UnescapeState::NORMAL;
    do {
        switch (state) {
        case UnescapeState::NORMAL:
            if (str[i] == '\\') {
                state = UnescapeState::ESCAPE;
            } else {
                str[j] = str[i];
                j++;
            }
            break;
        case UnescapeState::ESCAPE:
            if (str[i] == '\\') {
                str[j] = '\\';
                j++;
            } else if (str[i] == 'n') {
                str[j] = '\n';
                j++;
            } else if (str[i] == '\"' && unescapeQuote) {
                str[j] = '\"';
                j++;
            } else {
                return false;
            }
            state = UnescapeState::NORMAL;
            break;
        }
    } while (str[i++]);
    str.resize(j - 1);
    return true;
}

} // namespace fcitx::gtk
