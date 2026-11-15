#define _XOPEN_SOURCE 700

#include "ui_text.h"

#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

static int char_display_width(wchar_t ch) {
    int width = wcwidth(ch);
    return width > 0 ? width : 1;
}

int tform_ui_text_width_n(const char *text, size_t bytes) {
    mbstate_t state;
    size_t offset = 0;
    int width = 0;

    if (!text || bytes == 0) {
        return 0;
    }

    memset(&state, 0, sizeof(state));
    while (offset < bytes && text[offset]) {
        wchar_t ch;
        size_t consumed = mbrtowc(&ch, text + offset, bytes - offset, &state);
        if (consumed == (size_t)-1 || consumed == (size_t)-2 || consumed == 0) {
            memset(&state, 0, sizeof(state));
            offset++;
            width++;
            continue;
        }
        width += char_display_width(ch);
        offset += consumed;
    }
    return width;
}

int tform_ui_text_width(const char *text) {
    return tform_ui_text_width_n(text, text ? strlen(text) : 0);
}

size_t tform_ui_text_bytes_for_width(const char *text, int max_width) {
    mbstate_t state;
    size_t offset = 0;
    size_t last_fit = 0;
    int width = 0;

    if (!text || max_width <= 0) {
        return 0;
    }

    memset(&state, 0, sizeof(state));
    while (text[offset]) {
        wchar_t ch;
        size_t remaining = strlen(text + offset);
        size_t consumed = mbrtowc(&ch, text + offset, remaining, &state);
        int next_width;
        if (consumed == (size_t)-1 || consumed == (size_t)-2 || consumed == 0) {
            memset(&state, 0, sizeof(state));
            consumed = 1;
            next_width = 1;
        } else {
            next_width = char_display_width(ch);
        }
        if (width + next_width > max_width) {
            break;
        }
        width += next_width;
        offset += consumed;
        last_fit = offset;
    }
    return last_fit;
}

int tform_ui_text_addnstr_width(WINDOW *win, const char *text, size_t bytes, int max_width) {
    size_t clipped;
    if (!win || !text || max_width <= 0) {
        return 0;
    }
    clipped = tform_ui_text_bytes_for_width(text, max_width);
    if (clipped > bytes) {
        clipped = bytes;
    }
    waddnstr(win, text, (int)clipped);
    return tform_ui_text_width_n(text, clipped);
}

int tform_ui_text_addstr_width(WINDOW *win, const char *text, int max_width) {
    if (!text) {
        return 0;
    }
    return tform_ui_text_addnstr_width(win, text, strlen(text), max_width);
}
