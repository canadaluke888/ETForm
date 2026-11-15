#ifndef TFORM_UI_LIST_H
#define TFORM_UI_LIST_H

#include <curses.h>

typedef enum {
    TFORM_UI_LIST_ROW_HEADER = 0,
    TFORM_UI_LIST_ROW_DIVIDER,
    TFORM_UI_LIST_ROW_SPACER,
    TFORM_UI_LIST_ROW_ITEM
} TFormUiListRowKind;

typedef struct {
    TFormUiListRowKind kind;
    const char *label;
    int id;
    int enabled;
    int indent;
    short color_pair;
    int attrs;
    void *data;
} TFormUiListRow;

int tform_ui_list_row_is_selectable(const TFormUiListRow *row);
int tform_ui_list_find_first_selectable(const TFormUiListRow *rows, int count);
int tform_ui_list_find_next_selectable(const TFormUiListRow *rows, int count, int start, int dir);
void tform_ui_list_clamp_view(const TFormUiListRow *rows, int count, int visible, int *top, int *selected);
int tform_ui_list_row_at(int top, int visible, int list_top_row, int window_row, const TFormUiListRow *rows, int count);
void tform_ui_list_draw(
    WINDOW *win,
    const TFormUiListRow *rows,
    int count,
    int top,
    int selected,
    int y,
    int x,
    int h,
    int w
);

#endif
