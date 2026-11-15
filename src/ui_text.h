#ifndef ETFORM_UI_TEXT_H
#define ETFORM_UI_TEXT_H

#include <stddef.h>
#include <curses.h>

int etform_ui_text_width(const char *text);
int etform_ui_text_width_n(const char *text, size_t bytes);
size_t etform_ui_text_bytes_for_width(const char *text, int max_width);
int etform_ui_text_addnstr_width(WINDOW *win, const char *text, size_t bytes, int max_width);
int etform_ui_text_addstr_width(WINDOW *win, const char *text, int max_width);

#endif
