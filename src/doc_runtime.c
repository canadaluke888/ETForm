#include "doc_runtime.h"

#include "editor_theme.h"
#include "form_runtime.h"
#include "panel_manager.h"
#include "syntax_highlighter.h"
#include "terminal_control.h"
#include "ui_footer.h"
#include "ui_text.h"

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static int code_line_count(const char *code) {
    int lines = 1;
    if (!code || !*code) return lines;
    for (const char *cursor = code; *cursor; cursor++) {
        if (*cursor == '\n') lines++;
    }
    return lines;
}

static int doc_node_height(ETFormDocNode *node, int width, int level);

static int doc_text_color_pair(ETFormTextColor color) {
    switch (color) {
        case ETFORM_TEXT_COLOR_WHITE:
            return ETFORM_PAIR_NORMAL;
        case ETFORM_TEXT_COLOR_CYAN:
            return ETFORM_PAIR_SYNTAX_KEYWORD;
        case ETFORM_TEXT_COLOR_YELLOW:
            return ETFORM_PAIR_SYNTAX_TYPE;
        case ETFORM_TEXT_COLOR_GREEN:
            return ETFORM_PAIR_SYNTAX_STRING;
        case ETFORM_TEXT_COLOR_BLUE:
            return ETFORM_PAIR_SYNTAX_COMMENT;
        case ETFORM_TEXT_COLOR_MAGENTA:
            return ETFORM_PAIR_SYNTAX_NUMBER;
        case ETFORM_TEXT_COLOR_RED:
            return ETFORM_PAIR_GUTTER_MARKER;
        case ETFORM_TEXT_COLOR_DEFAULT:
        default:
            return ETFORM_PAIR_NORMAL;
    }
}

static int doc_text_attr(const ETFormTextStyle *style) {
    int attr = etform_color_pair(ETFORM_PAIR_NORMAL);
    if (style && style->color != ETFORM_TEXT_COLOR_DEFAULT && has_colors()) {
        attr = COLOR_PAIR(doc_text_color_pair(style->color));
    }
    if (style && style->bold) attr |= A_BOLD;
#ifdef A_ITALIC
    if (style && style->italicized) attr |= A_ITALIC;
#endif
    if (style && style->dim) attr |= A_DIM;
    if (style && style->underline) attr |= A_UNDERLINE;
    if (style && style->blink) attr |= A_BLINK;
    return attr;
}

static size_t next_char_bytes(const char *text, size_t offset, int *display_width) {
    if (!text || !text[offset]) {
        if (display_width) *display_width = 0;
        return 0;
    }
    mbstate_t state;
    memset(&state, 0, sizeof(state));
    wchar_t ch;
    size_t consumed = mbrtowc(&ch, text + offset, strlen(text + offset), &state);
    if (consumed == (size_t)-1 || consumed == (size_t)-2 || consumed == 0) {
        if (display_width) *display_width = 1;
        return 1;
    }
    int width = etform_ui_text_width_n(text + offset, consumed);
    if (display_width) *display_width = width > 0 ? width : 1;
    return consumed;
}

static size_t next_word_bytes(const char *text, size_t offset) {
    size_t cursor = offset;
    if (!text || !text[offset]) return 0;
    while (text[cursor] &&
           text[cursor] != '\r' &&
           text[cursor] != '\n' &&
           text[cursor] != ' ' &&
           text[cursor] != '\t') {
        int width;
        size_t bytes = next_char_bytes(text, cursor, &width);
        if (bytes == 0) break;
        cursor += bytes;
    }
    return cursor - offset;
}

static int paragraph_height_prefixed(
    ETFormParagraphObject *paragraph,
    int width,
    int first_prefix_width,
    int continuation_prefix_width
) {
    if (!paragraph || width <= 0) return 1;
    int rows = 1;
    int col = first_prefix_width < width ? first_prefix_width : 0;
    for (Py_ssize_t i = 0; i < paragraph->run_count; i++) {
        ETFormTextStyle style;
        if (etform_text_style_from_object(paragraph->runs[i].object, 1, &style) < 0) {
            PyErr_Clear();
            continue;
        }
        const char *text = style.value ? style.value : "";
        size_t offset = 0;
        while (text[offset]) {
            if (text[offset] == '\r' || text[offset] == '\n') {
                if (text[offset] == '\r' && text[offset + 1] == '\n') offset++;
                offset++;
                rows++;
                col = continuation_prefix_width < width ? continuation_prefix_width : 0;
                continue;
            }
            if (text[offset] == ' ' || text[offset] == '\t') {
                if (col == first_prefix_width || col == continuation_prefix_width) {
                    offset++;
                    continue;
                }
                if (col + 1 > width) {
                    rows++;
                    col = continuation_prefix_width < width ? continuation_prefix_width : 0;
                    offset++;
                    continue;
                }
                col++;
                offset++;
                continue;
            }
            size_t bytes = next_word_bytes(text, offset);
            int token_width = etform_ui_text_width_n(text + offset, bytes);
            if (col > continuation_prefix_width && col + token_width > width) {
                rows++;
                col = continuation_prefix_width < width ? continuation_prefix_width : 0;
            }
            if (token_width <= width - col) {
                col += token_width;
                offset += bytes;
                continue;
            }
            size_t token_offset = 0;
            while (token_offset < bytes) {
                int char_width = 1;
                size_t char_bytes = next_char_bytes(text, offset + token_offset, &char_width);
                if (char_bytes == 0) break;
                if (col > continuation_prefix_width && col + char_width > width) {
                    rows++;
                    col = continuation_prefix_width < width ? continuation_prefix_width : 0;
                }
                col += char_width;
                token_offset += char_bytes;
            }
            offset += bytes;
        }
        etform_text_style_free(&style);
    }
    return rows;
}

static int paragraph_height(ETFormParagraphObject *paragraph, int width) {
    return paragraph_height_prefixed(paragraph, width, 0, 0);
}

static void draw_paragraph_prefixed(
    WINDOW *win,
    int y,
    int x,
    int width,
    ETFormParagraphObject *paragraph,
    const char *first_prefix,
    int continuation_prefix_width,
    int clip_top,
    int clip_bottom
) {
    if (!win || !paragraph || width <= 0) return;
    int row = 0;
    int first_prefix_width = etform_ui_text_width(first_prefix ? first_prefix : "");
    int col = first_prefix_width < width ? first_prefix_width : 0;
    if (first_prefix && *first_prefix && y >= clip_top && y < clip_bottom) {
        wattron(win, etform_color_pair(ETFORM_PAIR_NORMAL));
        wmove(win, y, x);
        etform_ui_text_addstr_width(win, first_prefix, width);
        wattroff(win, etform_color_pair(ETFORM_PAIR_NORMAL));
    }
    for (Py_ssize_t i = 0; i < paragraph->run_count; i++) {
        ETFormTextStyle style;
        if (etform_text_style_from_object(paragraph->runs[i].object, 1, &style) < 0) {
            PyErr_Clear();
            continue;
        }
        const char *text = style.value ? style.value : "";
        int attr = doc_text_attr(&style);
        size_t offset = 0;
        while (text[offset]) {
            if (text[offset] == '\r' || text[offset] == '\n') {
                if (text[offset] == '\r' && text[offset + 1] == '\n') offset++;
                offset++;
                row++;
                col = continuation_prefix_width < width ? continuation_prefix_width : 0;
                continue;
            }
            if (text[offset] == ' ' || text[offset] == '\t') {
                if (col == first_prefix_width || col == continuation_prefix_width) {
                    offset++;
                    continue;
                }
                if (col + 1 > width) {
                    row++;
                    col = continuation_prefix_width < width ? continuation_prefix_width : 0;
                    offset++;
                    continue;
                }
                int draw_y = y + row;
                if (draw_y >= clip_top && draw_y < clip_bottom) {
                    wattron(win, attr);
                    mvwaddch(win, draw_y, x + col, ' ');
                    wattroff(win, attr);
                }
                col++;
                offset++;
                continue;
            }
            size_t bytes = next_word_bytes(text, offset);
            int token_width = etform_ui_text_width_n(text + offset, bytes);
            if (col > continuation_prefix_width && col + token_width > width) {
                row++;
                col = continuation_prefix_width < width ? continuation_prefix_width : 0;
            }
            if (token_width <= width - col) {
                int draw_y = y + row;
                if (draw_y >= clip_top && draw_y < clip_bottom) {
                    wattron(win, attr);
                    wmove(win, draw_y, x + col);
                    etform_ui_text_addnstr_width(win, text + offset, bytes, width - col);
                    wattroff(win, attr);
                }
                col += token_width;
                offset += bytes;
                continue;
            }
            size_t token_offset = 0;
            while (token_offset < bytes) {
                int char_width = 1;
                size_t char_bytes = next_char_bytes(text, offset + token_offset, &char_width);
                if (char_bytes == 0) break;
                if (col > continuation_prefix_width && col + char_width > width) {
                    row++;
                    col = continuation_prefix_width < width ? continuation_prefix_width : 0;
                }
                int draw_y = y + row;
                if (draw_y >= clip_top && draw_y < clip_bottom) {
                    wattron(win, attr);
                    wmove(win, draw_y, x + col);
                    etform_ui_text_addnstr_width(
                        win, text + offset + token_offset, char_bytes, width - col
                    );
                    wattroff(win, attr);
                }
                col += char_width;
                token_offset += char_bytes;
            }
            offset += bytes;
        }
        etform_text_style_free(&style);
    }
}

static int doc_list_height(ETFormDocListObject *list, int width, int level) {
    if (!list || width <= 0) return 0;
    int height = 0;
    int indent = level * 2;
    if (indent > width - 1) indent = width > 1 ? width - 1 : 0;
    for (Py_ssize_t i = 0; i < list->item_count; i++) {
        ETFormDocListItem *item = &list->items[i];
        if (item->kind == ETFORM_DOC_LIST_ITEM_ORDERED_LIST ||
            item->kind == ETFORM_DOC_LIST_ITEM_UNORDERED_LIST) {
            height += doc_list_height((ETFormDocListObject *)item->object, width, level + 1);
            continue;
        }
        char marker[32];
        if (list->ordered) {
            snprintf(marker, sizeof(marker), "%*s%zd. ", indent, "", i + 1);
        } else {
            snprintf(marker, sizeof(marker), "%*s• ", indent, "");
        }
        int prefix_width = etform_ui_text_width(marker);
        height += paragraph_height_prefixed(
            (ETFormParagraphObject *)item->object,
            width,
            prefix_width,
            prefix_width
        );
    }
    return height > 0 ? height : 1;
}

static int doc_group_height(
    ETFormDocGroupObject *group,
    int width,
    int level
) {
    if (!group || width <= 0) return 0;
    int box_rows = group->boxed ? 2 : 0;
    int inner_width = width - (group->boxed ? 4 : 0) - group->padding * 2;
    if (inner_width < 1) inner_width = 1;
    int height = box_rows + group->padding * 2;
    for (Py_ssize_t i = 0; i < group->child_count; i++) {
        if (i > 0) height++;
        height += doc_node_height(&group->children[i], inner_width, level + 1);
    }
    if (height < box_rows + 1) height = box_rows + 1;
    return height;
}

static int static_object_height(PyObject *object, int width) {
    ETFormSectionElement element;
    memset(&element, 0, sizeof(element));
    if (etform_section_element_from_object(&element, object) < 0) {
        PyErr_Clear();
        return 0;
    }
    int height = etform_static_element_height(&element, width);
    etform_section_element_free(&element);
    return height;
}

static int code_block_height(ETFormCodeBlockObject *block) {
    if (!block) return 0;
    return code_line_count(block->code) + (block->background ? 2 : 0);
}

static int doc_node_height(ETFormDocNode *node, int width, int level) {
    if (!node || width <= 0) return 0;
    switch (node->kind) {
        case ETFORM_DOC_NODE_PARAGRAPH:
            return paragraph_height((ETFormParagraphObject *)node->object, width);
        case ETFORM_DOC_NODE_GROUP:
            return doc_group_height((ETFormDocGroupObject *)node->object, width, level);
        case ETFORM_DOC_NODE_TABLE:
        case ETFORM_DOC_NODE_BRAILLE_CANVAS:
        case ETFORM_DOC_NODE_IMAGE:
            return static_object_height(node->object, width);
        case ETFORM_DOC_NODE_CODE_BLOCK:
            return code_block_height((ETFormCodeBlockObject *)node->object);
        case ETFORM_DOC_NODE_ORDERED_LIST:
        case ETFORM_DOC_NODE_UNORDERED_LIST:
            return doc_list_height((ETFormDocListObject *)node->object, width, level);
    }
    return 0;
}

static int doc_total_height(ETFormDocObject *doc, int width) {
    int height = 0;
    if (!doc) return 0;
    for (Py_ssize_t i = 0; i < doc->child_count; i++) {
        if (i > 0) height++;
        height += doc_node_height(&doc->children[i], width, 0);
    }
    return height;
}

static void doc_content_bounds(
    ETFormDocObject *doc,
    int columns,
    int *x,
    int *width
) {
    int content_x = doc ? doc->margin_left : 0;
    int right = doc ? doc->margin_right : 0;
    if (columns < 1) columns = 1;
    if (content_x < 0) content_x = 0;
    if (content_x >= columns) content_x = columns - 1;
    if (right < 0) right = 0;
    int content_width = columns - content_x - right;
    if (content_width < 1) content_width = 1;
    *x = content_x;
    *width = content_width;
}

static void doc_viewport_bounds(
    ETFormDocObject *doc,
    int body_h,
    int *top,
    int *bottom
) {
    int available = body_h - 1;
    if (available < 1) {
        *top = 0;
        *bottom = body_h > 0 ? body_h : 1;
        return;
    }
    int top_margin = doc ? doc->margin_top : 0;
    int bottom_margin = doc ? doc->margin_bottom : 0;
    if (top_margin < 0) top_margin = 0;
    if (top_margin > available - 1) top_margin = available - 1;
    int remaining = available - top_margin;
    if (bottom_margin < 0) bottom_margin = 0;
    if (bottom_margin > remaining - 1) bottom_margin = remaining - 1;
    *top = 1 + top_margin;
    *bottom = body_h - bottom_margin;
}

static void draw_doc_border(
    WINDOW *win,
    int x,
    int width,
    int top,
    int height,
    int clip_top,
    int clip_bottom
) {
    if (!win || width < 2 || height < 2) return;
    int right = x + width - 1;
    int last_row = top + height - 1;
    int attr = etform_color_pair(ETFORM_PAIR_FOOTER_BORDER);
    wattron(win, attr);
    if (top >= clip_top && top < clip_bottom) {
        mvwaddstr(win, top, x, "┌");
        for (int col = x + 1; col < right; col++) {
            mvwaddstr(win, top, col, "─");
        }
        mvwaddstr(win, top, right, "┐");
    }
    int side_top = top + 1;
    if (side_top < clip_top) side_top = clip_top;
    int side_bottom = last_row;
    if (side_bottom > clip_bottom) side_bottom = clip_bottom;
    for (int row = side_top; row < side_bottom; row++) {
        mvwaddstr(win, row, x, "│");
        mvwaddstr(win, row, right, "│");
    }
    if (last_row >= clip_top && last_row < clip_bottom) {
        mvwaddstr(win, last_row, x, "└");
        for (int col = x + 1; col < right; col++) {
            mvwaddstr(win, last_row, col, "─");
        }
        mvwaddstr(win, last_row, right, "┘");
    }
    wattroff(win, attr);
}

typedef struct {
    WINDOW *win;
    int remaining;
    int background;
} SyntaxDrawContext;

static int syntax_token_pair(SyntaxTokenKind kind, int background) {
    switch (kind) {
        case SYNTAX_TOKEN_COMMENT:
            return background
                ? ETFORM_PAIR_DOC_SYNTAX_COMMENT
                : ETFORM_PAIR_SYNTAX_COMMENT;
        case SYNTAX_TOKEN_STRING:
            return background
                ? ETFORM_PAIR_DOC_SYNTAX_STRING
                : ETFORM_PAIR_SYNTAX_STRING;
        case SYNTAX_TOKEN_NUMBER:
            return background
                ? ETFORM_PAIR_DOC_SYNTAX_NUMBER
                : ETFORM_PAIR_SYNTAX_NUMBER;
        case SYNTAX_TOKEN_KEYWORD:
            return background
                ? ETFORM_PAIR_DOC_SYNTAX_KEYWORD
                : ETFORM_PAIR_SYNTAX_KEYWORD;
        case SYNTAX_TOKEN_TYPE:
            return background
                ? ETFORM_PAIR_DOC_SYNTAX_TYPE
                : ETFORM_PAIR_SYNTAX_TYPE;
        case SYNTAX_TOKEN_PREPROCESSOR:
            return background
                ? ETFORM_PAIR_DOC_SYNTAX_PREPROCESSOR
                : ETFORM_PAIR_SYNTAX_PREPROCESSOR;
        case SYNTAX_TOKEN_OPERATOR:
            return background
                ? ETFORM_PAIR_DOC_SYNTAX_OPERATOR
                : ETFORM_PAIR_SYNTAX_OPERATOR;
        case SYNTAX_TOKEN_PLAIN:
            return background ? ETFORM_PAIR_DOC_CODE : ETFORM_PAIR_NORMAL;
    }
    return background ? ETFORM_PAIR_DOC_CODE : ETFORM_PAIR_NORMAL;
}

static void draw_syntax_token(
    SyntaxTokenKind kind,
    const char *text,
    size_t bytes,
    void *user_data
) {
    SyntaxDrawContext *draw = user_data;
    if (!draw || draw->remaining <= 0) return;
    int attr = etform_color_pair(
        (short)syntax_token_pair(kind, draw->background)
    );
    wattron(draw->win, attr);
    int drawn = etform_ui_text_addnstr_width(
        draw->win, text, bytes, draw->remaining
    );
    wattroff(draw->win, attr);
    draw->remaining -= drawn;
}

static void draw_code_line(
    WINDOW *win,
    int y,
    int x,
    int width,
    const char *line,
    size_t bytes,
    int background,
    SyntaxLanguage language,
    int syntax_highlight
) {
    int plain_pair = background ? ETFORM_PAIR_DOC_CODE : ETFORM_PAIR_NORMAL;
    if (!syntax_highlight || language == SYNTAX_LANGUAGE_UNKNOWN) {
        wattron(win, etform_color_pair((short)plain_pair));
        wmove(win, y, x);
        etform_ui_text_addnstr_width(win, line, bytes, width);
        wattroff(win, etform_color_pair((short)plain_pair));
        return;
    }

    char *line_copy = malloc(bytes + 1);
    if (!line_copy) {
        wattron(win, etform_color_pair((short)plain_pair));
        wmove(win, y, x);
        etform_ui_text_addnstr_width(win, line, bytes, width);
        wattroff(win, etform_color_pair((short)plain_pair));
        return;
    }
    memcpy(line_copy, line, bytes);
    line_copy[bytes] = '\0';

    SyntaxDrawContext context = {win, width, background};
    wmove(win, y, x);
    syntax_highlight_line(language, line_copy, draw_syntax_token, &context);
    free(line_copy);
}

static void draw_code_block(
    WINDOW *win,
    int y,
    int x,
    int width,
    const char *code,
    const char *lang,
    int background,
    int syntax_highlight,
    int clip_top,
    int clip_bottom
) {
    if (!win || width <= 0) return;
    int row = y;
    int content_x = x;
    int content_width = width;
    if (background) {
        if (row >= clip_top && row < clip_bottom) {
            wattron(win, etform_color_pair(ETFORM_PAIR_DOC_CODE));
            mvwhline(win, row, x, ' ', width);
            wattroff(win, etform_color_pair(ETFORM_PAIR_DOC_CODE));
        }
        row++;
        content_x++;
        content_width -= 2;
        if (content_width < 1) content_width = 1;
    }

    code = code ? code : "";
    SyntaxLanguage language = syntax_language_for_name(lang);
    size_t offset = 0;
    for (;;) {
        const char *line = code + offset;
        size_t bytes = strcspn(line, "\r\n");
        if (row >= clip_top && row < clip_bottom) {
            if (background) {
                wattron(win, etform_color_pair(ETFORM_PAIR_DOC_CODE));
                mvwhline(win, row, x, ' ', width);
                wattroff(win, etform_color_pair(ETFORM_PAIR_DOC_CODE));
            }
            draw_code_line(
                win, row, content_x, content_width, line, bytes,
                background, language, syntax_highlight
            );
        }
        row++;
        offset += bytes;
        if (!code[offset]) break;
        if (code[offset] == '\r' && code[offset + 1] == '\n') {
            offset += 2;
        } else {
            offset++;
        }
    }

    if (background && row >= clip_top && row < clip_bottom) {
        wattron(win, etform_color_pair(ETFORM_PAIR_DOC_CODE));
        mvwhline(win, row, x, ' ', width);
        wattroff(win, etform_color_pair(ETFORM_PAIR_DOC_CODE));
    }
}

static void draw_static_object(
    WINDOW *win,
    int y,
    int x,
    int width,
    PyObject *object,
    int clip_top,
    int clip_bottom
) {
    ETFormSectionElement element;
    memset(&element, 0, sizeof(element));
    if (etform_section_element_from_object(&element, object) < 0) {
        PyErr_Clear();
        return;
    }
    etform_draw_static_element(win, y, x, width, &element, clip_top, clip_bottom);
    etform_section_element_free(&element);
}

static void draw_code_block_object(
    WINDOW *win,
    int y,
    int x,
    int width,
    ETFormCodeBlockObject *block,
    int syntax_highlight,
    int clip_top,
    int clip_bottom
) {
    if (!block) return;
    draw_code_block(
        win, y, x, width, block->code, block->lang, block->background,
        syntax_highlight, clip_top, clip_bottom
    );
}

static void draw_doc_list(
    WINDOW *win,
    int y,
    int x,
    int width,
    ETFormDocListObject *list,
    int level,
    int clip_top,
    int clip_bottom
) {
    if (!win || !list || width <= 0) return;
    int row = y;
    int indent = level * 2;
    if (indent > width - 1) indent = width > 1 ? width - 1 : 0;
    for (Py_ssize_t i = 0; i < list->item_count; i++) {
        ETFormDocListItem *item = &list->items[i];
        if (item->kind == ETFORM_DOC_LIST_ITEM_ORDERED_LIST ||
            item->kind == ETFORM_DOC_LIST_ITEM_UNORDERED_LIST) {
            int nested_height = doc_list_height(
                (ETFormDocListObject *)item->object, width, level + 1
            );
            draw_doc_list(
                win, row, x, width, (ETFormDocListObject *)item->object,
                level + 1, clip_top, clip_bottom
            );
            row += nested_height;
            continue;
        }
        char marker[32];
        if (list->ordered) {
            snprintf(marker, sizeof(marker), "%*s%zd. ", indent, "", i + 1);
        } else {
            snprintf(marker, sizeof(marker), "%*s• ", indent, "");
        }
        int prefix_width = etform_ui_text_width(marker);
        int height = paragraph_height_prefixed(
            (ETFormParagraphObject *)item->object,
            width,
            prefix_width,
            prefix_width
        );
        draw_paragraph_prefixed(
            win, row, x, width, (ETFormParagraphObject *)item->object,
            marker, prefix_width, clip_top, clip_bottom
        );
        row += height;
    }
}

static void draw_doc_node(
    WINDOW *win,
    int y,
    int x,
    int width,
    ETFormDocNode *node,
    int level,
    int syntax_highlight,
    int clip_top,
    int clip_bottom
);

static void draw_doc_group(
    WINDOW *win,
    int y,
    int x,
    int width,
    ETFormDocGroupObject *group,
    int level,
    int syntax_highlight,
    int clip_top,
    int clip_bottom
) {
    if (!win || !group || width <= 0) return;
    int height = doc_group_height(group, width, level);
    int inner_x = x + group->padding;
    int inner_y = y + group->padding;
    int inner_width = width - group->padding * 2;
    if (group->boxed) {
        etform_draw_static_box(
            win, y, x, width, height, clip_top, clip_bottom
        );
        inner_x += 2;
        inner_y += 1;
        inner_width -= 4;
        if (y >= clip_top && y < clip_bottom &&
            group->label && *group->label && width > 6) {
            int title_width = etform_ui_text_width(group->label);
            if (title_width > width - 6) title_width = width - 6;
            wattron(win, etform_color_pair(ETFORM_PAIR_FOOTER_BORDER) | A_BOLD);
            mvwaddch(win, y, x + 2, ' ');
            wmove(win, y, x + 3);
            etform_ui_text_addstr_width(win, group->label, title_width);
            waddch(win, ' ');
            wattroff(win, etform_color_pair(ETFORM_PAIR_FOOTER_BORDER) | A_BOLD);
        }
    }
    if (inner_width < 1) inner_width = 1;

    int row = inner_y;
    for (Py_ssize_t i = 0; i < group->child_count; i++) {
        if (i > 0) row++;
        ETFormDocNode *node = &group->children[i];
        int item_height = doc_node_height(node, inner_width, level + 1);
        draw_doc_node(
            win, row, inner_x, inner_width, node,
            level + 1, syntax_highlight, clip_top, clip_bottom
        );
        row += item_height;
    }
}

static void draw_doc_node(
    WINDOW *win,
    int y,
    int x,
    int width,
    ETFormDocNode *node,
    int level,
    int syntax_highlight,
    int clip_top,
    int clip_bottom
) {
    if (!win || !node || width <= 0) return;
    switch (node->kind) {
        case ETFORM_DOC_NODE_PARAGRAPH:
            draw_paragraph_prefixed(
                win, y, x, width, (ETFormParagraphObject *)node->object,
                "", 0, clip_top, clip_bottom
            );
            break;
        case ETFORM_DOC_NODE_GROUP:
            draw_doc_group(
                win, y, x, width, (ETFormDocGroupObject *)node->object,
                level, syntax_highlight, clip_top, clip_bottom
            );
            break;
        case ETFORM_DOC_NODE_TABLE:
        case ETFORM_DOC_NODE_BRAILLE_CANVAS:
        case ETFORM_DOC_NODE_IMAGE:
            draw_static_object(win, y, x, width, node->object, clip_top, clip_bottom);
            break;
        case ETFORM_DOC_NODE_CODE_BLOCK:
            draw_code_block_object(
                win, y, x, width, (ETFormCodeBlockObject *)node->object,
                syntax_highlight, clip_top, clip_bottom
            );
            break;
        case ETFORM_DOC_NODE_ORDERED_LIST:
        case ETFORM_DOC_NODE_UNORDERED_LIST:
            draw_doc_list(
                win, y, x, width, (ETFormDocListObject *)node->object,
                level, clip_top, clip_bottom
            );
            break;
    }
}

static void draw_doc(
    ETFormPmNode *main_panel,
    ETFormPmNode *footer_panel,
    ETFormDocObject *doc,
    int *top
) {
    int body_h = LINES > 4 ? LINES - 3 : LINES - 1;
    int footer_h = LINES - body_h;
    if (footer_h < 1) footer_h = 1;
    etform_pm_resize(main_panel, body_h, COLS);
    etform_pm_resize(footer_panel, footer_h, COLS);
    etform_pm_move(footer_panel, body_h, 0);
    werase(main_panel->win);
    wbkgd(main_panel->win, etform_color_pair(ETFORM_PAIR_NORMAL));
    wattron(main_panel->win, etform_color_pair(ETFORM_PAIR_HEADER));
    mvwhline(main_panel->win, 0, 0, ' ', COLS);
    mvwprintw(
        main_panel->win, 0, 1, "%.*s",
        COLS > 2 ? COLS - 2 : 0,
        doc->title ? doc->title : "ETForm Doc"
    );
    wattroff(main_panel->win, etform_color_pair(ETFORM_PAIR_HEADER));

    int content_x;
    int content_width;
    doc_content_bounds(doc, COLS, &content_x, &content_width);

    int clip_top;
    int clip_bottom;
    doc_viewport_bounds(doc, body_h, &clip_top, &clip_bottom);
    int border_visible = doc->show_border &&
                         content_width >= 3;
    if (border_visible) {
        if (content_width >= 3) {
            content_x++;
            content_width -= 2;
        }
    }
    int content_height = doc_total_height(doc, content_width);
    int document_height = content_height + (border_visible ? 2 : 0);
    int viewport = clip_bottom - clip_top;
    if (viewport < 1) viewport = 1;
    int max_top = document_height > viewport ? document_height - viewport : 0;
    if (*top < 0) *top = 0;
    if (*top > max_top) *top = max_top;

    int document_y = clip_top - *top;
    if (border_visible) {
        draw_doc_border(
            main_panel->win,
            content_x - 1,
            content_width + 2,
            document_y,
            document_height,
            clip_top,
            clip_bottom
        );
    }

    int row = document_y + (border_visible ? 1 : 0);
    for (Py_ssize_t i = 0; i < doc->child_count; i++) {
        if (i > 0) row++;
        ETFormDocNode *node = &doc->children[i];
        draw_doc_node(
            main_panel->win,
            row,
            content_x,
            content_width,
            node,
            0,
            doc->syntax_highlight,
            clip_top,
            clip_bottom
        );
        row += doc_node_height(node, content_width, 0);
    }

    etform_ui_draw_footer(
        footer_panel->win,
        "[Up/Down] Scroll  [PgUp/PgDn] Page  [Home/End] Document  [Q/Esc] Exit"
    );
    etform_pm_wnoutrefresh(main_panel);
    etform_pm_wnoutrefresh(footer_panel);
    etform_pm_update();
}

int etform_run_doc(ETFormDocObject *doc) {
    setlocale(LC_ALL, "");
    etform_disable_flow_control();
    setenv("TERM", "xterm-256color", 0);
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    set_escdelay(25);
    curs_set(0);
    etform_init_editor_theme();
    etform_pm_init();

    int body_h = LINES > 4 ? LINES - 3 : LINES - 1;
    int footer_h = LINES - body_h;
    if (footer_h < 1) footer_h = 1;
    ETFormPmNode *main_panel = etform_pm_add(
        0, 0, body_h, COLS,
        ETFORM_PM_LAYER_MAIN, ETFORM_PM_LAYER_MAIN
    );
    ETFormPmNode *footer_panel = etform_pm_add(
        body_h, 0, footer_h, COLS,
        ETFORM_PM_LAYER_FOOTER, ETFORM_PM_LAYER_FOOTER
    );
    if (!main_panel || !footer_panel) {
        etform_pm_teardown();
        endwin();
        return -1;
    }
    keypad(main_panel->win, TRUE);

    int top = 0;
    for (;;) {
        draw_doc(main_panel, footer_panel, doc, &top);
        int content_x;
        int content_width;
        doc_content_bounds(doc, COLS, &content_x, &content_width);
        (void)content_x;
        int viewport_top;
        int viewport_bottom;
        doc_viewport_bounds(
            doc,
            LINES > 4 ? LINES - 3 : LINES - 1,
            &viewport_top,
            &viewport_bottom
        );
        int border_visible = doc->show_border &&
                             content_width >= 3;
        if (border_visible) {
            if (content_width >= 3) content_width -= 2;
        }
        int viewport = viewport_bottom - viewport_top;
        if (viewport < 1) viewport = 1;
        int total_height = doc_total_height(doc, content_width) +
                           (border_visible ? 2 : 0);
        int max_top = total_height > viewport ? total_height - viewport : 0;
        int ch = wgetch(main_panel->win);
        if (ch == 'q' || ch == 'Q' || ch == 27) break;
        if (ch == KEY_UP) {
            if (top > 0) top--;
        } else if (ch == KEY_DOWN) {
            if (top < max_top) top++;
        } else if (ch == KEY_PPAGE) {
            top -= viewport > 2 ? viewport - 2 : 1;
            if (top < 0) top = 0;
        } else if (ch == KEY_NPAGE) {
            top += viewport > 2 ? viewport - 2 : 1;
            if (top > max_top) top = max_top;
        } else if (ch == KEY_HOME) {
            top = 0;
        } else if (ch == KEY_END) {
            top = max_top;
        }
    }

    etform_pm_teardown();
    endwin();
    return 0;
}
