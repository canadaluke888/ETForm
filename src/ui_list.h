#ifndef GHOSTCODE_UI_LIST_H
#define GHOSTCODE_UI_LIST_H

#include <curses.h>

typedef enum {
    GHOSTCODE_UI_LIST_ROW_HEADER = 0,
    GHOSTCODE_UI_LIST_ROW_DIVIDER,
    GHOSTCODE_UI_LIST_ROW_SPACER,
    GHOSTCODE_UI_LIST_ROW_ITEM
} GhostcodeUiListRowKind;

typedef struct {
    GhostcodeUiListRowKind kind;
    const char *label;
    int id;
    int enabled;
    int indent;
    short color_pair;
    int attrs;
    void *data;
} GhostcodeUiListRow;

int ghostcode_ui_list_row_is_selectable(const GhostcodeUiListRow *row);
int ghostcode_ui_list_find_first_selectable(const GhostcodeUiListRow *rows, int count);
int ghostcode_ui_list_find_next_selectable(const GhostcodeUiListRow *rows, int count, int start, int dir);
void ghostcode_ui_list_clamp_view(const GhostcodeUiListRow *rows, int count, int visible, int *top, int *selected);
int ghostcode_ui_list_row_at(int top, int visible, int list_top_row, int window_row, const GhostcodeUiListRow *rows, int count);
void ghostcode_ui_list_draw(
    WINDOW *win,
    const GhostcodeUiListRow *rows,
    int count,
    int top,
    int selected,
    int y,
    int x,
    int h,
    int w
);

#endif
