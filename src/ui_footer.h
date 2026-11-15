#ifndef ETFORM_UI_FOOTER_H
#define ETFORM_UI_FOOTER_H

#include <curses.h>

void etform_ui_draw_status_segment(WINDOW *win, int y, int *x, int max_x, int attr, const char *text);
void etform_ui_draw_action_hint_segment(WINDOW *win, int y, int *x, int max_x, const char *text);
void etform_ui_draw_footer_separator(WINDOW *win, int y, int *x, int max_x);
void etform_ui_draw_footer_box(WINDOW *win);
void etform_ui_draw_footer(WINDOW *win, const char *hint_text);

#endif
