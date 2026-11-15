#ifndef TFORM_UI_TEXT_H
#define TFORM_UI_TEXT_H

#include <stddef.h>
#include <curses.h>

int tform_ui_text_width(const char *text);
int tform_ui_text_width_n(const char *text, size_t bytes);
size_t tform_ui_text_bytes_for_width(const char *text, int max_width);
int tform_ui_text_addnstr_width(WINDOW *win, const char *text, size_t bytes, int max_width);
int tform_ui_text_addstr_width(WINDOW *win, const char *text, int max_width);

#endif
