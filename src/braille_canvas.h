#ifndef UI_BRAILLE_CANVAS_H
#define UI_BRAILLE_CANVAS_H

#include <ncurses.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int char_width;
    int char_height;
    uint8_t *cells;
    uint16_t *cell_colors;
} BrailleCanvas;

int braille_canvas_init(BrailleCanvas *canvas, int char_width, int char_height);
int braille_canvas_copy(BrailleCanvas *dest, const BrailleCanvas *src);
void braille_canvas_free(BrailleCanvas *canvas);
void braille_canvas_clear(BrailleCanvas *canvas);
int braille_canvas_dot_width(const BrailleCanvas *canvas);
int braille_canvas_dot_height(const BrailleCanvas *canvas);
int braille_canvas_set_dot(BrailleCanvas *canvas, int x, int y);
int braille_canvas_draw_line(BrailleCanvas *canvas, int x0, int y0, int x1, int y1);
int braille_canvas_draw_circle(BrailleCanvas *canvas, int center_x, int center_y, int radius);
int braille_canvas_draw_image(BrailleCanvas *canvas, const char *path);
int braille_canvas_image_size(const char *path, int *width, int *height);
const char *braille_canvas_image_error(void);
uint8_t braille_canvas_cell(const BrailleCanvas *canvas, int cell_x, int cell_y);
uint16_t braille_canvas_cell_color(const BrailleCanvas *canvas, int cell_x, int cell_y);
int braille_canvas_encode_cell(uint8_t mask, char out[4]);
int braille_canvas_render(const BrailleCanvas *canvas,
                          WINDOW *win,
                          int origin_y,
                          int origin_x,
                          int max_rows,
                          int max_columns);

#endif
