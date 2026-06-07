#ifndef GHOSTCODE_UI_TEXT_H
#define GHOSTCODE_UI_TEXT_H

#include <stddef.h>
#include <curses.h>

int ghostcode_ui_text_width(const char *text);
int ghostcode_ui_text_width_n(const char *text, size_t bytes);
size_t ghostcode_ui_text_bytes_for_width(const char *text, int max_width);
int ghostcode_ui_text_addnstr_width(WINDOW *win, const char *text, size_t bytes, int max_width);
int ghostcode_ui_text_addstr_width(WINDOW *win, const char *text, int max_width);

#endif
