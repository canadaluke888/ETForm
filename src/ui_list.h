#ifndef ETFORM_UI_LIST_H
#define ETFORM_UI_LIST_H

#include <curses.h>

typedef enum {
    ETFORM_UI_LIST_ROW_HEADER = 0,
    ETFORM_UI_LIST_ROW_DIVIDER,
    ETFORM_UI_LIST_ROW_SPACER,
    ETFORM_UI_LIST_ROW_ITEM
} ETFormUiListRowKind;

typedef struct {
    ETFormUiListRowKind kind;
    const char *label;
    int id;
    int enabled;
    int indent;
    short color_pair;
    int attrs;
    void *data;
} ETFormUiListRow;

int etform_ui_list_row_is_selectable(const ETFormUiListRow *row);
int etform_ui_list_find_first_selectable(const ETFormUiListRow *rows, int count);
int etform_ui_list_find_next_selectable(const ETFormUiListRow *rows, int count, int start, int dir);
void etform_ui_list_clamp_view(const ETFormUiListRow *rows, int count, int visible, int *top, int *selected);
int etform_ui_list_row_at(int top, int visible, int list_top_row, int window_row, const ETFormUiListRow *rows, int count);
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
);

#endif
