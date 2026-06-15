#include "doc_runtime.h"

#include "editor_theme.h"
#include "form_runtime.h"
#include "panel_manager.h"
#include "syntax_highlighter.h"
#include "terminal_control.h"
#include "ui_footer.h"
#include "ui_text.h"

#include <locale.h>
#include <stdlib.h>
#include <string.h>

static int code_line_count(const char *code) {
    int lines = 1;
    if (!code || !*code) return lines;
    for (const char *cursor = code; *cursor; cursor++) {
        if (*cursor == '\n') lines++;
    }
    return lines;
}

static int doc_item_height(TFormDocItem *item, int width, int level);

static int doc_section_height(
    TFormDocSectionObject *section,
    int width,
    int level
) {
    if (!section || width <= 0) return 0;
    int box_rows = section->boxed ? 2 : 0;
    int inner_width = width - (section->boxed ? 4 : 0) - section->padding * 2;
    if (inner_width < 1) inner_width = 1;
    int height = box_rows + section->padding * 2;
    for (int i = 0; i < section->item_count; i++) {
        if (i > 0) height++;
        height += doc_item_height(&section->items[i], inner_width, level);
    }
    if (height < box_rows + 1) height = box_rows + 1;
    return height;
}

static int doc_item_height(TFormDocItem *item, int width, int level) {
    if (!item || width <= 0) return 0;
    switch (item->type) {
        case TFORM_DOC_ITEM_STATIC:
            return tform_static_element_height(&item->element, width);
        case TFORM_DOC_ITEM_PARAGRAPH:
        case TFORM_DOC_ITEM_HEADING:
            return tform_static_text_height(&item->text, width, 1);
        case TFORM_DOC_ITEM_CODE_BLOCK:
            return code_line_count(item->code) + (item->code_background ? 2 : 0);
        case TFORM_DOC_ITEM_SEPARATOR:
            return 1;
        case TFORM_DOC_ITEM_SECTION: {
            int child_width = width - 2;
            if (child_width < 1) child_width = 1;
            return doc_section_height(
                (TFormDocSectionObject *)item->section,
                child_width,
                level + 1
            );
        }
    }
    return 0;
}

static int doc_total_height(TFormDocObject *doc, int width) {
    int height = 0;
    if (!doc) return 0;
    for (int i = 0; i < doc->section_count; i++) {
        if (i > 0) height++;
        height += doc_section_height(
            (TFormDocSectionObject *)doc->sections[i], width, 0
        );
    }
    return height;
}

static void doc_content_bounds(
    TFormDocObject *doc,
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
    TFormDocObject *doc,
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
    int attr = ghostcode_color_pair(GHOSTCODE_PAIR_FOOTER_BORDER);
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
                ? GHOSTCODE_PAIR_DOC_SYNTAX_COMMENT
                : GHOSTCODE_PAIR_SYNTAX_COMMENT;
        case SYNTAX_TOKEN_STRING:
            return background
                ? GHOSTCODE_PAIR_DOC_SYNTAX_STRING
                : GHOSTCODE_PAIR_SYNTAX_STRING;
        case SYNTAX_TOKEN_NUMBER:
            return background
                ? GHOSTCODE_PAIR_DOC_SYNTAX_NUMBER
                : GHOSTCODE_PAIR_SYNTAX_NUMBER;
        case SYNTAX_TOKEN_KEYWORD:
            return background
                ? GHOSTCODE_PAIR_DOC_SYNTAX_KEYWORD
                : GHOSTCODE_PAIR_SYNTAX_KEYWORD;
        case SYNTAX_TOKEN_TYPE:
            return background
                ? GHOSTCODE_PAIR_DOC_SYNTAX_TYPE
                : GHOSTCODE_PAIR_SYNTAX_TYPE;
        case SYNTAX_TOKEN_PREPROCESSOR:
            return background
                ? GHOSTCODE_PAIR_DOC_SYNTAX_PREPROCESSOR
                : GHOSTCODE_PAIR_SYNTAX_PREPROCESSOR;
        case SYNTAX_TOKEN_OPERATOR:
            return background
                ? GHOSTCODE_PAIR_DOC_SYNTAX_OPERATOR
                : GHOSTCODE_PAIR_SYNTAX_OPERATOR;
        case SYNTAX_TOKEN_PLAIN:
            return background ? GHOSTCODE_PAIR_DOC_CODE : GHOSTCODE_PAIR_NORMAL;
    }
    return background ? GHOSTCODE_PAIR_DOC_CODE : GHOSTCODE_PAIR_NORMAL;
}

static void draw_syntax_token(
    SyntaxTokenKind kind,
    const char *text,
    size_t bytes,
    void *user_data
) {
    SyntaxDrawContext *draw = user_data;
    if (!draw || draw->remaining <= 0) return;
    int attr = ghostcode_color_pair(
        (short)syntax_token_pair(kind, draw->background)
    );
    wattron(draw->win, attr);
    int drawn = ghostcode_ui_text_addnstr_width(
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
    int plain_pair = background ? GHOSTCODE_PAIR_DOC_CODE : GHOSTCODE_PAIR_NORMAL;
    if (!syntax_highlight || language == SYNTAX_LANGUAGE_UNKNOWN) {
        wattron(win, ghostcode_color_pair((short)plain_pair));
        wmove(win, y, x);
        ghostcode_ui_text_addnstr_width(win, line, bytes, width);
        wattroff(win, ghostcode_color_pair((short)plain_pair));
        return;
    }

    char *line_copy = malloc(bytes + 1);
    if (!line_copy) {
        wattron(win, ghostcode_color_pair((short)plain_pair));
        wmove(win, y, x);
        ghostcode_ui_text_addnstr_width(win, line, bytes, width);
        wattroff(win, ghostcode_color_pair((short)plain_pair));
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
    TFormDocItem *item,
    int syntax_highlight,
    int clip_top,
    int clip_bottom
) {
    if (!win || !item || width <= 0) return;
    int row = y;
    int content_x = x;
    int content_width = width;
    if (item->code_background) {
        if (row >= clip_top && row < clip_bottom) {
            wattron(win, ghostcode_color_pair(GHOSTCODE_PAIR_DOC_CODE));
            mvwhline(win, row, x, ' ', width);
            wattroff(win, ghostcode_color_pair(GHOSTCODE_PAIR_DOC_CODE));
        }
        row++;
        content_x++;
        content_width -= 2;
        if (content_width < 1) content_width = 1;
    }

    const char *code = item->code ? item->code : "";
    SyntaxLanguage language = syntax_language_for_name(item->lang);
    size_t offset = 0;
    for (;;) {
        const char *line = code + offset;
        size_t bytes = strcspn(line, "\r\n");
        if (row >= clip_top && row < clip_bottom) {
            if (item->code_background) {
                wattron(win, ghostcode_color_pair(GHOSTCODE_PAIR_DOC_CODE));
                mvwhline(win, row, x, ' ', width);
                wattroff(win, ghostcode_color_pair(GHOSTCODE_PAIR_DOC_CODE));
            }
            draw_code_line(
                win, row, content_x, content_width, line, bytes,
                item->code_background, language, syntax_highlight
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

    if (item->code_background && row >= clip_top && row < clip_bottom) {
        wattron(win, ghostcode_color_pair(GHOSTCODE_PAIR_DOC_CODE));
        mvwhline(win, row, x, ' ', width);
        wattroff(win, ghostcode_color_pair(GHOSTCODE_PAIR_DOC_CODE));
    }
}

static void draw_separator(
    WINDOW *win,
    int y,
    int x,
    int width,
    TFormTextStyle *text,
    int clip_top,
    int clip_bottom
) {
    if (!win || y < clip_top || y >= clip_bottom || width <= 0) return;
    int attr = ghostcode_color_pair(GHOSTCODE_PAIR_FOOTER_BORDER);
    wattron(win, attr);
    mvwhline(win, y, x, ACS_HLINE, width);
    wattroff(win, attr);
    const char *label = text && text->value ? text->value : "";
    if (*label && width > 4) {
        int label_width = ghostcode_ui_text_width(label);
        if (label_width > width - 4) label_width = width - 4;
        int label_x = x + (width - label_width) / 2;
        wattron(win, ghostcode_color_pair(GHOSTCODE_PAIR_NORMAL) | A_BOLD);
        mvwaddch(win, y, label_x - 1, ' ');
        wmove(win, y, label_x);
        ghostcode_ui_text_addstr_width(win, label, label_width);
        waddch(win, ' ');
        wattroff(win, ghostcode_color_pair(GHOSTCODE_PAIR_NORMAL) | A_BOLD);
    }
}

static void draw_doc_section(
    WINDOW *win,
    int y,
    int x,
    int width,
    TFormDocSectionObject *section,
    int level,
    int syntax_highlight,
    int clip_top,
    int clip_bottom
) {
    if (!win || !section || width <= 0) return;
    int height = doc_section_height(section, width, level);
    int inner_x = x + section->padding;
    int inner_y = y + section->padding;
    int inner_width = width - section->padding * 2;
    if (section->boxed) {
        tform_draw_static_box(
            win, y, x, width, height, clip_top, clip_bottom
        );
        inner_x += 2;
        inner_y += 1;
        inner_width -= 4;
        if (y >= clip_top && y < clip_bottom &&
            section->title && *section->title && width > 6) {
            int title_width = ghostcode_ui_text_width(section->title);
            if (title_width > width - 6) title_width = width - 6;
            wattron(win, ghostcode_color_pair(GHOSTCODE_PAIR_FOOTER_BORDER) | A_BOLD);
            mvwaddch(win, y, x + 2, ' ');
            wmove(win, y, x + 3);
            ghostcode_ui_text_addstr_width(win, section->title, title_width);
            waddch(win, ' ');
            wattroff(win, ghostcode_color_pair(GHOSTCODE_PAIR_FOOTER_BORDER) | A_BOLD);
        }
    }
    if (inner_width < 1) inner_width = 1;

    int row = inner_y;
    for (int i = 0; i < section->item_count; i++) {
        if (i > 0) row++;
        TFormDocItem *item = &section->items[i];
        int item_height = doc_item_height(item, inner_width, level);
        switch (item->type) {
            case TFORM_DOC_ITEM_STATIC:
                tform_draw_static_element(
                    win, row, inner_x, inner_width, &item->element,
                    clip_top, clip_bottom
                );
                break;
            case TFORM_DOC_ITEM_PARAGRAPH:
            case TFORM_DOC_ITEM_HEADING:
                tform_draw_static_text(
                    win, row, inner_x, inner_width, item_height,
                    &item->text, 1, clip_top, clip_bottom
                );
                break;
            case TFORM_DOC_ITEM_CODE_BLOCK:
                draw_code_block(
                    win, row, inner_x, inner_width, item,
                    syntax_highlight,
                    clip_top, clip_bottom
                );
                break;
            case TFORM_DOC_ITEM_SEPARATOR:
                draw_separator(
                    win, row, inner_x, inner_width, &item->text,
                    clip_top, clip_bottom
                );
                break;
            case TFORM_DOC_ITEM_SECTION: {
                int child_x = inner_x + 2;
                int child_width = inner_width - 2;
                if (child_width < 1) {
                    child_x = inner_x;
                    child_width = inner_width;
                }
                draw_doc_section(
                    win, row, child_x, child_width,
                    (TFormDocSectionObject *)item->section,
                    level + 1, syntax_highlight, clip_top, clip_bottom
                );
                break;
            }
        }
        row += item_height;
    }
}

static void draw_doc(
    GhostcodePmNode *main_panel,
    GhostcodePmNode *footer_panel,
    TFormDocObject *doc,
    int *top
) {
    int body_h = LINES > 4 ? LINES - 3 : LINES - 1;
    int footer_h = LINES - body_h;
    if (footer_h < 1) footer_h = 1;
    ghostcode_pm_resize(main_panel, body_h, COLS);
    ghostcode_pm_resize(footer_panel, footer_h, COLS);
    ghostcode_pm_move(footer_panel, body_h, 0);
    werase(main_panel->win);
    wbkgd(main_panel->win, ghostcode_color_pair(GHOSTCODE_PAIR_NORMAL));
    wattron(main_panel->win, ghostcode_color_pair(GHOSTCODE_PAIR_HEADER));
    mvwhline(main_panel->win, 0, 0, ' ', COLS);
    mvwprintw(
        main_panel->win, 0, 1, "%.*s",
        COLS > 2 ? COLS - 2 : 0,
        doc->title ? doc->title : "TForm Doc"
    );
    wattroff(main_panel->win, ghostcode_color_pair(GHOSTCODE_PAIR_HEADER));

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
    for (int i = 0; i < doc->section_count; i++) {
        if (i > 0) row++;
        TFormDocSectionObject *section =
            (TFormDocSectionObject *)doc->sections[i];
        draw_doc_section(
            main_panel->win,
            row,
            content_x,
            content_width,
            section,
            0,
            doc->syntax_highlight,
            clip_top,
            clip_bottom
        );
        row += doc_section_height(section, content_width, 0);
    }

    ghostcode_ui_draw_footer(
        footer_panel->win,
        "[Up/Down] Scroll  [PgUp/PgDn] Page  [Home/End] Document  [Q/Esc] Exit"
    );
    ghostcode_pm_wnoutrefresh(main_panel);
    ghostcode_pm_wnoutrefresh(footer_panel);
    ghostcode_pm_update();
}

int tform_run_doc(TFormDocObject *doc) {
    setlocale(LC_ALL, "");
    ghostcode_disable_flow_control();
    setenv("TERM", "xterm-256color", 0);
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    set_escdelay(25);
    curs_set(0);
    ghostcode_init_editor_theme();
    ghostcode_pm_init();

    int body_h = LINES > 4 ? LINES - 3 : LINES - 1;
    int footer_h = LINES - body_h;
    if (footer_h < 1) footer_h = 1;
    GhostcodePmNode *main_panel = ghostcode_pm_add(
        0, 0, body_h, COLS,
        GHOSTCODE_PM_LAYER_MAIN, GHOSTCODE_PM_LAYER_MAIN
    );
    GhostcodePmNode *footer_panel = ghostcode_pm_add(
        body_h, 0, footer_h, COLS,
        GHOSTCODE_PM_LAYER_FOOTER, GHOSTCODE_PM_LAYER_FOOTER
    );
    if (!main_panel || !footer_panel) {
        ghostcode_pm_teardown();
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

    ghostcode_pm_teardown();
    endwin();
    return 0;
}
