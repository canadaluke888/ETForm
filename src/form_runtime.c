#include "form_runtime.h"

#include "dialogs.h"
#include "editor_theme.h"
#include "panel_manager.h"
#include "terminal_control.h"
#include "ui_footer.h"
#include "ui_list.h"
#include "ui_text.h"

#include <curses.h>
#include <locale.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define CTRL_S 19
#define CTRL_E 5
#define CTRL_V 22
#define NUMERIC_ACCEL_WINDOW_MS 180

typedef enum {
    TFORM_EDIT_NONE,
    TFORM_EDIT_SCROLL,
    TFORM_EDIT_TEXT,
    TFORM_EDIT_LIST,
    TFORM_EDIT_CHECKBOX
} TFormEditMode;

typedef struct {
    int key;
    int field;
    int repeats;
    long long last_ms;
} NumericScrollState;

static void trim_in_place(char *value) {
    if (!value) {
        return;
    }
    char *start = value;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') {
        start++;
    }
    char *end = start + strlen(start);
    while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r')) {
        end--;
    }
    *end = '\0';
    if (start != value) {
        memmove(value, start, strlen(start) + 1);
    }
}

static void replace_string(char **target, char *value, int allow_empty) {
    if (!target || !value) {
        free(value);
        return;
    }
    trim_in_place(value);
    if (!allow_empty && !*value) {
        free(value);
        return;
    }
    free(*target);
    *target = value;
}

static int text_field_wrap_line_count(const char *value, int max_cols) {
    if (max_cols < 1) {
        max_cols = 1;
    }
    size_t length = value ? strlen(value) : 0;
    return (int)(length / (size_t)max_cols) + 1;
}

static int text_field_insert_char(TFormField *field, int *cursor, char ch, int max_cols) {
    if (!field || !cursor || max_cols <= 0) {
        return 0;
    }
    if (!field->text_value) {
        field->text_value = strdup("");
        if (!field->text_value) {
            return 0;
        }
    }
    if (ch < 32 || ch > 126) {
        return 0;
    }
    size_t length = strlen(field->text_value);
    if (!field->wrap_text && (int)length >= max_cols) {
        return 0;
    }
    if (*cursor < 0) {
        *cursor = 0;
    }
    if ((size_t)*cursor > length) {
        *cursor = (int)length;
    }
    char *next = realloc(field->text_value, length + 2);
    if (!next) {
        return 0;
    }
    field->text_value = next;
    memmove(field->text_value + *cursor + 1, field->text_value + *cursor, length - (size_t)*cursor + 1);
    field->text_value[*cursor] = ch;
    (*cursor)++;
    return 1;
}

static void text_field_insert_text(TFormField *field, int *cursor, const char *text, int max_cols) {
    if (!field || !cursor || !text) {
        return;
    }
    for (size_t i = 0; text[i]; i++) {
        char ch = text[i];
        if (ch == '\r' || ch == '\n' || ch == '\t') {
            ch = ' ';
        }
        text_field_insert_char(field, cursor, ch, max_cols);
    }
}

static void text_field_backspace(TFormField *field, int *cursor) {
    if (!field || !cursor || !field->text_value) {
        return;
    }
    size_t length = strlen(field->text_value);
    if (*cursor <= 0 || length == 0) {
        return;
    }
    if ((size_t)*cursor > length) {
        *cursor = (int)length;
    }
    memmove(field->text_value + *cursor - 1, field->text_value + *cursor, length - (size_t)*cursor + 1);
    (*cursor)--;
}

static void text_field_delete_at_cursor(TFormField *field, int cursor) {
    if (!field || !field->text_value) {
        return;
    }
    size_t length = strlen(field->text_value);
    if (cursor < 0 || (size_t)cursor >= length) {
        return;
    }
    memmove(field->text_value + cursor, field->text_value + cursor + 1, length - (size_t)cursor);
}

static void text_field_move_cursor(TFormField *field, int *cursor, int dir, int max_cols) {
    if (!field || !cursor) {
        return;
    }
    int length = field->text_value ? (int)strlen(field->text_value) : 0;
    if (*cursor < 0) {
        *cursor = 0;
    }
    if (*cursor > length) {
        *cursor = length;
    }
    if (dir < 0 && *cursor > 0) {
        (*cursor)--;
    } else if (dir > 0 && *cursor < length) {
        (*cursor)++;
    }
    (void)max_cols;
}

static void text_field_move_cursor_vertical(TFormField *field, int *cursor, int dir, int max_cols) {
    if (!field || !cursor || max_cols <= 0) {
        return;
    }
    int length = field->text_value ? (int)strlen(field->text_value) : 0;
    if (*cursor < 0) {
        *cursor = 0;
    }
    if (*cursor > length) {
        *cursor = length;
    }
    int next = *cursor + (dir * max_cols);
    if (next < 0) {
        next = 0;
    }
    if (next > length) {
        next = length;
    }
    *cursor = next;
}

static void text_field_cursor_position(const char *value, int cursor, int max_cols, int *row, int *col) {
    int length = value ? (int)strlen(value) : 0;
    if (max_cols < 1) {
        max_cols = 1;
    }
    if (cursor < 0) {
        cursor = 0;
    }
    if (cursor > length) {
        cursor = length;
    }
    if (row) {
        *row = cursor / max_cols;
    }
    if (col) {
        *col = cursor % max_cols;
    }
}

static void text_field_clear(TFormField *field) {
    if (!field) {
        return;
    }
    free(field->text_value);
    field->text_value = strdup("");
}

static int list_field_append_char(TFormField *field, int *selected, char ch, int max_cols) {
    if (!field || !selected || field->kind != TFORM_FIELD_LIST || max_cols <= 0) {
        return 0;
    }
    if (field->list_value.count <= 0) {
        if (tform_list_append_value(field, " ", NULL) < 0) {
            return 0;
        }
        field->list_value.items[0][0] = '\0';
        if (field->list_styles && field->list_styles[0].value) {
            field->list_styles[0].value[0] = '\0';
        }
        *selected = 0;
    }
    if (*selected < 0 || *selected >= field->list_value.count) {
        *selected = 0;
    }
    char *value = field->list_value.items[*selected];
    size_t length = value ? strlen(value) : 0;
    int content_cols = max_cols - 2;
    if (content_cols < 1 || (int)length >= content_cols || ch < 32 || ch > 126) {
        return 0;
    }
    char *next = realloc(value, length + 2);
    if (!next) {
        return 0;
    }
    next[length] = ch;
    next[length + 1] = '\0';
    field->list_value.items[*selected] = next;
    return 1;
}

static void list_field_append_text(TFormField *field, int *selected, const char *text, int max_cols) {
    if (!field || !selected || !text) {
        return;
    }
    for (size_t i = 0; text[i]; i++) {
        char ch = text[i];
        if (ch == '\r' || ch == '\n') {
            continue;
        }
        if (ch == '\t') {
            ch = ' ';
        }
        list_field_append_char(field, selected, ch, max_cols);
    }
}

static void list_field_backspace(TFormField *field, int selected) {
    if (!field || field->kind != TFORM_FIELD_LIST || selected < 0 || selected >= field->list_value.count) {
        return;
    }
    char *value = field->list_value.items[selected];
    if (!value) {
        return;
    }
    size_t length = strlen(value);
    if (length > 0) {
        value[length - 1] = '\0';
    }
}

static void list_field_prune_empty(TFormField *field, int *selected) {
    if (!field || field->kind != TFORM_FIELD_LIST) {
        return;
    }
    for (int i = 0; i < field->list_value.count;) {
        trim_in_place(field->list_value.items[i]);
        if (!field->list_value.items[i] || !*field->list_value.items[i]) {
            tform_list_delete_value(field, i);
            continue;
        }
        if (field->list_styles && i < field->list_style_capacity) {
            free(field->list_styles[i].value);
            field->list_styles[i].value = strdup(field->list_value.items[i]);
        }
        i++;
    }
    if (!selected) {
        return;
    }
    if (*selected >= field->list_value.count) {
        *selected = field->list_value.count - 1;
    }
    if (*selected < 0) {
        *selected = 0;
    }
}

static char *read_clipboard_text(void) {
    FILE *pipe = popen("xclip -selection clipboard -o 2>/dev/null", "r");
    if (!pipe) {
        return NULL;
    }

    size_t capacity = 1024;
    size_t length = 0;
    char *buffer = calloc(capacity, 1);
    if (!buffer) {
        pclose(pipe);
        return NULL;
    }

    int ch;
    while ((ch = fgetc(pipe)) != EOF) {
        if (length + 2 >= capacity) {
            capacity *= 2;
            char *next = realloc(buffer, capacity);
            if (!next) {
                free(buffer);
                pclose(pipe);
                return NULL;
            }
            buffer = next;
        }
        buffer[length++] = (char)ch;
        buffer[length] = '\0';
    }
    int status = pclose(pipe);
    if (status != 0 || length == 0) {
        free(buffer);
        return NULL;
    }
    return buffer;
}

static const char *bool_text(int value) {
    return value ? "true" : "false";
}

static void summarize_list(const TFormStringList *list, char *buffer, size_t size) {
    if (!buffer || size == 0) {
        return;
    }
    buffer[0] = '\0';
    if (!list || list->count == 0) {
        snprintf(buffer, size, "[]");
        return;
    }
    for (int i = 0; i < list->count; i++) {
        if (i > 0) {
            strncat(buffer, ", ", size - strlen(buffer) - 1);
        }
        strncat(buffer, list->items[i], size - strlen(buffer) - 1);
        if (strlen(buffer) >= size - 8 && i < list->count - 1) {
            strncat(buffer, "...", size - strlen(buffer) - 1);
            break;
        }
    }
}

static void summarize_checkbox(TFormField *field, char *buffer, size_t size) {
    int checked = 0;
    if (!buffer || size == 0) {
        return;
    }
    buffer[0] = '\0';
    if (!field || field->checkbox_count <= 0) {
        snprintf(buffer, size, "[]");
        return;
    }
    for (int i = 0; i < field->checkbox_count; i++) {
        if (field->checkbox_checked[i]) {
            checked++;
        }
    }
    snprintf(buffer, size, "%d/%d checked", checked, field->checkbox_count);
}

static void field_value(TFormField *field, char *buffer, size_t size) {
    switch (field->kind) {
        case TFORM_FIELD_TEXT:
        case TFORM_FIELD_CHOICE:
            snprintf(buffer, size, "%s", field->text_value ? field->text_value : "");
            break;
        case TFORM_FIELD_OPTIONAL_TEXT:
            snprintf(buffer, size, "%s", field->text_value && *field->text_value ? field->text_value : "None");
            break;
        case TFORM_FIELD_BOOL:
            snprintf(buffer, size, "%s", bool_text(field->bool_value));
            break;
        case TFORM_FIELD_INT:
            snprintf(buffer, size, "%d", field->int_value);
            break;
        case TFORM_FIELD_LIST:
            summarize_list(&field->list_value, buffer, size);
            break;
        case TFORM_FIELD_CHECKBOX:
            summarize_checkbox(field, buffer, size);
            break;
        case TFORM_FIELD_TEXT_SECTION:
        case TFORM_FIELD_SEPARATOR:
        case TFORM_FIELD_SECTION:
            snprintf(buffer, size, "%s", field->label.value ? field->label.value : "");
            break;
    }
}

static int field_is_selectable(TFormField *field) {
    return field && field->kind != TFORM_FIELD_SEPARATOR &&
        field->kind != TFORM_FIELD_TEXT_SECTION &&
        field->kind != TFORM_FIELD_SECTION;
}

static int field_is_inline_scrollable(TFormField *field) {
    if (!field) {
        return 0;
    }
    return field->kind == TFORM_FIELD_BOOL ||
        field->kind == TFORM_FIELD_CHOICE ||
        (field->kind == TFORM_FIELD_INT && field->scrollable);
}

static int field_is_numeric(TFormField *field) {
    return field && field->kind == TFORM_FIELD_INT;
}

static long long current_millis(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((long long)tv.tv_sec * 1000LL) + ((long long)tv.tv_usec / 1000);
}

static void reset_numeric_scroll(NumericScrollState *state) {
    if (!state) {
        return;
    }
    state->key = 0;
    state->field = INT_MAX;
    state->repeats = 0;
    state->last_ms = 0;
}

static int numeric_scroll_amount(NumericScrollState *state, int field, int key) {
    long long now = current_millis();
    if (!state) {
        return 1;
    }
    if (state->key == key && state->field == field && now - state->last_ms <= NUMERIC_ACCEL_WINDOW_MS) {
        state->repeats++;
    } else {
        state->key = key;
        state->field = field;
        state->repeats = 0;
    }
    state->last_ms = now;

    if (state->repeats >= 36) {
        return 1000;
    }
    if (state->repeats >= 18) {
        return 100;
    }
    if (state->repeats >= 6) {
        return 10;
    }
    return 1;
}

static int selected_index_for_text(TFormField *field) {
    if (!field || !field->choices || field->choice_count <= 0) {
        return 0;
    }
    const char *current = field->text_value ? field->text_value : "";
    for (int i = 0; i < field->choice_count; i++) {
        if (strcmp(field->choices[i], current) == 0) {
            return i;
        }
    }
    return 0;
}

static int cycle_text_value(TFormField *field, int dir) {
    if (!field || !field->choices || field->choice_count <= 0) {
        return 0;
    }
    int selected = selected_index_for_text(field);
    selected = tform_next_form_index(selected, field->choice_count, dir);
    replace_string(&field->text_value, strdup(field->choices[selected]), 1);
    return 1;
}

static int clamp_int(long value, int minimum, int maximum) {
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return (int)value;
}

static int field_minimum(TFormField *field) {
    return field && field->has_min ? field->min_value : INT_MIN;
}

static int field_maximum(TFormField *field) {
    return field && field->has_max ? field->max_value : INT_MAX;
}

static void show_int_bounds_error(TFormField *field, long attempted) {
    char message[160];
    if (!field) {
        return;
    }
    if (field->has_min && field->has_max) {
        snprintf(
            message,
            sizeof(message),
            "%s must be between %d and %d.",
            field->label.value ? field->label.value : "Value",
            field->min_value,
            field->max_value
        );
    } else if (field->has_min) {
        snprintf(
            message,
            sizeof(message),
            "%s must be at least %d.",
            field->label.value ? field->label.value : "Value",
            field->min_value
        );
    } else if (field->has_max) {
        snprintf(
            message,
            sizeof(message),
            "%s must be at most %d.",
            field->label.value ? field->label.value : "Value",
            field->max_value
        );
    } else {
        snprintf(message, sizeof(message), "%s is outside the supported integer range.", field->label.value ? field->label.value : "Value");
    }
    (void)attempted;
    ghostcode_message_dialog(message);
}

static int int_value_in_bounds(TFormField *field, long value) {
    return value >= field_minimum(field) && value <= field_maximum(field);
}

static int scroll_numeric_field(TFormField *field, int dir, int amount) {
    if (!field || field->kind != TFORM_FIELD_INT) {
        return 0;
    }
    if (amount < 1) {
        amount = 1;
    }
    long next = (long)field->int_value + ((long)dir * (long)amount);
    if (!int_value_in_bounds(field, next)) {
        field->int_value = clamp_int(next, field_minimum(field), field_maximum(field));
        show_int_bounds_error(field, next);
        return 0;
    }
    field->int_value = (int)next;
    return 1;
}

static int cycle_field_value(TFormField *field, int dir, int amount) {
    if (!field) {
        return 0;
    }
    switch (field->kind) {
        case TFORM_FIELD_CHOICE:
            return cycle_text_value(field, dir);
        case TFORM_FIELD_BOOL:
            field->bool_value = !field->bool_value;
            return 1;
        case TFORM_FIELD_INT:
            return scroll_numeric_field(field, dir, amount);
        default:
            return 0;
    }
}

static void draw_input_cursor(WINDOW *win, int y, int x, char ch) {
    if (!win) {
        return;
    }
    if (ch < 32 || ch > 126) {
        ch = ' ';
    }
    wattron(win, ghostcode_color_pair(GHOSTCODE_PAIR_CURSOR_ALT));
    mvwaddch(win, y, x, ch);
    wattroff(win, ghostcode_color_pair(GHOSTCODE_PAIR_CURSOR_ALT));
}

static int read_form_key(WINDOW *win) {
    int ch = wgetch(win);
    if (ch != 27) {
        return ch;
    }

    wtimeout(win, 25);
    int second = wgetch(win);
    if (second == ERR) {
        wtimeout(win, -1);
        return 27;
    }
    if (second != '[') {
        wtimeout(win, -1);
        return 27;
    }

    int third = wgetch(win);
    wtimeout(win, -1);
    switch (third) {
        case 'A':
            return KEY_UP;
        case 'B':
            return KEY_DOWN;
        case 'C':
            return KEY_RIGHT;
        case 'D':
            return KEY_LEFT;
        case 'H':
            return KEY_HOME;
        case 'F':
            return KEY_END;
        case '3': {
            wtimeout(win, 25);
            int fourth = wgetch(win);
            wtimeout(win, -1);
            return fourth == '~' ? KEY_DC : 27;
        }
        default:
            return 27;
    }
}

static void edit_field(TFormField *field) {
    if (!field) {
        return;
    }
    switch (field->kind) {
        case TFORM_FIELD_TEXT:
        case TFORM_FIELD_OPTIONAL_TEXT:
            break;
        case TFORM_FIELD_INT:
            if (!field->scrollable) {
                char initial[64];
                snprintf(initial, sizeof(initial), "%d", field->int_value);
                char *value = ghostcode_query_dialog(field->label.value, initial);
                if (value) {
                    trim_in_place(value);
                    if (*value) {
                        char *end = NULL;
                        long parsed = strtol(value, &end, 10);
                        if (end && *end == '\0') {
                            if (int_value_in_bounds(field, parsed)) {
                                field->int_value = (int)parsed;
                            } else {
                                show_int_bounds_error(field, parsed);
                            }
                        }
                    }
                    free(value);
                }
            }
            break;
        case TFORM_FIELD_LIST:
        case TFORM_FIELD_CHECKBOX:
            break;
        case TFORM_FIELD_TEXT_SECTION:
        case TFORM_FIELD_SEPARATOR:
        case TFORM_FIELD_SECTION:
            break;
        case TFORM_FIELD_CHOICE:
        case TFORM_FIELD_BOOL:
            break;
    }
}

static int first_selectable_field(TFormObject *form) {
    if (!form) {
        return -1;
    }
    for (int i = 0; i < form->field_count; i++) {
        if (field_is_selectable(&form->fields[i])) {
            return i;
        }
    }
    return -1;
}

static int next_selectable_field(TFormObject *form, int current, int dir) {
    if (!form || form->field_count <= 0 || dir == 0) {
        return current;
    }
    int index = current;
    for (int step = 0; step < form->field_count; step++) {
        index = tform_next_form_index(index, form->field_count, dir);
        if (field_is_selectable(&form->fields[index])) {
            return index;
        }
    }
    return current;
}

static int field_inner_width(int width) {
    return width > 4 ? width - 4 : 1;
}

static int form_width_from_cols(void) {
    int form_width = COLS - 4;
    if (form_width > 72) {
        form_width = 72;
    }
    if (form_width < 28) {
        form_width = COLS > 2 ? COLS - 2 : COLS;
    }
    return form_width;
}

static int text_section_line_count(const char *text) {
    int lines = 1;
    if (!text || !*text) {
        return lines;
    }
    for (size_t i = 0; text[i]; i++) {
        if (text[i] == '\n') {
            lines++;
        }
    }
    return lines;
}

static int text_section_wrap_line_count(const char *text, int width) {
    int lines = 1;
    size_t offset = 0;
    if (!text || !*text || width <= 0) {
        return lines;
    }
    lines = 0;
    while (text[offset]) {
        const char *line = text + offset;
        size_t line_bytes = strcspn(line, "\r\n");
        size_t line_offset = 0;
        if (line_bytes == 0) {
            lines++;
        }
        while (line_offset < line_bytes) {
            size_t chunk_bytes = ghostcode_ui_text_bytes_for_width(line + line_offset, width);
            if (chunk_bytes == 0 || chunk_bytes > line_bytes - line_offset) {
                chunk_bytes = line_bytes - line_offset;
            }
            line_offset += chunk_bytes;
            lines++;
        }
        offset += line_bytes;
        if (text[offset] == '\r' && text[offset + 1] == '\n') {
            offset += 2;
        } else if (text[offset]) {
            offset++;
        }
    }
    return lines > 0 ? lines : 1;
}

static void table_column_widths(TFormTable *table, int *widths) {
    if (!table || !widths) {
        return;
    }
    for (int col = 0; col < table->column_count; col++) {
        widths[col] = ghostcode_ui_text_width(table->columns[col] ? table->columns[col] : "");
    }
    for (int row = 0; row < table->row_count; row++) {
        if (table->rows[row].is_separator) {
            continue;
        }
        for (int col = 0; col < table->column_count; col++) {
            int width = ghostcode_ui_text_width(table->rows[row].cells[col] ? table->rows[row].cells[col] : "");
            if (width > widths[col]) {
                widths[col] = width;
            }
        }
    }
}

static int table_inner_width_from_columns(TFormTable *table, const int *widths) {
    if (!table || table->column_count <= 0 || !widths) {
        return 1;
    }
    int width = 0;
    if (table->show_lines) {
        width = 1;
        for (int col = 0; col < table->column_count; col++) {
            width += widths[col] + 3;
        }
    } else {
        for (int col = 0; col < table->column_count; col++) {
            width += widths[col];
            if (col + 1 < table->column_count) {
                width += 2;
            }
        }
        width += 4;
    }
    return width;
}

static int table_display_height(TFormTable *table) {
    int padding = table && table->padding >= 0 ? table->padding : TFORM_DEFAULT_PADDING;
    if (!table || table->column_count <= 0) {
        return 1 + padding * 2;
    }
    int height = table->show_lines ? 4 : 3;
    if (table->title.value && *table->title.value) {
        height++;
    }
    for (int row = 0; row < table->row_count; row++) {
        if (table->rows[row].is_separator) {
            if (table->show_lines) {
                height++;
            }
        } else {
            height++;
        }
    }
    return height + padding * 2;
}

static int section_element_height(TFormSectionElement *element, int width) {
    if (!element) {
        return 0;
    }
    if (element->kind == TFORM_SECTION_ELEMENT_TABLE) {
        return table_display_height(&element->table);
    }
    if (element->text.wrap_text) {
        return text_section_wrap_line_count(element->text.value, width);
    }
    return text_section_line_count(element->text.value);
}

static int section_display_height(TFormField *field, int width) {
    if (!field) {
        return 0;
    }
    int height = field->section_boxed ? 2 : 0;
    int inner_width = width;
    if (field->section_boxed) {
        inner_width -= 4;
    }
    if (inner_width < 1) {
        inner_width = 1;
    }
    for (int i = 0; i < field->section_element_count; i++) {
        if (i > 0) {
            height++;
        }
        height += section_element_height(&field->section_elements[i], inner_width);
    }
    if (height < 1) {
        height = 1;
    }
    return height;
}

static int field_display_height(TFormField *field, int width) {
    int padding = field && field->padding >= 0 ? field->padding : TFORM_DEFAULT_PADDING;
    int height;
    if (field && field->kind == TFORM_FIELD_SEPARATOR) {
        return 3 + padding * 2;
    }
    if (field && field->kind == TFORM_FIELD_TEXT_SECTION) {
        if (field->wrap_text) {
            return text_section_wrap_line_count(field->label.value, width) + padding * 2;
        }
        return text_section_line_count(field->label.value) + padding * 2;
    }
    if (field && field->kind == TFORM_FIELD_SECTION) {
        return section_display_height(field, width) + padding * 2;
    }
    if (field && field->kind == TFORM_FIELD_LIST) {
        return (field->list_value.count > 0 ? field->list_value.count : 1) + 3 + padding * 2;
    }
    if (field && field->kind == TFORM_FIELD_CHECKBOX) {
        return (field->checkbox_count > 0 ? field->checkbox_count : 1) + 3 + padding * 2;
    }
    if (field &&
        (field->kind == TFORM_FIELD_TEXT || field->kind == TFORM_FIELD_OPTIONAL_TEXT) &&
        field->wrap_text) {
        return text_field_wrap_line_count(field->text_value, field_inner_width(width)) + 3 + padding * 2;
    }
    (void)width;
    height = 4 + padding * 2;
    return height;
}

static int field_top_row(TFormObject *form, int index, int width) {
    int row = 1;
    if (!form) {
        return row;
    }
    for (int i = 0; i < index && i < form->field_count; i++) {
        row += field_display_height(&form->fields[i], width);
    }
    return row;
}

static int total_form_rows(TFormObject *form, int width) {
    int rows = 1;
    if (!form) {
        return rows;
    }
    for (int i = 0; i < form->field_count; i++) {
        rows += field_display_height(&form->fields[i], width);
    }
    return rows;
}

static int aligned_text_x(int base_x, int width, int text_width, TFormLabelAlign align, int edge_offset) {
    if (text_width > width) {
        text_width = width;
    }
    if (edge_offset < 0) {
        edge_offset = 0;
    }
    if (edge_offset > width - text_width) {
        edge_offset = width - text_width;
    }
    switch (align) {
        case TFORM_LABEL_ALIGN_CENTER:
            return base_x + ((width - text_width) / 2);
        case TFORM_LABEL_ALIGN_RIGHT:
            return base_x + width - text_width - edge_offset;
        case TFORM_LABEL_ALIGN_LEFT:
        default:
            return base_x + edge_offset;
    }
}

static int text_color_pair(TFormTextColor color) {
    switch (color) {
        case TFORM_TEXT_COLOR_WHITE:
            return GHOSTCODE_PAIR_NORMAL;
        case TFORM_TEXT_COLOR_CYAN:
            return GHOSTCODE_PAIR_SYNTAX_KEYWORD;
        case TFORM_TEXT_COLOR_YELLOW:
            return GHOSTCODE_PAIR_SYNTAX_TYPE;
        case TFORM_TEXT_COLOR_GREEN:
            return GHOSTCODE_PAIR_SYNTAX_STRING;
        case TFORM_TEXT_COLOR_BLUE:
            return GHOSTCODE_PAIR_SYNTAX_COMMENT;
        case TFORM_TEXT_COLOR_MAGENTA:
            return GHOSTCODE_PAIR_SYNTAX_NUMBER;
        case TFORM_TEXT_COLOR_RED:
            return GHOSTCODE_PAIR_GUTTER_MARKER;
        case TFORM_TEXT_COLOR_DEFAULT:
        default:
            return 0;
    }
}

static int label_default_attr(void) {
    return has_colors() ? COLOR_PAIR(GHOSTCODE_PAIR_SYNTAX_KEYWORD) : A_NORMAL;
}

static int text_attr(const TFormTextStyle *style, int default_attr) {
    int attr = default_attr;
    if (style && style->color != TFORM_TEXT_COLOR_DEFAULT && has_colors()) {
        attr = COLOR_PAIR(text_color_pair(style->color));
    }
    if (style && style->bold) {
        attr |= A_BOLD;
    }
    return attr;
}

static void draw_field(
    WINDOW *win,
    int y,
    int x,
    int width,
    int height,
    int selected,
    int inputting,
    int text_edit,
    int wrap_text,
    int text_cursor,
    const TFormTextStyle *label,
    TFormTextColor field_color,
    const char *value,
    int clip_top,
    int clip_bottom
) {
    TFormTextStyle border_style;
    tform_text_style_init(&border_style);
    border_style.color = field_color;
    int border_attr = text_attr(&border_style, ghostcode_color_pair(GHOSTCODE_PAIR_NORMAL));
    if (selected) {
        border_attr = has_colors() ? COLOR_PAIR(GHOSTCODE_PAIR_FORM_BORDER_SELECTED) | A_BOLD : A_BOLD;
    }
    int label_attr = text_attr(label, label_default_attr());
    if (inputting) {
        label_attr |= A_BOLD;
    }
    const char *label_value = label && label->value ? label->value : "";
    int label_width = ghostcode_ui_text_width(label_value);
    if (label_width > width) {
        label_width = width;
    }
    int label_x = aligned_text_x(x, width, label_width, label ? label->align : TFORM_LABEL_ALIGN_LEFT, 0);
    if (y >= clip_top && y < clip_bottom) {
        wattron(win, label_attr);
        wmove(win, y, label_x);
        ghostcode_ui_text_addstr_width(win, label_value, label_width);
        wattroff(win, label_attr);
    }

    wattron(win, border_attr);
    if (y + 1 >= clip_top && y + 1 < clip_bottom) {
        mvwaddch(win, y + 1, x, ACS_ULCORNER);
        mvwhline(win, y + 1, x + 1, ACS_HLINE, width - 2);
        mvwaddch(win, y + 1, x + width - 1, ACS_URCORNER);
    }
    int bottom = y + height - 1;
    for (int row = y + 2; row < bottom; row++) {
        if (row < clip_top || row >= clip_bottom) {
            continue;
        }
        mvwaddch(win, row, x, ACS_VLINE);
        mvwaddch(win, row, x + width - 1, ACS_VLINE);
    }
    if (bottom >= clip_top && bottom < clip_bottom) {
        mvwaddch(win, bottom, x, ACS_LLCORNER);
        mvwhline(win, bottom, x + 1, ACS_HLINE, width - 2);
        mvwaddch(win, bottom, x + width - 1, ACS_LRCORNER);
    }
    wattroff(win, border_attr);

    int inner_width = field_inner_width(width);
    int content_rows = height - 3;
    int row = 0;
    int col = 0;
    const char *text = value ? value : "";
    if (wrap_text) {
        for (size_t i = 0; text[i] && row < content_rows; i++) {
            char ch = text[i];
            if (ch == '\n' || ch == '\r' || ch == '\t') {
                ch = ' ';
            }
            if (ch >= 32) {
                int draw_y = y + 2 + row;
                if (draw_y >= clip_top && draw_y < clip_bottom) {
                    mvwaddch(win, draw_y, x + 2 + col, ch);
                }
                col++;
                if (col >= inner_width) {
                    row++;
                    col = 0;
                }
            }
        }
    } else {
        for (size_t i = 0; text[i] && row < content_rows; i++) {
            if (col < inner_width && text[i] >= 32) {
                int draw_y = y + 2 + row;
                if (draw_y >= clip_top && draw_y < clip_bottom) {
                    mvwaddch(win, draw_y, x + 2 + col, text[i]);
                }
                col++;
            }
        }
    }
    if (text_edit) {
        int cursor_row = 0;
        int cursor_col = 0;
        if (wrap_text) {
            text_field_cursor_position(text, text_cursor, inner_width, &cursor_row, &cursor_col);
        } else {
            int length = (int)strlen(text);
            if (text_cursor < 0) {
                text_cursor = 0;
            }
            if (text_cursor > length) {
                text_cursor = length;
            }
            cursor_col = text_cursor;
        }
        if (cursor_row < content_rows && cursor_col < inner_width) {
            int length = (int)strlen(text);
            char cursor_ch = text_cursor >= 0 && text_cursor < length ? text[text_cursor] : ' ';
            if (cursor_ch == '\n' || cursor_ch == '\r' || cursor_ch == '\t') {
                cursor_ch = ' ';
            }
            int draw_y = y + 2 + cursor_row;
            if (draw_y >= clip_top && draw_y < clip_bottom) {
                draw_input_cursor(win, draw_y, x + 2 + cursor_col, cursor_ch);
            }
        }
    }
}

static void draw_list_items(
    WINDOW *win,
    int y,
    int x,
    int width,
    int height,
    TFormField *field,
    int selected,
    int editing,
    int inputting,
    int clip_top,
    int clip_bottom
) {
    if (!win || !field || width <= 4 || height <= 3) {
        return;
    }
    const TFormStringList *list = &field->list_value;
    int inner_width = field_inner_width(width);
    int rows = height - 3;
    for (int row = 0; row < rows; row++) {
        int item_index = row;
        int draw_y = y + 2 + row;
        int draw_x = x + 2;
        if (draw_y < clip_top || draw_y >= clip_bottom) {
            continue;
        }
        int active = editing && item_index == selected && item_index >= 0 &&
            (item_index < list->count || list->count == 0);
        int reverse_row = active && !inputting;
        if (reverse_row) {
            wattron(win, A_REVERSE);
            mvwhline(win, draw_y, draw_x, ' ', inner_width);
        }
        if (item_index < list->count) {
            const char *item = list->items[item_index] ? list->items[item_index] : "";
            const TFormTextStyle *style = item_index < field->list_style_capacity ? &field->list_styles[item_index] : NULL;
            int item_attr = text_attr(style, ghostcode_color_pair(GHOSTCODE_PAIR_NORMAL));
            mvwaddch(win, draw_y, draw_x, '-');
            if (inner_width > 1) {
                mvwaddch(win, draw_y, draw_x + 1, ' ');
            }
            if (inner_width > 2) {
                int content_width = inner_width - 2;
                int item_width = ghostcode_ui_text_width(item);
                if (item_width > content_width) {
                    item_width = content_width;
                }
                int item_x = aligned_text_x(
                    draw_x + 2,
                    content_width,
                    item_width,
                    style ? style->align : TFORM_LABEL_ALIGN_LEFT,
                    0
                );
                wattron(win, item_attr);
                wmove(win, draw_y, item_x);
                ghostcode_ui_text_addstr_width(win, item, item_width);
                wattroff(win, item_attr);
            }
            if (inputting && active && inner_width > 2) {
                int cursor_col = 2 + (int)strlen(item);
                if (cursor_col >= inner_width) {
                    cursor_col = inner_width - 1;
                }
                draw_input_cursor(win, draw_y, draw_x + cursor_col, ' ');
            }
        } else if (list->count == 0 && row == 0) {
            mvwaddch(win, draw_y, draw_x, '-');
            if (inner_width > 1) {
                mvwaddch(win, draw_y, draw_x + 1, ' ');
            }
            if (inner_width > 2) {
                ghostcode_ui_text_addstr_width(win, "(empty)", inner_width - 2);
            }
        }
        if (reverse_row) {
            wattroff(win, A_REVERSE);
        }
    }
}

static const char *checkbox_tick_mark(TFormCheckboxTick tick) {
    switch (tick) {
        case TFORM_CHECKBOX_TICK_BLOCK:
            return "\xE2\x96\x88";
        case TFORM_CHECKBOX_TICK_DOT:
            return "\xE2\x80\xA2";
        case TFORM_CHECKBOX_TICK_X:
        default:
            return "x";
    }
}

static void draw_checkbox_items(
    WINDOW *win,
    int y,
    int x,
    int width,
    int height,
    TFormField *field,
    int selected,
    int editing,
    int clip_top,
    int clip_bottom
) {
    if (!win || !field || width <= 6 || height <= 3) {
        return;
    }
    int inner_width = field_inner_width(width);
    int rows = height - 3;
    for (int row = 0; row < rows; row++) {
        int item_index = row;
        int draw_y = y + 2 + row;
        int draw_x = x + 2;
        if (draw_y < clip_top || draw_y >= clip_bottom) {
            continue;
        }
        int active = editing && item_index == selected && item_index >= 0 && item_index < field->checkbox_count;
        if (active) {
            wattron(win, A_REVERSE);
            mvwhline(win, draw_y, draw_x, ' ', inner_width);
        }
        if (item_index < field->checkbox_count) {
            const char *label = field->checkbox_options[item_index].value ? field->checkbox_options[item_index].value : "";
            int option_attr = text_attr(&field->checkbox_options[item_index], ghostcode_color_pair(GHOSTCODE_PAIR_NORMAL));
            mvwaddch(win, draw_y, draw_x, '[');
            if (field->checkbox_checked[item_index]) {
                wattron(win, A_BOLD);
                wmove(win, draw_y, draw_x + 1);
                ghostcode_ui_text_addstr_width(win, checkbox_tick_mark(field->checkbox_tick), 1);
                wattroff(win, A_BOLD);
            } else {
                mvwaddch(win, draw_y, draw_x + 1, ' ');
            }
            mvwaddch(win, draw_y, draw_x + 2, ']');
            if (inner_width > 4) {
                mvwaddch(win, draw_y, draw_x + 3, ' ');
                wattron(win, option_attr);
                int content_width = inner_width - 4;
                int label_width = ghostcode_ui_text_width(label);
                if (label_width > content_width) {
                    label_width = content_width;
                }
                int label_x = aligned_text_x(
                    draw_x + 4,
                    content_width,
                    label_width,
                    field->checkbox_options[item_index].align,
                    0
                );
                wmove(win, draw_y, label_x);
                ghostcode_ui_text_addstr_width(win, label, label_width);
                wattroff(win, option_attr);
            }
        } else if (field->checkbox_count == 0 && row == 0) {
            ghostcode_ui_text_addstr_width(win, "(empty)", inner_width);
        }
        if (active) {
            wattroff(win, A_REVERSE);
        }
    }
}

static void draw_separator(
    WINDOW *win,
    int y,
    int width,
    const TFormTextStyle *label,
    TFormTextColor line_color,
    int clip_top,
    int clip_bottom
) {
    TFormTextStyle line_style;
    tform_text_style_init(&line_style);
    line_style.color = line_color;
    int line_attr = text_attr(&line_style, ghostcode_color_pair(GHOSTCODE_PAIR_FOOTER_BORDER));
    int label_attr = text_attr(label, label_default_attr());
    int line_y = y + 1;
    if (line_y < clip_top || line_y >= clip_bottom) {
        return;
    }
    wattron(win, line_attr);
    mvwhline(win, line_y, 0, ACS_HLINE, width);
    wattroff(win, line_attr);
    const char *label_value = label && label->value ? label->value : "";
    if (*label_value && width > 8) {
        int label_width = ghostcode_ui_text_width(label_value);
        int max_label_width = width - 6;
        if (label_width > max_label_width) {
            label_width = max_label_width;
        }
        int start = aligned_text_x(0, width, label_width, label ? label->align : TFORM_LABEL_ALIGN_LEFT, 4);
        if (start < width - 4) {
            wattron(win, label_attr);
            mvwaddch(win, line_y, start - 1, ' ');
            wmove(win, line_y, start);
            int written = ghostcode_ui_text_addstr_width(win, label_value, label_width);
            int end = start + written;
            if (end < width) {
                mvwaddch(win, line_y, end, ' ');
            }
            wattroff(win, label_attr);
        }
    }
}

static void draw_text_section(
    WINDOW *win,
    int y,
    int x,
    int width,
    int height,
    const TFormTextStyle *text,
    int wrap_text,
    int clip_top,
    int clip_bottom
) {
    if (!win || !text || width <= 0 || height <= 0) {
        return;
    }
    const char *value = text->value ? text->value : "";
    int attr = text_attr(text, label_default_attr());
    int row = 0;
    size_t offset = 0;
    while (row < height) {
        const char *line = value + offset;
        size_t line_bytes = strcspn(line, "\r\n");
        size_t line_offset = 0;
        do {
            size_t draw_bytes = line_bytes - line_offset;
            if (wrap_text && draw_bytes > 0) {
                draw_bytes = ghostcode_ui_text_bytes_for_width(line + line_offset, width);
                if (draw_bytes == 0 || draw_bytes > line_bytes - line_offset) {
                    draw_bytes = line_bytes - line_offset;
                }
            }
            int draw_y = y + row;
            if (draw_y >= clip_top && draw_y < clip_bottom) {
                int line_width = ghostcode_ui_text_width_n(line + line_offset, draw_bytes);
                if (line_width > width) {
                    line_width = width;
                }
                int line_x = aligned_text_x(x, width, line_width, text->align, 0);
                wattron(win, attr);
                wmove(win, draw_y, line_x);
                ghostcode_ui_text_addnstr_width(win, line + line_offset, draw_bytes, line_width);
                wattroff(win, attr);
            }
            row++;
            line_offset += draw_bytes;
            if (!wrap_text || line_bytes == 0) {
                break;
            }
        } while (line_offset < line_bytes && row < height);
        offset += line_bytes;
        if (!value[offset]) {
            break;
        }
        if (value[offset] == '\r' && value[offset + 1] == '\n') {
            offset += 2;
        } else {
            offset++;
        }
    }
}

static int table_rule_width(TFormTable *table, const int *widths) {
    return table_inner_width_from_columns(table, widths);
}

static void table_fit_widths(TFormTable *table, int *widths, int max_width) {
    if (!table || !widths || max_width <= 0) {
        return;
    }
    while (table_rule_width(table, widths) > max_width) {
        int largest = -1;
        for (int col = 0; col < table->column_count; col++) {
            if (widths[col] > 1 && (largest < 0 || widths[col] > widths[largest])) {
                largest = col;
            }
        }
        if (largest < 0) {
            break;
        }
        widths[largest]--;
    }
}

static void draw_utf8_repeat(WINDOW *win, const char *text, int count) {
    if (!win || !text || count <= 0) {
        return;
    }
    for (int i = 0; i < count; i++) {
        waddstr(win, text);
    }
}

static void draw_table_rule(
    WINDOW *win,
    int y,
    int x,
    int max_width,
    TFormTable *table,
    const int *widths,
    const char *left,
    const char *mid,
    const char *right,
    const char *fill,
    int attr,
    int clip_top,
    int clip_bottom
) {
    if (!win || !table || !widths || y < clip_top || y >= clip_bottom || max_width <= 0) {
        return;
    }
    wattron(win, attr);
    wmove(win, y, x);
    if (table->show_lines) {
        waddstr(win, left);
        for (int col = 0; col < table->column_count; col++) {
            draw_utf8_repeat(win, fill, widths[col] + 2);
            waddstr(win, col == table->column_count - 1 ? right : mid);
        }
    } else {
        waddstr(win, left);
        draw_utf8_repeat(win, fill, table_inner_width_from_columns(table, widths) - 2);
        waddstr(win, right);
    }
    wattroff(win, attr);
}

static void draw_table_title(
    WINDOW *win,
    int y,
    int x,
    int table_width,
    TFormTable *table,
    int clip_top,
    int clip_bottom
) {
    if (!win || !table || !table->title.value || !*table->title.value ||
        table_width <= 0 || y < clip_top || y >= clip_bottom) {
        return;
    }
    int title_width = ghostcode_ui_text_width(table->title.value);
    if (title_width > table_width) {
        title_width = table_width;
    }
    int title_x = aligned_text_x(x, table_width, title_width, table->title.align, 0);
    int attr = text_attr(&table->title, label_default_attr());
    wattron(win, attr);
    wmove(win, y, title_x);
    ghostcode_ui_text_addstr_width(win, table->title.value, title_width);
    wattroff(win, attr);
}

static void draw_table_top_rule(
    WINDOW *win,
    int y,
    int x,
    int max_width,
    TFormTable *table,
    const int *widths,
    int attr,
    int title_attr,
    int clip_top,
    int clip_bottom
) {
    if (!win || !table || !widths || y < clip_top || y >= clip_bottom || max_width <= 0) {
        return;
    }
    (void)title_attr;
    wattron(win, attr);
    wmove(win, y, x);
    if (table->show_lines) {
        waddstr(win, "┏");
        for (int col = 0; col < table->column_count; col++) {
            draw_utf8_repeat(win, "━", widths[col] + 2);
            waddstr(win, col == table->column_count - 1 ? "┓" : "┳");
        }
    } else {
        int width = table_inner_width_from_columns(table, widths);
        waddstr(win, "┏");
        draw_utf8_repeat(win, "━", width - 2);
        waddstr(win, "┓");
    }
    wattroff(win, attr);
}

static void draw_table_cells(
    WINDOW *win,
    int y,
    int x,
    int max_width,
    TFormTable *table,
    const int *widths,
    const char **cells,
    const char *left,
    const char *mid,
    const char *right,
    int attr,
    int cell_attr,
    int clip_top,
    int clip_bottom
) {
    if (!win || !table || !widths || !cells || y < clip_top || y >= clip_bottom || max_width <= 0) {
        return;
    }
    wattron(win, attr);
    wmove(win, y, x);
    waddstr(win, left);
    wattroff(win, attr);
    if (table->show_lines) {
        for (int col = 0; col < table->column_count; col++) {
            waddch(win, ' ');
            wattron(win, cell_attr);
            const char *text = cells[col] ? cells[col] : "";
            int visible = ghostcode_ui_text_addstr_width(win, text, widths[col]);
            wattroff(win, cell_attr);
            for (int pad = visible; pad < widths[col] + 1; pad++) {
                waddch(win, ' ');
            }
            wattron(win, attr);
            waddstr(win, col == table->column_count - 1 ? right : mid);
            wattroff(win, attr);
        }
    } else {
        int content_width = table_inner_width_from_columns(table, widths) - 4;
        int used = 0;
        waddch(win, ' ');
        for (int col = 0; col < table->column_count; col++) {
            if (col > 0) {
                waddstr(win, "  ");
                used += 2;
            }
            wattron(win, cell_attr);
            const char *text = cells[col] ? cells[col] : "";
            int visible = ghostcode_ui_text_addstr_width(win, text, widths[col]);
            wattroff(win, cell_attr);
            used += visible;
            for (int pad = visible; pad < widths[col]; pad++) {
                waddch(win, ' ');
                used++;
            }
        }
        while (used < content_width) {
            waddch(win, ' ');
            used++;
        }
        waddch(win, ' ');
        wattron(win, attr);
        waddstr(win, right);
        wattroff(win, attr);
    }
}

static void draw_table(
    WINDOW *win,
    int y,
    int x,
    int width,
    TFormTable *table,
    int clip_top,
    int clip_bottom
) {
    if (!win || !table || table->column_count <= 0 || width <= 0) {
        return;
    }
    int *widths = calloc((size_t)table->column_count, sizeof(int));
    if (!widths) {
        return;
    }
    table_column_widths(table, widths);
    table_fit_widths(table, widths, width);
    int table_width = table_inner_width_from_columns(table, widths);
    if (table_width > width) {
        table_width = width;
    }
    int draw_x = aligned_text_x(x, width, table_width, TFORM_LABEL_ALIGN_LEFT, 0);
    TFormTextStyle border_style;
    tform_text_style_init(&border_style);
    border_style.color = table->color;
    int border_attr = text_attr(&border_style, ghostcode_color_pair(GHOSTCODE_PAIR_FOOTER_BORDER));
    int header_attr = ghostcode_color_pair(GHOSTCODE_PAIR_NORMAL) | A_BOLD;
    int row_attr = ghostcode_color_pair(GHOSTCODE_PAIR_NORMAL);
    int row = y + table->padding;
    if (table->title.value && *table->title.value) {
        draw_table_title(win, row++, draw_x, table_width, table, clip_top, clip_bottom);
    }
    draw_table_top_rule(win, row++, draw_x, width, table, widths, border_attr, header_attr, clip_top, clip_bottom);
    draw_table_cells(
        win,
        row++,
        draw_x,
        width,
        table,
        widths,
        (const char **)table->columns,
        "┃",
        "┃",
        "┃",
        border_attr,
        header_attr,
        clip_top,
        clip_bottom
    );
    if (table->show_lines) {
        draw_table_rule(win, row++, draw_x, width, table, widths, "┡", "╇", "┩", "━", border_attr, clip_top, clip_bottom);
    }
    for (int i = 0; i < table->row_count; i++) {
        if (table->rows[i].is_separator) {
            if (table->show_lines) {
                draw_table_rule(win, row++, draw_x, width, table, widths, "├", "┼", "┤", "─", border_attr, clip_top, clip_bottom);
            }
            continue;
        }
        draw_table_cells(
            win,
            row++,
            draw_x,
            width,
            table,
            widths,
            (const char **)table->rows[i].cells,
            "│",
            "│",
            "│",
            border_attr,
            row_attr,
            clip_top,
            clip_bottom
        );
    }
    draw_table_rule(win, row, draw_x, width, table, widths, "└", "┴", "┘", "─", border_attr, clip_top, clip_bottom);
    free(widths);
}

static void draw_section_box(WINDOW *win, int y, int x, int width, int height, int clip_top, int clip_bottom) {
    if (!win || width < 2 || height < 2) {
        return;
    }
    int attr = ghostcode_color_pair(GHOSTCODE_PAIR_FOOTER_BORDER);
    wattron(win, attr);
    if (y >= clip_top && y < clip_bottom) {
        mvwaddch(win, y, x, ACS_ULCORNER);
        mvwhline(win, y, x + 1, ACS_HLINE, width - 2);
        mvwaddch(win, y, x + width - 1, ACS_URCORNER);
    }
    for (int row = y + 1; row < y + height - 1; row++) {
        if (row >= clip_top && row < clip_bottom) {
            mvwaddch(win, row, x, ACS_VLINE);
            mvwaddch(win, row, x + width - 1, ACS_VLINE);
        }
    }
    int bottom = y + height - 1;
    if (bottom >= clip_top && bottom < clip_bottom) {
        mvwaddch(win, bottom, x, ACS_LLCORNER);
        mvwhline(win, bottom, x + 1, ACS_HLINE, width - 2);
        mvwaddch(win, bottom, x + width - 1, ACS_LRCORNER);
    }
    wattroff(win, attr);
}

static void draw_section(
    WINDOW *win,
    int y,
    int x,
    int width,
    int height,
    TFormField *field,
    int clip_top,
    int clip_bottom
) {
    if (!win || !field || width <= 0 || height <= 0) {
        return;
    }
    int inner_x = x;
    int inner_y = y;
    int inner_width = width;
    int inner_height = height;
    if (field->section_boxed) {
        draw_section_box(win, y, x, width, height, clip_top, clip_bottom);
        inner_x += 2;
        inner_y += 1;
        inner_width -= 4;
        inner_height -= 2;
    }
    if (inner_width < 1 || inner_height < 1) {
        return;
    }
    int row = inner_y;
    for (int i = 0; i < field->section_element_count; i++) {
        if (i > 0) {
            row++;
        }
        TFormSectionElement *element = &field->section_elements[i];
        int element_height = section_element_height(element, inner_width);
        if (element->kind == TFORM_SECTION_ELEMENT_TABLE) {
            draw_table(win, row, inner_x, inner_width, &element->table, clip_top, clip_bottom);
        } else {
            draw_text_section(
                win,
                row,
                inner_x,
                inner_width,
                element_height,
                &element->text,
                element->text.wrap_text,
                clip_top,
                clip_bottom
            );
        }
        row += element_height;
    }
}

static void draw_selection_pointer(WINDOW *win, int field_y, int field_height, int form_x, int body_h) {
    if (!win || body_h <= 1) {
        return;
    }
    int pointer_y = field_y + 2;
    int max_y = field_y + field_height - 1;
    if (pointer_y > max_y) {
        pointer_y = max_y;
    }
    if (pointer_y < 1) {
        pointer_y = 1;
    }
    if (pointer_y >= body_h) {
        pointer_y = body_h - 1;
    }
    int pointer_x = form_x >= 2 ? form_x - 2 : 0;
    int attr = has_colors() ? COLOR_PAIR(GHOSTCODE_PAIR_GUTTER_MARKER) | A_BOLD : A_BOLD;
    wattron(win, attr);
    mvwaddch(win, pointer_y, pointer_x, '>');
    wattroff(win, attr);
}

static void draw_form(
    GhostcodePmNode *main_panel,
    GhostcodePmNode *footer_panel,
    TFormObject *form,
    int selected,
    TFormEditMode edit_mode,
    int text_cursor,
    int list_selected,
    int list_inputting,
    int checkbox_selected,
    int *top
) {
    int body_h = LINES > 4 ? LINES - 3 : LINES - 1;
    int footer_h = LINES - body_h;
    if (footer_h < 1) {
        footer_h = 1;
    }
    ghostcode_pm_resize(main_panel, body_h, COLS);
    ghostcode_pm_resize(footer_panel, footer_h, COLS);
    ghostcode_pm_move(footer_panel, body_h, 0);
    werase(main_panel->win);
    wbkgd(main_panel->win, ghostcode_color_pair(GHOSTCODE_PAIR_NORMAL));
    wattron(main_panel->win, ghostcode_color_pair(GHOSTCODE_PAIR_HEADER));
    mvwhline(main_panel->win, 0, 0, ' ', COLS);
    mvwprintw(main_panel->win, 0, 1, "%.*s", COLS - 2, form->title ? form->title : "TForm");
    wattroff(main_panel->win, ghostcode_color_pair(GHOSTCODE_PAIR_HEADER));

    int form_width = form_width_from_cols();
    int form_x = (COLS - form_width) / 2;
    if (form_x < 0) {
        form_x = 0;
    }

    int viewport_rows = body_h - 1;
    if (viewport_rows < 1) {
        viewport_rows = 1;
    }
    int selected_top = field_top_row(form, selected, form_width) - 1;
    int selected_bottom = selected_top + field_display_height(&form->fields[selected], form_width);
    if (selected == first_selectable_field(form)) {
        *top = 0;
    } else {
        if (selected_top < *top) {
            *top = selected_top;
        }
        if (selected_bottom > *top + viewport_rows) {
            *top = selected_bottom - viewport_rows;
        }
    }
    int max_top = total_form_rows(form, form_width) - 1 - viewport_rows;
    if (max_top < 0) {
        max_top = 0;
    }
    if (*top > max_top) {
        *top = max_top;
    }
    if (*top < 0) {
        *top = 0;
    }

    for (int index = 0; index < form->field_count; index++) {
        int y = field_top_row(form, index, form_width) - *top;
        int height = field_display_height(&form->fields[index], form_width);
        int padding = form->fields[index].padding >= 0 ? form->fields[index].padding : TFORM_DEFAULT_PADDING;
        int content_y = y + padding;
        int content_height = height - padding * 2;
        if (y + height <= 1 || y >= body_h ||
            content_y + content_height <= 1 || content_y >= body_h ||
            content_height <= 0) {
            continue;
        }
        if (form->fields[index].kind == TFORM_FIELD_SEPARATOR) {
            draw_separator(
                main_panel->win,
                content_y,
                COLS,
                &form->fields[index].label,
                form->fields[index].separator_color,
                1,
                body_h
            );
            continue;
        }
        if (form->fields[index].kind == TFORM_FIELD_TEXT_SECTION) {
            draw_text_section(
                main_panel->win,
                content_y,
                form_x,
                form_width,
                content_height,
                &form->fields[index].label,
                form->fields[index].wrap_text,
                1,
                body_h
            );
            continue;
        }
        if (form->fields[index].kind == TFORM_FIELD_SECTION) {
            draw_section(
                main_panel->win,
                content_y,
                form_x,
                form_width,
                content_height,
                &form->fields[index],
                1,
                body_h
            );
            continue;
        }
        char value[1024];
        if (form->fields[index].kind == TFORM_FIELD_LIST) {
            value[0] = '\0';
        } else if (form->fields[index].kind == TFORM_FIELD_CHECKBOX) {
            value[0] = '\0';
        } else {
            field_value(&form->fields[index], value, sizeof(value));
        }
        draw_field(
            main_panel->win,
            content_y,
            form_x,
            form_width,
            content_height,
            index == selected,
            edit_mode != TFORM_EDIT_NONE && index == selected,
            edit_mode == TFORM_EDIT_TEXT && index == selected,
            form->fields[index].wrap_text,
            index == selected ? text_cursor : 0,
            &form->fields[index].label,
            form->fields[index].field_color,
            value,
            1,
            body_h
        );
        if (form->fields[index].kind == TFORM_FIELD_LIST) {
            draw_list_items(
                main_panel->win,
                content_y,
                form_x,
                form_width,
                content_height,
                &form->fields[index],
                list_selected,
                edit_mode == TFORM_EDIT_LIST && index == selected,
                edit_mode == TFORM_EDIT_LIST && list_inputting && index == selected,
                1,
                body_h
            );
        }
        if (form->fields[index].kind == TFORM_FIELD_CHECKBOX) {
            draw_checkbox_items(
                main_panel->win,
                content_y,
                form_x,
                form_width,
                content_height,
                &form->fields[index],
                checkbox_selected,
                edit_mode == TFORM_EDIT_CHECKBOX && index == selected,
                1,
                body_h
            );
        }
        if (index == selected && field_is_selectable(&form->fields[index])) {
            draw_selection_pointer(main_panel->win, content_y, content_height, form_x, body_h);
        }
    }
    if (edit_mode == TFORM_EDIT_TEXT) {
        TFormField *field = &form->fields[selected];
        if (field->wrap_text) {
            ghostcode_ui_draw_footer(footer_panel->win, "[Arrows] Move  [Enter] Done  [Ctrl+E] Done  [Ctrl+V] Paste  [Esc] Clear");
        } else {
            ghostcode_ui_draw_footer(footer_panel->win, "[Left/Right] Move  [Enter] Done  [Ctrl+V] Paste  [Esc] Clear");
        }
    } else if (edit_mode == TFORM_EDIT_SCROLL) {
        ghostcode_ui_draw_footer(footer_panel->win, "[Up/Down] Value  [Enter] Done  [Esc] Done  [Ctrl+S] Save");
    } else if (edit_mode == TFORM_EDIT_LIST) {
        TFormField *field = &form->fields[selected];
        if (list_inputting) {
            ghostcode_ui_draw_footer(footer_panel->win, "Entry  [Ctrl+E] Done  [Ctrl+V] Paste  [Esc] Exit");
        } else if (field->has_file_picker) {
            ghostcode_ui_draw_footer(footer_panel->win, "[Up/Down] Item  [A] Pick  [E] Replace  [D] Delete  [Esc] Exit");
        } else {
            ghostcode_ui_draw_footer(footer_panel->win, "[Up/Down] Item  [A] Add  [E] Edit  [D] Delete  [Esc] Exit");
        }
    } else if (edit_mode == TFORM_EDIT_CHECKBOX) {
        ghostcode_ui_draw_footer(footer_panel->win, "[Up/Down] Option  [Space/Enter] Toggle  [Ctrl+E] Done  [Esc] Done");
    } else {
        ghostcode_ui_draw_footer(footer_panel->win, "[Up/Down] Field  [Enter] Edit  [I] Info  [Ctrl+S] Save  [Q] Exit");
    }
    ghostcode_pm_wnoutrefresh(main_panel);
    ghostcode_pm_wnoutrefresh(footer_panel);
    ghostcode_pm_update();
}

int tform_run_form(TFormObject *form) {
    setlocale(LC_ALL, "");
    ghostcode_disable_flow_control();
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
    if (footer_h < 1) {
        footer_h = 1;
    }
    GhostcodePmNode *main_panel = ghostcode_pm_add(0, 0, body_h, COLS, GHOSTCODE_PM_LAYER_MAIN, GHOSTCODE_PM_LAYER_MAIN);
    GhostcodePmNode *footer_panel = ghostcode_pm_add(body_h, 0, footer_h, COLS, GHOSTCODE_PM_LAYER_FOOTER, GHOSTCODE_PM_LAYER_FOOTER);
    if (!main_panel || !footer_panel) {
        ghostcode_pm_teardown();
        endwin();
        return -1;
    }
    keypad(main_panel->win, TRUE);

    int selected = first_selectable_field(form);
    if (selected < 0) {
        selected = 0;
    }
    int top = 0;
    int text_cursor = 0;
    int list_selected = 0;
    int list_inputting = 0;
    int checkbox_selected = 0;
    TFormEditMode edit_mode = TFORM_EDIT_NONE;
    NumericScrollState numeric_scroll;
    reset_numeric_scroll(&numeric_scroll);
    int result = 0;
    for (;;) {
        draw_form(main_panel, footer_panel, form, selected, edit_mode, text_cursor, list_selected, list_inputting, checkbox_selected, &top);
        int ch = read_form_key(main_panel->win);
        if (edit_mode == TFORM_EDIT_TEXT) {
            TFormField *field = &form->fields[selected];
            int max_cols = field_inner_width(form_width_from_cols());
            int length = field->text_value ? (int)strlen(field->text_value) : 0;
            if (text_cursor < 0) {
                text_cursor = 0;
            }
            if (text_cursor > length) {
                text_cursor = length;
            }
            if (ch == 27) {
                text_field_clear(field);
                text_cursor = 0;
                edit_mode = TFORM_EDIT_NONE;
                reset_numeric_scroll(&numeric_scroll);
            } else if (ch == CTRL_E || ch == '\n' || ch == KEY_ENTER) {
                edit_mode = TFORM_EDIT_NONE;
                reset_numeric_scroll(&numeric_scroll);
            } else if (ch == KEY_LEFT) {
                text_field_move_cursor(field, &text_cursor, -1, max_cols);
            } else if (ch == KEY_RIGHT) {
                text_field_move_cursor(field, &text_cursor, 1, max_cols);
            } else if (ch == KEY_UP && field->wrap_text) {
                text_field_move_cursor_vertical(field, &text_cursor, -1, max_cols);
            } else if (ch == KEY_DOWN && field->wrap_text) {
                text_field_move_cursor_vertical(field, &text_cursor, 1, max_cols);
            } else if (ch == KEY_HOME) {
                text_cursor = 0;
            } else if (ch == KEY_END) {
                text_cursor = field->text_value ? (int)strlen(field->text_value) : 0;
            } else if (ch == KEY_DC) {
                text_field_delete_at_cursor(field, text_cursor);
            } else if (ch == CTRL_V) {
                char *clipboard = read_clipboard_text();
                if (clipboard) {
                    text_field_insert_text(field, &text_cursor, clipboard, max_cols);
                    free(clipboard);
                }
            } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
                text_field_backspace(field, &text_cursor);
            } else if (ch >= 32 && ch <= 126) {
                text_field_insert_char(field, &text_cursor, (char)ch, max_cols);
            } else if (ch == CTRL_S) {
                result = 1;
                break;
            }
            continue;
        }
        if (edit_mode == TFORM_EDIT_SCROLL) {
            if (ch == '\n' || ch == KEY_ENTER || ch == 27) {
                edit_mode = TFORM_EDIT_NONE;
                reset_numeric_scroll(&numeric_scroll);
            } else if (ch == KEY_UP || ch == KEY_DOWN) {
                TFormField *field = &form->fields[selected];
                int amount = field_is_numeric(field) ? numeric_scroll_amount(&numeric_scroll, selected, ch) : 1;
                cycle_field_value(field, ch == KEY_UP ? 1 : -1, amount);
            } else if (ch == CTRL_S) {
                result = 1;
                break;
            }
            continue;
        }
        if (edit_mode == TFORM_EDIT_LIST) {
            TFormField *field = &form->fields[selected];
            int max_cols = field_inner_width(form_width_from_cols());
            if (field->list_value.count <= 0) {
                list_selected = 0;
            } else if (list_selected < 0 || list_selected >= field->list_value.count) {
                list_selected = 0;
            }
            if (list_inputting) {
                if (ch == CTRL_E) {
                    list_field_prune_empty(field, &list_selected);
                    list_inputting = 0;
                } else if (ch == 27) {
                    list_field_prune_empty(field, &list_selected);
                    list_inputting = 0;
                    edit_mode = TFORM_EDIT_NONE;
                    reset_numeric_scroll(&numeric_scroll);
                } else if (ch == CTRL_V) {
                    char *clipboard = read_clipboard_text();
                    if (clipboard) {
                        list_field_append_text(field, &list_selected, clipboard, max_cols);
                        free(clipboard);
                    }
                } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
                    list_field_backspace(field, list_selected);
                } else if (ch >= 32 && ch <= 126) {
                    list_field_append_char(field, &list_selected, (char)ch, max_cols);
                } else if (ch == CTRL_S) {
                    list_field_prune_empty(field, &list_selected);
                    result = 1;
                    break;
                }
                continue;
            }
            if (ch == 27 || ch == CTRL_E) {
                list_field_prune_empty(field, &list_selected);
                edit_mode = TFORM_EDIT_NONE;
                list_inputting = 0;
                reset_numeric_scroll(&numeric_scroll);
            } else if (ch == KEY_UP && field->list_value.count > 0) {
                list_selected = tform_next_form_index(list_selected, field->list_value.count, -1);
            } else if (ch == KEY_DOWN && field->list_value.count > 0) {
                list_selected = tform_next_form_index(list_selected, field->list_value.count, 1);
            } else if (ch == 'a' || ch == 'A') {
                if (field->has_file_picker) {
                    char *value = ghostcode_file_picker_dialog(
                        field->label.value,
                        field->file_picker_mode,
                        field->file_picker_start_dir
                    );
                    if (value) {
                        trim_in_place(value);
                        if (*value && tform_list_append_value(field, value, NULL) == 0) {
                            list_selected = field->list_value.count - 1;
                        }
                        free(value);
                    }
                } else if (tform_list_append_value(field, " ", NULL) == 0) {
                    list_selected = field->list_value.count - 1;
                    field->list_value.items[list_selected][0] = '\0';
                    if (field->list_styles && field->list_styles[list_selected].value) {
                        field->list_styles[list_selected].value[0] = '\0';
                    }
                    list_inputting = 1;
                }
            } else if ((ch == 'e' || ch == 'E') && field->list_value.count > 0) {
                if (field->has_file_picker) {
                    char *value = ghostcode_file_picker_dialog(
                        field->label.value,
                        field->file_picker_mode,
                        field->file_picker_start_dir
                    );
                    if (value) {
                        trim_in_place(value);
                        if (*value) {
                            tform_list_set_value(field, list_selected, value, NULL);
                        }
                        free(value);
                    }
                } else {
                    list_inputting = 1;
                }
            } else if ((ch == 'd' || ch == 'D') && field->list_value.count > 0) {
                tform_list_delete_value(field, list_selected);
                if (list_selected >= field->list_value.count) {
                    list_selected = field->list_value.count - 1;
                }
                if (list_selected < 0) {
                    list_selected = 0;
                }
            } else if (ch == CTRL_S) {
                list_field_prune_empty(field, &list_selected);
                result = 1;
                break;
            }
            continue;
        }
        if (edit_mode == TFORM_EDIT_CHECKBOX) {
            TFormField *field = &form->fields[selected];
            if (field->checkbox_count <= 0) {
                checkbox_selected = 0;
            } else if (checkbox_selected < 0 || checkbox_selected >= field->checkbox_count) {
                checkbox_selected = 0;
            }
            if (ch == 27 || ch == CTRL_E) {
                edit_mode = TFORM_EDIT_NONE;
                reset_numeric_scroll(&numeric_scroll);
            } else if (ch == KEY_UP && field->checkbox_count > 0) {
                checkbox_selected = tform_next_form_index(checkbox_selected, field->checkbox_count, -1);
            } else if (ch == KEY_DOWN && field->checkbox_count > 0) {
                checkbox_selected = tform_next_form_index(checkbox_selected, field->checkbox_count, 1);
            } else if ((ch == ' ' || ch == '\n' || ch == KEY_ENTER) && field->checkbox_count > 0) {
                field->checkbox_checked[checkbox_selected] = !field->checkbox_checked[checkbox_selected];
            } else if (ch == CTRL_S) {
                result = 1;
                break;
            }
            continue;
        }
        reset_numeric_scroll(&numeric_scroll);
        if (ch == 'q' || ch == 'Q') {
            break;
        }
        if (ch == KEY_UP) {
            selected = next_selectable_field(form, selected, -1);
            if (selected == first_selectable_field(form)) {
                top = 0;
            }
        } else if (ch == KEY_DOWN) {
            selected = next_selectable_field(form, selected, 1);
            if (selected == first_selectable_field(form)) {
                top = 0;
            }
        } else if ((ch == '\n' || ch == KEY_ENTER) && field_is_selectable(&form->fields[selected])) {
            if (field_is_inline_scrollable(&form->fields[selected])) {
                edit_mode = TFORM_EDIT_SCROLL;
                reset_numeric_scroll(&numeric_scroll);
            } else if (form->fields[selected].kind == TFORM_FIELD_TEXT ||
                       form->fields[selected].kind == TFORM_FIELD_OPTIONAL_TEXT) {
                TFormField *field = &form->fields[selected];
                if (field->has_file_picker) {
                    char *value = ghostcode_file_picker_dialog(
                        field->label.value,
                        field->file_picker_mode,
                        field->file_picker_start_dir
                    );
                    if (value) {
                        replace_string(&field->text_value, value, field->kind == TFORM_FIELD_OPTIONAL_TEXT);
                    }
                } else {
                    edit_mode = TFORM_EDIT_TEXT;
                    text_cursor = form->fields[selected].text_value ? (int)strlen(form->fields[selected].text_value) : 0;
                }
            } else if (form->fields[selected].kind == TFORM_FIELD_LIST) {
                list_selected = form->fields[selected].list_value.count > 0 ? 0 : 0;
                list_inputting = 0;
                edit_mode = TFORM_EDIT_LIST;
            } else if (form->fields[selected].kind == TFORM_FIELD_CHECKBOX) {
                checkbox_selected = form->fields[selected].checkbox_count > 0 ? 0 : 0;
                edit_mode = TFORM_EDIT_CHECKBOX;
            } else {
                edit_field(&form->fields[selected]);
            }
        } else if ((ch == 'i' || ch == 'I') && field_is_selectable(&form->fields[selected])) {
            ghostcode_readonly_dialog(form->fields[selected].label.value, form->fields[selected].description);
        } else if (ch == CTRL_S) {
            result = 1;
            break;
        }
    }

    ghostcode_pm_teardown();
    endwin();
    return result;
}
