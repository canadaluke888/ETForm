#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "../src/form_runtime.c"

static wchar_t screen_char(WINDOW *win, int y, int x) {
    cchar_t cell;
    wchar_t text[CCHARW_MAX];
    attr_t attrs;
    short pair;
    assert(mvwin_wch(win, y, x, &cell) == OK);
    assert(getcchar(&cell, text, &attrs, &pair, NULL) == OK);
    return text[0];
}

static void check_layout(WINDOW *win, int lines) {
    char *columns[] = {"ColA", "ColB"};
    char *first[] = {"alpha", "bravo"};
    char *second[] = {"charlie", "delta"};
    char *third[] = {"e", "f"};
    ETFormTableRow rows[] = {
        {.cells = first}, {.cells = second}, {.cells = third}
    };
    unsigned char colors[] = {196, 46};
    ETFormField field = {0};
    field.interactive_table.columns = columns;
    field.interactive_table.column_count = 2;
    field.interactive_table.rows = rows;
    field.interactive_table.row_count = 3;
    field.interactive_table.show_lines = lines;
    field.table_column_colors = colors;

    int widths[] = {7, 5};
    int x = 4;
    int top = 4;
    int right = x + table_inner_width_from_columns(&field.interactive_table, widths) - 1;
    int second_col = x + 2 + widths[0] + (lines ? 3 : 2);
    int bottom = top + 3 + 3 + (lines ? 2 : 0);

    for (int selected_row = 0; selected_row < 3; selected_row++) {
        for (int selected_col = 0; selected_col < 2; selected_col++) {
            werase(win);
            assert(draw_interactive_table_field(
                win, 1, 2, 48, 20, &field, 1, 1,
                selected_row, selected_col, -1, 0, 25
            ) == 0);
            int row = top + 3 + selected_row * (lines ? 2 : 1);
            int col = selected_col ? second_col : x + 2;
            chtype selected = mvwinch(win, row, col);
            assert(selected & A_REVERSE);
            assert(PAIR_NUMBER(selected) == etform_image_color_pair(colors[selected_col]));
            assert(screen_char(win, row, x) == L'│');
            assert(screen_char(win, row, right) == L'│');
            if (lines) {
                assert(screen_char(win, row, x + widths[0] + 3) == L'│');
                for (int separator = 0; separator < 2; separator++) {
                    int rule_y = top + 4 + separator * 2;
                    assert(screen_char(win, rule_y, x) == L'├');
                    assert(screen_char(win, rule_y, x + widths[0] + 3) == L'┼');
                    assert(screen_char(win, rule_y, right) == L'┤');
                }
            }
            assert(screen_char(win, top, x) == L'┏');
            assert(screen_char(win, bottom, right) == L'┘');
            assert(PAIR_NUMBER(mvwinch(win, top + 1, x + 2)) ==
                etform_image_color_pair(colors[0]));
            assert(PAIR_NUMBER(mvwinch(win, top + 1, second_col)) ==
                etform_image_color_pair(colors[1]));
        }
    }
}

static void check_paging(WINDOW *win) {
    char *columns[] = {"first", "second", "third", "fourth"};
    char *cells[] = {"one", "two", "three", "four"};
    ETFormTableRow rows[] = {{.cells = cells}, {.cells = cells}};
    unsigned char colors[] = {196, 46, 51, 190};
    ETFormField field = {0};
    field.interactive_table.columns = columns;
    field.interactive_table.column_count = 4;
    field.interactive_table.rows = rows;
    field.interactive_table.row_count = 2;
    field.interactive_table.show_lines = 1;
    field.table_column_colors = colors;

    werase(win);
    assert(draw_interactive_table_field(win, 1, 2, 22, 12, &field, 1, 1, 0, 3, -1, 0, 25) == 0);
    assert(screen_char(win, 5, 6) == L't');
    assert(screen_char(win, 5, 14) == L'f');
    assert(screen_char(win, 7, 14) == L'f');
    assert(mvwinch(win, 7, 14) & A_REVERSE);
    assert(PAIR_NUMBER(mvwinch(win, 7, 14)) == etform_image_color_pair(colors[3]));
    assert(screen_char(win, 7, 4) == L'│');
    assert(screen_char(win, 7, 21) == L'│');
    werase(win);
    assert(draw_interactive_table_field(win, 1, 2, 22, 12, &field, 1, 1, 1, 3, 0, 0, 25) == 0);
    for (int y = 7; y <= 9; y += 2) {
        assert(mvwinch(win, y, 6) & A_REVERSE);
        assert(PAIR_NUMBER(mvwinch(win, y, 6)) == etform_image_color_pair(colors[2]));
        assert(mvwinch(win, y, 14) & A_REVERSE);
        assert(PAIR_NUMBER(mvwinch(win, y, 14)) == etform_image_color_pair(colors[3]));
        assert(screen_char(win, y, 4) == L'│');
        assert(screen_char(win, y, 21) == L'│');
    }
}

static void check_swap_highlight(WINDOW *win, int lines) {
    char *columns[] = {"A", "B"};
    char *first[] = {"ab", "cd"};
    char *second[] = {"ef", "gh"};
    char *third[] = {"ij", ""};
    ETFormTableRow rows[] = {{.cells = first}, {.cells = second}, {.cells = third}};
    unsigned char colors[] = {196, 46};
    ETFormField field = {0};
    field.interactive_table.columns = columns;
    field.interactive_table.column_count = 2;
    field.interactive_table.rows = rows;
    field.interactive_table.row_count = 3;
    field.interactive_table.show_lines = lines;
    field.table_column_colors = colors;

    werase(win);
    assert(draw_interactive_table_field(win, 1, 2, 48, 20, &field, 1, 1, 2, 0, 0, 0, 25) == 0);
    int step = lines ? 2 : 1;
    int second_cell = lines ? 10 : 9;
    for (int row = 0; row < 3; row++) {
        int y = 7 + row * step;
        for (int col = 0; col < 2; col++) {
            int left = col ? second_cell : 5;
            for (int x = left; x < left + 4; x++) {
                chtype cell = mvwinch(win, y, x);
                assert(!!(cell & A_REVERSE) == (row != 1));
                if (row != 1) {
                    assert(PAIR_NUMBER(cell) == etform_image_color_pair(colors[col]));
                }
            }
        }
        assert(screen_char(win, y, 4) == L'│');
        assert(screen_char(win, y, lines ? 14 : 13) == L'│');
        if (lines) assert(screen_char(win, y, 9) == L'│');
    }

    werase(win);
    assert(draw_interactive_table_field(win, 1, 2, 48, 20, &field, 1, 1, 0, 0, 0, 0, 25) == 0);
    assert(mvwinch(win, 7, 5) & A_REVERSE);
    assert(!(mvwinch(win, 7 + step, 5) & A_REVERSE));
}

static void check_empty_and_resized_cells(WINDOW *win) {
    char *columns[] = {"A", "B"};
    char *cells[] = {"long value", ""};
    ETFormTableRow rows[] = {{.cells = cells}};
    unsigned char colors[] = {196, 46};
    ETFormField field = {0};
    field.interactive_table.columns = columns;
    field.interactive_table.column_count = 2;
    field.interactive_table.rows = rows;
    field.interactive_table.row_count = 1;
    field.interactive_table.show_lines = 1;
    field.table_column_colors = colors;

    werase(win);
    assert(draw_interactive_table_field(win, 1, 2, 48, 12, &field, 1, 1, 0, 1, -1, 0, 25) == 0);
    assert(screen_char(win, 7, 17) == L'│');
    assert(PAIR_NUMBER(mvwinch(win, 7, 19)) == ETFORM_PAIR_CURSOR_ALT);
    assert(screen_char(win, 7, 21) == L'│');

    cells[0] = "x";
    werase(win);
    assert(draw_interactive_table_field(win, 1, 2, 48, 12, &field, 1, 1, 0, 1, -1, 0, 25) == 0);
    assert(screen_char(win, 7, 8) == L'│');
    assert(PAIR_NUMBER(mvwinch(win, 7, 10)) == ETFORM_PAIR_CURSOR_ALT);
    assert(screen_char(win, 7, 12) == L'│');

    field.interactive_table.column_count = 1;
    cells[0] = "a very long value";
    werase(win);
    assert(draw_interactive_table_field(win, 1, 2, 12, 12, &field, 1, 1, 0, 0, -1, 0, 25) == 0);
    assert(screen_char(win, 7, 11) == L'│');
    assert(PAIR_NUMBER(mvwinch(win, 7, 10)) == ETFORM_PAIR_CURSOR_ALT);
}

static void check_vertical_scroll(void) {
    char *columns[] = {"A"};
    char *cells[] = {"x"};
    ETFormTableRow rows[20] = {0};
    unsigned char colors[] = {196};
    for (int i = 0; i < 20; i++) rows[i].cells = cells;
    ETFormField field = {0};
    field.kind = ETFORM_FIELD_ITABLE;
    field.interactive_table.columns = columns;
    field.interactive_table.column_count = 1;
    field.interactive_table.rows = rows;
    field.interactive_table.row_count = 20;
    field.interactive_table.show_lines = 1;
    field.table_column_colors = colors;
    ETFormObject form = {0};
    form.title = "Scrolling";
    form.fields = &field;
    form.field_count = 1;
    etform_pm_init();
    ETFormPmNode *main = etform_pm_add(0, 0, 27, 80, ETFORM_PM_LAYER_MAIN, ETFORM_PM_LAYER_MAIN);
    ETFormPmNode *footer = etform_pm_add(27, 0, 3, 80, ETFORM_PM_LAYER_FOOTER, ETFORM_PM_LAYER_FOOTER);
    assert(main && footer);
    int top = 0;
    assert(draw_form(main, footer, &form, 0, ETFORM_EDIT_ITABLE, 0, 0, 0, 0, 0, 0, -1, 1, &top) == 0);
    assert(top == 0);
    assert(draw_form(main, footer, &form, 0, ETFORM_EDIT_ITABLE, 0, 0, 0, 0, 19, 0, -1, 1, &top) == 0);
    assert(top == 19);
    assert(mvwinch(main->win, 26, 8) & A_REVERSE);
    assert(draw_form(main, footer, &form, 0, ETFORM_EDIT_ITABLE, 0, 0, 0, 0, 19, 0, 0, 1, &top) == 0);
    assert(top == 19);
    assert(mvwinch(main->win, 26, 8) & A_REVERSE);
    assert(PAIR_NUMBER(mvwinch(main->win, 26, 8)) == etform_image_color_pair(colors[0]));
    assert(draw_form(main, footer, &form, 0, ETFORM_EDIT_ITABLE, 0, 0, 0, 0, 0, 0, -1, 1, &top) == 0);
    assert(top == 6);
    assert(mvwinch(main->win, 1, 8) & A_REVERSE);
    etform_pm_teardown();
}

static void check_random_colors(void) {
    assert(PyType_Ready(&ETFormType) == 0);
    assert(PyType_Ready(&ETFormITableType) == 0);
    PyObject *input = PyObject_CallFunction((PyObject *)&ETFormITableType, "s", "table");
    PyObject *form = PyObject_CallFunction((PyObject *)&ETFormType, "s", "form");
    assert(input && form);
    for (int i = 0; i < 64; i++) {
        PyObject *result = PyObject_CallMethod(input, "add_column", "s", "column");
        assert(result);
        Py_DECREF(result);
    }
    PyObject *result = PyObject_CallMethod(form, "add_table_input", "O", input);
    assert(result);
    Py_DECREF(result);
    ETFormField *field = &((ETFormObject *)form)->fields[0];
    for (int i = 1; i < 64; i++) {
        int a = field->table_column_colors[i - 1] - 16;
        int b = field->table_column_colors[i] - 16;
        int dr = a / 36 - b / 36;
        int dg = a / 6 % 6 - b / 6 % 6;
        int db = a % 6 - b % 6;
        assert(dr * dr + dg * dg + db * db >= 18);
    }
    Py_DECREF(input);
    Py_DECREF(form);
}

int main(void) {
    setlocale(LC_ALL, "");
    Py_Initialize();
    check_random_colors();
    FILE *input = tmpfile();
    FILE *output = tmpfile();
    assert(input && output);
    SCREEN *screen = newterm("xterm-256color", output, input);
    assert(screen);
    assert(resizeterm(30, 80) == OK);
    etform_init_editor_theme();
    assert(COLORS >= 256 && COLOR_PAIRS >= ETFORM_IMAGE_PAIR_BASE + ETFORM_IMAGE_COLOR_COUNT);
    WINDOW *win = newwin(25, 60, 0, 0);
    assert(win);
    check_layout(win, 0);
    check_layout(win, 1);
    check_paging(win);
    check_swap_highlight(win, 0);
    check_swap_highlight(win, 1);
    check_empty_and_resized_cells(win);
    delwin(win);
    check_vertical_scroll();
    endwin();
    delscreen(screen);
    fclose(input);
    fclose(output);
    Py_Finalize();
    puts("interactive table rendering tests passed");
    return 0;
}
