#ifndef ETFORM_FORM_RUNTIME_H
#define ETFORM_FORM_RUNTIME_H

#include "form_object.h"

int etform_run_form(ETFormObject *form);
int etform_static_text_height(const ETFormTextStyle *text, int width, int wrap_text);
void etform_draw_static_text(
    WINDOW *win,
    int y,
    int x,
    int width,
    int height,
    const ETFormTextStyle *text,
    int wrap_text,
    int clip_top,
    int clip_bottom
);
int etform_static_element_height(ETFormSectionElement *element, int width);
void etform_draw_static_element(
    WINDOW *win,
    int y,
    int x,
    int width,
    ETFormSectionElement *element,
    int clip_top,
    int clip_bottom
);
void etform_draw_static_box(
    WINDOW *win,
    int y,
    int x,
    int width,
    int height,
    int clip_top,
    int clip_bottom
);

#endif
