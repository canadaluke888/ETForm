#ifndef TFORM_FORM_RUNTIME_H
#define TFORM_FORM_RUNTIME_H

#include "form_object.h"

int tform_run_form(TFormObject *form);
int tform_static_text_height(const TFormTextStyle *text, int width, int wrap_text);
void tform_draw_static_text(
    WINDOW *win,
    int y,
    int x,
    int width,
    int height,
    const TFormTextStyle *text,
    int wrap_text,
    int clip_top,
    int clip_bottom
);
int tform_static_element_height(TFormSectionElement *element, int width);
void tform_draw_static_element(
    WINDOW *win,
    int y,
    int x,
    int width,
    TFormSectionElement *element,
    int clip_top,
    int clip_bottom
);
void tform_draw_static_box(
    WINDOW *win,
    int y,
    int x,
    int width,
    int height,
    int clip_top,
    int clip_bottom
);

#endif
