
/* A 2x4-dot Unicode Braille raster for terminal graphics. */

#include "braille_canvas.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
#include "third_party/stb_image.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static uint8_t braille_dot_mask(int dot_x, int dot_y)
{
    static const uint8_t masks[4][2] = {
        {0x01, 0x08},
        {0x02, 0x10},
        {0x04, 0x20},
        {0x40, 0x80}
    };

    return masks[dot_y][dot_x];
}

static int braille_nearest_color_cube_level(int value)
{
    static const int levels[] = {0, 95, 135, 175, 215, 255};
    int nearest = 0;
    int distance = abs(value - levels[0]);

    for (int index = 1; index < 6; index++) {
        int next_distance = abs(value - levels[index]);
        if (next_distance < distance) {
            nearest = index;
            distance = next_distance;
        }
    }
    return nearest;
}

static uint16_t braille_xterm_color(int red, int green, int blue)
{
    int red_level = braille_nearest_color_cube_level(red);
    int green_level = braille_nearest_color_cube_level(green);
    int blue_level = braille_nearest_color_cube_level(blue);
    return (uint16_t)(16 + red_level * 36 + green_level * 6 + blue_level);
}

int braille_canvas_init(BrailleCanvas *canvas, int char_width, int char_height)
{
    size_t count;

    if (!canvas || char_width <= 0 || char_height <= 0 ||
        char_width > INT_MAX / 2 || char_height > INT_MAX / 4) {
        return -1;
    }
    if ((size_t)char_width > SIZE_MAX / (size_t)char_height) return -1;
    count = (size_t)char_width * (size_t)char_height;
    canvas->cells = calloc(count, sizeof(*canvas->cells));
    if (!canvas->cells) {
        memset(canvas, 0, sizeof(*canvas));
        return -1;
    }
    canvas->cell_colors = malloc(count * sizeof(*canvas->cell_colors));
    if (!canvas->cell_colors) {
        free(canvas->cells);
        memset(canvas, 0, sizeof(*canvas));
        return -1;
    }
    for (size_t index = 0; index < count; index++) {
        canvas->cell_colors[index] = UINT16_MAX;
    }
    canvas->char_width = char_width;
    canvas->char_height = char_height;
    return 0;
}

int braille_canvas_copy(BrailleCanvas *dest, const BrailleCanvas *src)
{
    size_t count;

    if (!dest || !src || !src->cells ||
        braille_canvas_init(dest, src->char_width, src->char_height) < 0) {
        return -1;
    }
    count = (size_t)src->char_width * (size_t)src->char_height;
    memcpy(dest->cells, src->cells, count);
    memcpy(dest->cell_colors, src->cell_colors, count * sizeof(*dest->cell_colors));
    return 0;
}

void braille_canvas_free(BrailleCanvas *canvas)
{
    if (!canvas) return;
    free(canvas->cells);
    free(canvas->cell_colors);
    memset(canvas, 0, sizeof(*canvas));
}

void braille_canvas_clear(BrailleCanvas *canvas)
{
    if (!canvas || !canvas->cells) return;
    memset(canvas->cells, 0,
           (size_t)canvas->char_width * (size_t)canvas->char_height);
    for (size_t index = 0;
         index < (size_t)canvas->char_width * (size_t)canvas->char_height;
         index++) {
        canvas->cell_colors[index] = UINT16_MAX;
    }
}

int braille_canvas_dot_width(const BrailleCanvas *canvas)
{
    return canvas ? canvas->char_width * 2 : 0;
}

int braille_canvas_dot_height(const BrailleCanvas *canvas)
{
    return canvas ? canvas->char_height * 4 : 0;
}

int braille_canvas_set_dot(BrailleCanvas *canvas, int x, int y)
{
    int cell_x;
    int cell_y;
    size_t index;

    if (!canvas || !canvas->cells || x < 0 || y < 0 ||
        x >= braille_canvas_dot_width(canvas) ||
        y >= braille_canvas_dot_height(canvas)) {
        return 0;
    }
    cell_x = x / 2;
    cell_y = y / 4;
    index = (size_t)cell_y * (size_t)canvas->char_width + (size_t)cell_x;
    canvas->cells[index] |= braille_dot_mask(x % 2, y % 4);
    return 1;
}

int braille_canvas_draw_line(BrailleCanvas *canvas, int x0, int y0, int x1, int y1)
{
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    int dots = 0;

    if (!canvas || !canvas->cells) return -1;
    for (;;) {
        dots += braille_canvas_set_dot(canvas, x0, y0);
        if (x0 == x1 && y0 == y1) break;
        {
            int doubled = error * 2;

            if (doubled >= dy) {
                error += dy;
                x0 += sx;
            }
            if (doubled <= dx) {
                error += dx;
                y0 += sy;
            }
        }
    }
    return dots;
}

int braille_canvas_draw_circle(BrailleCanvas *canvas, int center_x, int center_y, int radius)
{
    int x;
    int y;
    int error;
    int dots = 0;

    if (!canvas || !canvas->cells || radius < 0) return -1;
    x = radius;
    y = 0;
    error = 1 - radius;
    while (x >= y) {
        dots += braille_canvas_set_dot(canvas, center_x + x, center_y + y);
        dots += braille_canvas_set_dot(canvas, center_x + y, center_y + x);
        dots += braille_canvas_set_dot(canvas, center_x - y, center_y + x);
        dots += braille_canvas_set_dot(canvas, center_x - x, center_y + y);
        dots += braille_canvas_set_dot(canvas, center_x - x, center_y - y);
        dots += braille_canvas_set_dot(canvas, center_x - y, center_y - x);
        dots += braille_canvas_set_dot(canvas, center_x + y, center_y - x);
        dots += braille_canvas_set_dot(canvas, center_x + x, center_y - y);
        y++;
        if (error < 0) {
            error += 2 * y + 1;
        } else {
            x--;
            error += 2 * (y - x) + 1;
        }
    }
    return dots;
}

int braille_canvas_draw_image(BrailleCanvas *canvas, const char *path)
{
    static const uint8_t bayer[4][4] = {
        {0, 8, 2, 10},
        {12, 4, 14, 6},
        {3, 11, 1, 9},
        {15, 7, 13, 5}
    };
    unsigned char *pixels;
    long long *red_sums;
    long long *green_sums;
    long long *blue_sums;
    int *color_counts;
    size_t cell_count;
    int image_width;
    int image_height;
    int channels;
    int target_width;
    int target_height;
    int offset_x;
    int offset_y;
    int dot_width;
    int dot_height;
    int dots = 0;

    if (!canvas || !canvas->cells || !path || !*path) return -1;
    pixels = stbi_load(path, &image_width, &image_height, &channels, 4);
    if (!pixels) return -1;
    cell_count = (size_t)canvas->char_width * (size_t)canvas->char_height;
    red_sums = calloc(cell_count, sizeof(*red_sums));
    green_sums = calloc(cell_count, sizeof(*green_sums));
    blue_sums = calloc(cell_count, sizeof(*blue_sums));
    color_counts = calloc(cell_count, sizeof(*color_counts));
    if (!red_sums || !green_sums || !blue_sums || !color_counts) {
        free(red_sums);
        free(green_sums);
        free(blue_sums);
        free(color_counts);
        stbi_image_free(pixels);
        return -1;
    }

    dot_width = braille_canvas_dot_width(canvas);
    dot_height = braille_canvas_dot_height(canvas);
    target_width = dot_width;
    target_height = (int)((long long)image_height * target_width / image_width);
    if (target_height > dot_height) {
        target_height = dot_height;
        target_width = (int)((long long)image_width * target_height / image_height);
    }
    if (target_width < 1) target_width = 1;
    if (target_height < 1) target_height = 1;
    offset_x = (dot_width - target_width) / 2;
    offset_y = (dot_height - target_height) / 2;

    for (int target_y = 0; target_y < target_height; target_y++) {
        int source_y0 = (int)((long long)target_y * image_height / target_height);
        int source_y1 = (int)((long long)(target_y + 1) * image_height / target_height);
        if (source_y1 <= source_y0) source_y1 = source_y0 + 1;
        if (source_y1 > image_height) source_y1 = image_height;
        for (int target_x = 0; target_x < target_width; target_x++) {
            int source_x0 = (int)((long long)target_x * image_width / target_width);
            int source_x1 = (int)((long long)(target_x + 1) * image_width / target_width);
            long long red_sum = 0;
            long long green_sum = 0;
            long long blue_sum = 0;
            long long sample_count = 0;
            if (source_x1 <= source_x0) source_x1 = source_x0 + 1;
            if (source_x1 > image_width) source_x1 = image_width;
            for (int source_y = source_y0; source_y < source_y1; source_y++) {
                for (int source_x = source_x0; source_x < source_x1; source_x++) {
                    size_t index = ((size_t)source_y * (size_t)image_width +
                                    (size_t)source_x) * 4u;
                    int red = pixels[index];
                    int green = pixels[index + 1];
                    int blue = pixels[index + 2];
                    int alpha = pixels[index + 3];
                    red_sum += (red * alpha + 255 * (255 - alpha)) / 255;
                    green_sum += (green * alpha + 255 * (255 - alpha)) / 255;
                    blue_sum += (blue * alpha + 255 * (255 - alpha)) / 255;
                    sample_count++;
                }
            }
            if (sample_count > 0) {
                int red = (int)(red_sum / sample_count);
                int green = (int)(green_sum / sample_count);
                int blue = (int)(blue_sum / sample_count);
                int luminance = (red * 299 + green * 587 + blue * 114) / 1000;
                int threshold = ((int)bayer[(offset_y + target_y) % 4]
                                           [(offset_x + target_x) % 4] * 16) + 8;
                int dot_x = offset_x + target_x;
                int dot_y = offset_y + target_y;
                int cell_x = dot_x / 2;
                int cell_y = dot_y / 4;
                size_t cell_index = (size_t)cell_y * (size_t)canvas->char_width +
                                    (size_t)cell_x;

                if (luminance < threshold) {
                    dots += braille_canvas_set_dot(canvas, dot_x, dot_y);
                    red_sums[cell_index] += red;
                    green_sums[cell_index] += green;
                    blue_sums[cell_index] += blue;
                    color_counts[cell_index]++;
                }
            }
        }
    }
    for (size_t index = 0; index < cell_count; index++) {
        if (color_counts[index] > 0) {
            canvas->cell_colors[index] = braille_xterm_color(
                (int)(red_sums[index] / color_counts[index]),
                (int)(green_sums[index] / color_counts[index]),
                (int)(blue_sums[index] / color_counts[index])
            );
        }
    }
    free(red_sums);
    free(green_sums);
    free(blue_sums);
    free(color_counts);
    stbi_image_free(pixels);
    return dots;
}

int braille_canvas_image_size(const char *path, int *width, int *height)
{
    int channels;

    if (!path || !*path || !width || !height) return -1;
    return stbi_info(path, width, height, &channels) ? 0 : -1;
}

const char *braille_canvas_image_error(void)
{
    const char *reason = stbi_failure_reason();
    return reason ? reason : "unable to decode image";
}

uint8_t braille_canvas_cell(const BrailleCanvas *canvas, int cell_x, int cell_y)
{
    if (!canvas || !canvas->cells || cell_x < 0 || cell_y < 0 ||
        cell_x >= canvas->char_width || cell_y >= canvas->char_height) {
        return 0;
    }
    return canvas->cells[(size_t)cell_y * (size_t)canvas->char_width +
                         (size_t)cell_x];
}

uint16_t braille_canvas_cell_color(const BrailleCanvas *canvas, int cell_x, int cell_y)
{
    if (!canvas || !canvas->cell_colors || cell_x < 0 || cell_y < 0 ||
        cell_x >= canvas->char_width || cell_y >= canvas->char_height) {
        return UINT16_MAX;
    }
    return canvas->cell_colors[(size_t)cell_y * (size_t)canvas->char_width +
                               (size_t)cell_x];
}

int braille_canvas_encode_cell(uint8_t mask, char out[4])
{
    unsigned int codepoint;

    if (!out) return -1;
    codepoint = 0x2800u + mask;
    out[0] = (char)(0xE0u | ((codepoint >> 12) & 0x0Fu));
    out[1] = (char)(0x80u | ((codepoint >> 6) & 0x3Fu));
    out[2] = (char)(0x80u | (codepoint & 0x3Fu));
    out[3] = '\0';
    return 3;
}

int braille_canvas_render(const BrailleCanvas *canvas,
                          WINDOW *win,
                          int origin_y,
                          int origin_x,
                          int max_rows,
                          int max_columns)
{
    int rows;
    int columns;

    if (!canvas || !canvas->cells || !win || max_rows <= 0 || max_columns <= 0) {
        return -1;
    }
    rows = canvas->char_height < max_rows ? canvas->char_height : max_rows;
    columns = canvas->char_width < max_columns ? canvas->char_width : max_columns;
    for (int y = 0; y < rows; ++y) {
        if (wmove(win, origin_y + y, origin_x) == ERR) continue;
        for (int x = 0; x < columns; ++x) {
            char glyph[4];

            braille_canvas_encode_cell(braille_canvas_cell(canvas, x, y), glyph);
            if (waddnstr(win, glyph, 3) == ERR) break;
        }
    }
    return rows * columns;
}
