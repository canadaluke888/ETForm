#include "ui_list.h"

#include "editor_theme.h"
#include "ui_text.h"

#include <string.h>

int etform_ui_list_row_is_selectable(const ETFormUiListRow *row) {
    return row && row->kind == ETFORM_UI_LIST_ROW_ITEM && row->enabled;
}

int etform_ui_list_find_first_selectable(const ETFormUiListRow *rows, int count) {
    if (!rows || count <= 0) {
        return -1;
    }
    for (int i = 0; i < count; i++) {
        if (etform_ui_list_row_is_selectable(&rows[i])) {
            return i;
        }
    }
    return -1;
}

int etform_ui_list_find_next_selectable(const ETFormUiListRow *rows, int count, int start, int dir) {
    if (!rows || count <= 0 || dir == 0) {
        return -1;
    }

    int index = start;
    for (int step = 0; step < count; step++) {
        index += dir;
        if (index < 0) {
            index = count - 1;
        } else if (index >= count) {
            index = 0;
        }
        if (etform_ui_list_row_is_selectable(&rows[index])) {
            return index;
        }
    }
    return -1;
}

void etform_ui_list_clamp_view(
    const ETFormUiListRow *rows,
    int count,
    int visible,
    int *top,
    int *selected
) {
    int max_top;

    if (!rows || count <= 0 || !top || !selected) {
        return;
    }
    if (visible < 1) {
        visible = 1;
    }
    if (*selected < 0 || *selected >= count || !etform_ui_list_row_is_selectable(&rows[*selected])) {
        *selected = etform_ui_list_find_first_selectable(rows, count);
    }
    if (*selected < 0) {
        *selected = 0;
        *top = 0;
        return;
    }

    if (*selected < *top) {
        *top = *selected;
    }
    if (*selected >= *top + visible) {
        *top = *selected - visible + 1;
    }

    max_top = count > visible ? count - visible : 0;
    if (*top < 0) {
        *top = 0;
    }
    if (*top > max_top) {
        *top = max_top;
    }
}

int etform_ui_list_row_at(
    int top,
    int visible,
    int list_top_row,
    int window_row,
    const ETFormUiListRow *rows,
    int count
) {
    if (!rows || count <= 0 || visible <= 0) {
        return -1;
    }
    if (window_row < list_top_row || window_row >= list_top_row + visible) {
        return -1;
    }

    int index = top + (window_row - list_top_row);
    if (index < 0 || index >= count) {
        return -1;
    }
    return index;
}

void etform_ui_list_draw(
    WINDOW *win,
    const ETFormUiListRow *rows,
    int count,
    int top,
    int selected,
    int y,
    int x,
    int h,
    int w
) {
    if (!win || !rows || h <= 0 || w <= 0) {
        return;
    }

    for (int row = 0; row < h; row++) {
        int index = top + row;
        mvwhline(win, y + row, x, ' ', w);
        if (index < 0 || index >= count) {
            continue;
        }

        const char *label = rows[index].label ? rows[index].label : "";
        int content_x = x + rows[index].indent;
        int content_w = w - rows[index].indent;
        if (content_w < 1) {
            continue;
        }

        if (rows[index].kind == ETFORM_UI_LIST_ROW_HEADER) {
            wattron(win, etform_color_pair(ETFORM_PAIR_HEADER) | A_BOLD);
            wmove(win, y + row, content_x);
            etform_ui_text_addstr_width(win, label, content_w);
            wattroff(win, etform_color_pair(ETFORM_PAIR_HEADER) | A_BOLD);
        } else if (rows[index].kind == ETFORM_UI_LIST_ROW_DIVIDER) {
            mvwhline(win, y + row, content_x, ACS_HLINE, content_w);
        } else if (rows[index].kind == ETFORM_UI_LIST_ROW_ITEM) {
            int row_attrs = rows[index].attrs;
            if (rows[index].color_pair && has_colors()) {
                row_attrs |= COLOR_PAIR(rows[index].color_pair);
            }
            if (index == selected) {
                wattron(win, etform_color_pair(ETFORM_PAIR_CURSOR) | A_BOLD);
            } else if (row_attrs) {
                wattron(win, row_attrs);
            }
            wmove(win, y + row, content_x);
            etform_ui_text_addstr_width(win, label, content_w);
            if (index == selected) {
                wattroff(win, etform_color_pair(ETFORM_PAIR_CURSOR) | A_BOLD);
            } else if (row_attrs) {
                wattroff(win, row_attrs);
            }
        }
    }
}
