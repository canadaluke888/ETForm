#include "ui_footer.h"

#include "editor_theme.h"
#include "ui_text.h"

#include <string.h>

void etform_ui_draw_status_segment(WINDOW *win, int y, int *x, int max_x, int attr, const char *text) {
    int remaining;

    if (!win || !x || !text || !*text || *x > max_x) {
        return;
    }
    remaining = max_x - *x + 1;
    if (remaining <= 0) {
        return;
    }

    wmove(win, y, *x);
    wattron(win, attr);
    *x += etform_ui_text_addstr_width(win, text, remaining);
    wattroff(win, attr);
}

void etform_ui_draw_action_hint_segment(WINDOW *win, int y, int *x, int max_x, const char *text) {
    const char *cursor = text;

    if (!win || !text) {
        return;
    }

    while (*cursor && *x <= max_x) {
        const char *open = strchr(cursor, '[');
        if (!open) {
            etform_ui_draw_status_segment(
                win, y, x, max_x, etform_color_pair(ETFORM_PAIR_FOOTER_TEXT), cursor
            );
            break;
        }
        if (open > cursor) {
            char plain[256];
            size_t len = (size_t)(open - cursor);
            if (len >= sizeof(plain)) {
                len = sizeof(plain) - 1;
            }
            memcpy(plain, cursor, len);
            plain[len] = '\0';
            etform_ui_draw_status_segment(
                win, y, x, max_x, etform_color_pair(ETFORM_PAIR_FOOTER_TEXT), plain
            );
        }

        const char *close = strchr(open + 1, ']');
        if (!close) {
            etform_ui_draw_status_segment(
                win, y, x, max_x, etform_color_pair(ETFORM_PAIR_FOOTER_TEXT), open
            );
            break;
        }

        char key[128];
        size_t len = (size_t)(close - open + 1);
        if (len >= sizeof(key)) {
            len = sizeof(key) - 1;
        }
        memcpy(key, open, len);
        key[len] = '\0';
        etform_ui_draw_status_segment(
            win, y, x, max_x, etform_color_pair(ETFORM_PAIR_FOOTER_KEY) | A_BOLD, key
        );
        cursor = close + 1;
    }
}

void etform_ui_draw_footer_separator(WINDOW *win, int y, int *x, int max_x) {
    etform_ui_draw_status_segment(
        win, y, x, max_x, etform_color_pair(ETFORM_PAIR_FOOTER_SEPARATOR) | A_BOLD, "  |  "
    );
}

void etform_ui_draw_footer_box(WINDOW *win) {
    int h;
    int w;

    if (!win) {
        return;
    }
    getmaxyx(win, h, w);
    if (h < 3 || w < 4) {
        return;
    }

    wattron(win, etform_color_pair(ETFORM_PAIR_FOOTER_BORDER));
    box(win, 0, 0);
    wattroff(win, etform_color_pair(ETFORM_PAIR_FOOTER_BORDER));
}

void etform_ui_draw_footer(WINDOW *win, const char *hint_text) {
    int h;
    int w;
    int y;
    int x = 2;
    int max_x;

    if (!win) {
        return;
    }
    getmaxyx(win, h, w);
    werase(win);
    etform_ui_draw_footer_box(win);
    if (h < 2 || w < 4) {
        return;
    }

    y = h / 2;
    max_x = w - 3;
    etform_ui_draw_action_hint_segment(win, y, &x, max_x, hint_text);
}
