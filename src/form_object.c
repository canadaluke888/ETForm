#include "form_object.h"

#include "dialogs.h"
#include "form_runtime.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static char *etform_strdup(const char *value) {
    char *copy = strdup(value ? value : "");
    if (!copy) {
        PyErr_NoMemory();
    }
    return copy;
}

static const char *align_name(ETFormLabelAlign align) {
    switch (align) {
        case ETFORM_LABEL_ALIGN_CENTER:
            return "center";
        case ETFORM_LABEL_ALIGN_RIGHT:
            return "right";
        case ETFORM_LABEL_ALIGN_LEFT:
        default:
            return "left";
    }
}

static const char *color_name(ETFormTextColor color) {
    switch (color) {
        case ETFORM_TEXT_COLOR_WHITE:
            return "white";
        case ETFORM_TEXT_COLOR_CYAN:
            return "cyan";
        case ETFORM_TEXT_COLOR_YELLOW:
            return "yellow";
        case ETFORM_TEXT_COLOR_GREEN:
            return "green";
        case ETFORM_TEXT_COLOR_BLUE:
            return "blue";
        case ETFORM_TEXT_COLOR_MAGENTA:
            return "magenta";
        case ETFORM_TEXT_COLOR_RED:
            return "red";
        case ETFORM_TEXT_COLOR_DEFAULT:
        default:
            return "default";
    }
}

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

static int parse_label_object(PyObject *label_obj, int allow_empty, ETFormTextStyle *style);
int etform_text_style_copy(ETFormTextStyle *dest, const ETFormTextStyle *src, const char *fallback_value);

void etform_table_init(ETFormTable *table) {
    if (!table) {
        return;
    }
    memset(table, 0, sizeof(*table));
    etform_text_style_init(&table->title);
    table->show_lines = 0;
    table->color = ETFORM_TEXT_COLOR_DEFAULT;
    table->padding = ETFORM_DEFAULT_PADDING;
}

void etform_table_free(ETFormTable *table) {
    if (!table) {
        return;
    }
    etform_text_style_free(&table->title);
    for (int i = 0; i < table->column_count; i++) {
        free(table->columns[i]);
    }
    free(table->columns);
    for (int row = 0; row < table->row_count; row++) {
        if (!table->rows[row].is_separator) {
            for (int col = 0; col < table->column_count; col++) {
                free(table->rows[row].cells[col]);
            }
            free(table->rows[row].cells);
        } else {
            etform_text_style_free(&table->rows[row].separator);
        }
    }
    free(table->rows);
    etform_table_init(table);
}

static int etform_table_set_title(ETFormTable *table, const ETFormTextStyle *title) {
    if (!table) {
        return -1;
    }
    ETFormTextStyle copy;
    if (etform_text_style_copy(&copy, title, title && title->value ? title->value : "") < 0) {
        return -1;
    }
    etform_text_style_free(&table->title);
    table->title = copy;
    return 0;
}

static int etform_table_ensure_column_capacity(ETFormTable *table, int needed) {
    if (!table || needed < 0) {
        return -1;
    }
    if (table->column_capacity >= needed) {
        return 0;
    }
    int next_capacity = table->column_capacity ? table->column_capacity * 2 : 4;
    while (next_capacity < needed) {
        next_capacity *= 2;
    }
    char **next = realloc(table->columns, (size_t)next_capacity * sizeof(char *));
    if (!next) {
        PyErr_NoMemory();
        return -1;
    }
    for (int i = table->column_capacity; i < next_capacity; i++) {
        next[i] = NULL;
    }
    table->columns = next;
    table->column_capacity = next_capacity;
    return 0;
}

static int etform_table_ensure_row_capacity(ETFormTable *table, int needed) {
    if (!table || needed < 0) {
        return -1;
    }
    if (table->row_capacity >= needed) {
        return 0;
    }
    int next_capacity = table->row_capacity ? table->row_capacity * 2 : 8;
    while (next_capacity < needed) {
        next_capacity *= 2;
    }
    ETFormTableRow *next = realloc(table->rows, (size_t)next_capacity * sizeof(ETFormTableRow));
    if (!next) {
        PyErr_NoMemory();
        return -1;
    }
    memset(next + table->row_capacity, 0, (size_t)(next_capacity - table->row_capacity) * sizeof(ETFormTableRow));
    table->rows = next;
    table->row_capacity = next_capacity;
    return 0;
}

static int etform_table_add_column_value(ETFormTable *table, const char *name) {
    if (!table || !name || !*name) {
        PyErr_SetString(PyExc_ValueError, "column name must not be empty");
        return -1;
    }
    if (table->row_count > 0) {
        PyErr_SetString(PyExc_ValueError, "columns must be added before rows or separators");
        return -1;
    }
    if (etform_table_ensure_column_capacity(table, table->column_count + 1) < 0) {
        return -1;
    }
    char *copy = etform_strdup(name);
    if (!copy) {
        return -1;
    }
    table->columns[table->column_count++] = copy;
    return 0;
}

static int etform_table_add_row_values(ETFormTable *table, PyObject *args) {
    if (!table || !args) {
        return -1;
    }
    if (table->column_count <= 0) {
        PyErr_SetString(PyExc_ValueError, "columns must be added before rows");
        return -1;
    }
    Py_ssize_t count = PyTuple_GET_SIZE(args);
    if (count != table->column_count) {
        PyErr_SetString(PyExc_ValueError, "row cell count must match column count");
        return -1;
    }
    char **cells = calloc((size_t)table->column_count, sizeof(char *));
    if (!cells) {
        PyErr_NoMemory();
        return -1;
    }
    for (int i = 0; i < table->column_count; i++) {
        PyObject *item = PyTuple_GET_ITEM(args, i);
        const char *cell = PyUnicode_AsUTF8(item);
        if (!cell) {
            for (int j = 0; j < i; j++) {
                free(cells[j]);
            }
            free(cells);
            PyErr_SetString(PyExc_TypeError, "table cells must be str");
            return -1;
        }
        cells[i] = etform_strdup(cell);
        if (!cells[i]) {
            for (int j = 0; j < i; j++) {
                free(cells[j]);
            }
            free(cells);
            return -1;
        }
    }
    if (etform_table_ensure_row_capacity(table, table->row_count + 1) < 0) {
        for (int i = 0; i < table->column_count; i++) {
            free(cells[i]);
        }
        free(cells);
        return -1;
    }
    ETFormTableRow *row = &table->rows[table->row_count++];
    row->is_separator = 0;
    row->cells = cells;
    return 0;
}

static int etform_table_add_separator_value(ETFormTable *table, const ETFormTextStyle *label) {
    if (!table) {
        return -1;
    }
    if (table->column_count <= 0) {
        PyErr_SetString(PyExc_ValueError, "columns must be added before separators");
        return -1;
    }
    if (etform_table_ensure_row_capacity(table, table->row_count + 1) < 0) {
        return -1;
    }
    ETFormTableRow *row = &table->rows[table->row_count++];
    row->is_separator = 1;
    row->cells = NULL;
    if (etform_text_style_copy(&row->separator, label, label && label->value ? label->value : "") < 0) {
        table->row_count--;
        memset(row, 0, sizeof(*row));
        return -1;
    }
    return 0;
}

int etform_table_copy(ETFormTable *dest, const ETFormTable *src) {
    if (!dest || !src) {
        return -1;
    }
    etform_table_init(dest);
    if (etform_table_set_title(dest, &src->title) < 0) {
        return -1;
    }
    dest->show_lines = src->show_lines;
    dest->color = src->color;
    dest->padding = src->padding;
    for (int col = 0; col < src->column_count; col++) {
        if (etform_table_add_column_value(dest, src->columns[col] ? src->columns[col] : "") < 0) {
            etform_table_free(dest);
            return -1;
        }
    }
    for (int row = 0; row < src->row_count; row++) {
        if (src->rows[row].is_separator) {
            if (etform_table_add_separator_value(dest, &src->rows[row].separator) < 0) {
                etform_table_free(dest);
                return -1;
            }
        } else {
            PyObject *tuple = PyTuple_New(src->column_count);
            if (!tuple) {
                etform_table_free(dest);
                return -1;
            }
            for (int col = 0; col < src->column_count; col++) {
                PyObject *cell = PyUnicode_FromString(src->rows[row].cells[col] ? src->rows[row].cells[col] : "");
                if (!cell) {
                    Py_DECREF(tuple);
                    etform_table_free(dest);
                    return -1;
                }
                PyTuple_SET_ITEM(tuple, col, cell);
            }
            if (etform_table_add_row_values(dest, tuple) < 0) {
                Py_DECREF(tuple);
                etform_table_free(dest);
                return -1;
            }
            Py_DECREF(tuple);
        }
    }
    return 0;
}

void etform_section_element_free(ETFormSectionElement *element) {
    if (!element) {
        return;
    }
    if (element->kind == ETFORM_SECTION_ELEMENT_TABLE) {
        etform_table_free(&element->table);
    } else if (element->kind == ETFORM_SECTION_ELEMENT_BRAILLE_CANVAS) {
        braille_canvas_free(&element->canvas);
    } else {
        etform_text_style_free(&element->text);
    }
    memset(element, 0, sizeof(*element));
}

void etform_field_free(ETFormField *field) {
    if (!field) {
        return;
    }
    free(field->key);
    etform_text_style_free(&field->label);
    free(field->description);
    free(field->file_picker_start_dir);
    free(field->text_value);
    free(field->default_value);
    etform_text_style_free(&field->placeholder);
    for (int i = 0; i < field->choice_count; i++) {
        free(field->choices[i]);
    }
    free(field->choices);
    for (int i = 0; i < field->list_value.count; i++) {
        etform_text_style_free(&field->list_styles[i]);
    }
    free(field->list_styles);
    etform_string_list_free(&field->list_value);
    for (int i = 0; i < field->checkbox_count; i++) {
        etform_text_style_free(&field->checkbox_options[i]);
    }
    free(field->checkbox_options);
    free(field->checkbox_checked);
    for (int i = 0; i < field->section_element_count; i++) {
        etform_section_element_free(&field->section_elements[i]);
    }
    free(field->section_elements);
    memset(field, 0, sizeof(*field));
}

void etform_text_style_init(ETFormTextStyle *style) {
    if (!style) {
        return;
    }
    style->value = NULL;
    style->align = ETFORM_LABEL_ALIGN_LEFT;
    style->bold = 0;
    style->italicized = 0;
    style->dim = 0;
    style->underline = 0;
    style->blink = 0;
    style->strikethrough = 0;
    style->color = ETFORM_TEXT_COLOR_DEFAULT;
    style->wrap_text = 0;
}

void etform_text_style_free(ETFormTextStyle *style) {
    if (!style) {
        return;
    }
    free(style->value);
    etform_text_style_init(style);
}

static int etform_text_style_set_value(ETFormTextStyle *style, const char *value) {
    if (!style) {
        return -1;
    }
    char *copy = etform_strdup(value);
    if (!copy) {
        return -1;
    }
    free(style->value);
    style->value = copy;
    return 0;
}

int etform_text_style_copy(ETFormTextStyle *dest, const ETFormTextStyle *src, const char *fallback_value) {
    if (!dest) {
        return -1;
    }
    etform_text_style_init(dest);
    if (etform_text_style_set_value(dest, src && src->value ? src->value : fallback_value ? fallback_value : "") < 0) {
        return -1;
    }
    if (src) {
        dest->align = src->align;
        dest->bold = src->bold;
        dest->italicized = src->italicized;
        dest->dim = src->dim;
        dest->underline = src->underline;
        dest->blink = src->blink;
        dest->strikethrough = src->strikethrough;
        dest->color = src->color;
        dest->wrap_text = src->wrap_text;
    }
    return 0;
}

static int etform_list_styles_ensure_capacity(ETFormField *field, int needed) {
    if (!field || needed < 0) {
        return -1;
    }
    if (field->list_style_capacity >= needed) {
        return 0;
    }
    int next_capacity = field->list_style_capacity ? field->list_style_capacity * 2 : 8;
    while (next_capacity < needed) {
        next_capacity *= 2;
    }
    ETFormTextStyle *next = realloc(field->list_styles, (size_t)next_capacity * sizeof(ETFormTextStyle));
    if (!next) {
        PyErr_NoMemory();
        return -1;
    }
    for (int i = field->list_style_capacity; i < next_capacity; i++) {
        etform_text_style_init(&next[i]);
    }
    field->list_styles = next;
    field->list_style_capacity = next_capacity;
    return 0;
}

int etform_list_append_value(ETFormField *field, const char *value, const ETFormTextStyle *style) {
    if (!field || !value || etform_list_styles_ensure_capacity(field, field->list_value.count + 1) < 0) {
        return -1;
    }
    if (etform_string_list_append(&field->list_value, value) < 0) {
        return -1;
    }
    ETFormTextStyle row_style;
    if (etform_text_style_copy(&row_style, style, value) < 0) {
        etform_string_list_delete(&field->list_value, field->list_value.count - 1);
        return -1;
    }
    field->list_styles[field->list_value.count - 1] = row_style;
    return 0;
}

int etform_list_set_value(ETFormField *field, int index, const char *value, const ETFormTextStyle *style) {
    if (!field || index < 0 || index >= field->list_value.count || !value) {
        return -1;
    }
    if (etform_string_list_set(&field->list_value, index, value) < 0) {
        return -1;
    }
    if (style) {
        ETFormTextStyle next_style;
        if (etform_text_style_copy(&next_style, style, value) < 0) {
            return -1;
        }
        etform_text_style_free(&field->list_styles[index]);
        field->list_styles[index] = next_style;
    } else if (field->list_styles && index < field->list_style_capacity) {
        if (etform_text_style_set_value(&field->list_styles[index], value) < 0) {
            return -1;
        }
    }
    return 0;
}

int etform_list_delete_value(ETFormField *field, int index) {
    if (!field || index < 0 || index >= field->list_value.count) {
        return -1;
    }
    etform_text_style_free(&field->list_styles[index]);
    if (etform_string_list_delete(&field->list_value, index) < 0) {
        return -1;
    }
    for (int i = index; i < field->list_value.count; i++) {
        field->list_styles[i] = field->list_styles[i + 1];
    }
    if (field->list_styles && field->list_value.count < field->list_style_capacity) {
        etform_text_style_init(&field->list_styles[field->list_value.count]);
    }
    return 0;
}

static int ensure_field_capacity(ETFormObject *self) {
    if (self->field_count < self->field_capacity) {
        return 0;
    }
    int next_capacity = self->field_capacity ? self->field_capacity * 2 : 8;
    ETFormField *next = realloc(self->fields, (size_t)next_capacity * sizeof(ETFormField));
    if (!next) {
        PyErr_NoMemory();
        return -1;
    }
    memset(next + self->field_capacity, 0, (size_t)(next_capacity - self->field_capacity) * sizeof(ETFormField));
    self->fields = next;
    self->field_capacity = next_capacity;
    return 0;
}

static ETFormField *append_field(
    ETFormObject *self,
    const char *key,
    const char *description,
    ETFormFieldKind kind
) {
    if (ensure_field_capacity(self) < 0) {
        return NULL;
    }
    ETFormField *field = &self->fields[self->field_count];
    memset(field, 0, sizeof(*field));
    etform_text_style_init(&field->label);
    etform_text_style_init(&field->placeholder);
    etform_string_list_init(&field->list_value);
    field->padding = ETFORM_DEFAULT_PADDING;
    field->key = etform_strdup(key);
    field->description = etform_strdup(description ? description : "");
    field->kind = kind;
    if (!field->key || !field->description) {
        etform_field_free(field);
        return NULL;
    }
    self->field_count++;
    return field;
}

static int parse_label_align(const char *value, ETFormLabelAlign *align) {
    if (!value || !align) {
        PyErr_SetString(PyExc_ValueError, "align must be 'left', 'center', or 'right'");
        return -1;
    }
    if (strcmp(value, "left") == 0) {
        *align = ETFORM_LABEL_ALIGN_LEFT;
        return 0;
    }
    if (strcmp(value, "center") == 0) {
        *align = ETFORM_LABEL_ALIGN_CENTER;
        return 0;
    }
    if (strcmp(value, "right") == 0) {
        *align = ETFORM_LABEL_ALIGN_RIGHT;
        return 0;
    }
    PyErr_SetString(PyExc_ValueError, "align must be 'left', 'center', or 'right'");
    return -1;
}

static int parse_padding(PyObject *value, int *padding) {
    if (!padding) {
        return -1;
    }
    *padding = ETFORM_DEFAULT_PADDING;
    if (!value || value == Py_None) {
        return 0;
    }
    long parsed = PyLong_AsLong(value);
    if (PyErr_Occurred()) {
        return -1;
    }
    if (parsed < 0 || parsed > 20) {
        PyErr_SetString(PyExc_ValueError, "padding must be an integer between 0 and 20");
        return -1;
    }
    *padding = (int)parsed;
    return 0;
}

static int parse_text_color(const char *value, ETFormTextColor *color) {
    if (!value || !color) {
        PyErr_SetString(PyExc_ValueError, "color must be one of default, white, cyan, yellow, green, blue, magenta, or red");
        return -1;
    }
    if (strcmp(value, "default") == 0) {
        *color = ETFORM_TEXT_COLOR_DEFAULT;
        return 0;
    }
    if (strcmp(value, "white") == 0) {
        *color = ETFORM_TEXT_COLOR_WHITE;
        return 0;
    }
    if (strcmp(value, "cyan") == 0) {
        *color = ETFORM_TEXT_COLOR_CYAN;
        return 0;
    }
    if (strcmp(value, "yellow") == 0) {
        *color = ETFORM_TEXT_COLOR_YELLOW;
        return 0;
    }
    if (strcmp(value, "green") == 0) {
        *color = ETFORM_TEXT_COLOR_GREEN;
        return 0;
    }
    if (strcmp(value, "blue") == 0) {
        *color = ETFORM_TEXT_COLOR_BLUE;
        return 0;
    }
    if (strcmp(value, "magenta") == 0) {
        *color = ETFORM_TEXT_COLOR_MAGENTA;
        return 0;
    }
    if (strcmp(value, "red") == 0) {
        *color = ETFORM_TEXT_COLOR_RED;
        return 0;
    }
    PyErr_SetString(PyExc_ValueError, "color must be one of default, white, cyan, yellow, green, blue, magenta, or red");
    return -1;
}

static int parse_checkbox_tick(const char *value, ETFormCheckboxTick *tick) {
    if (!value || !tick) {
        PyErr_SetString(PyExc_ValueError, "tick must be 'x', 'block', or 'dot'");
        return -1;
    }
    if (strcmp(value, "x") == 0) {
        *tick = ETFORM_CHECKBOX_TICK_X;
        return 0;
    }
    if (strcmp(value, "block") == 0) {
        *tick = ETFORM_CHECKBOX_TICK_BLOCK;
        return 0;
    }
    if (strcmp(value, "dot") == 0) {
        *tick = ETFORM_CHECKBOX_TICK_DOT;
        return 0;
    }
    PyErr_SetString(PyExc_ValueError, "tick must be 'x', 'block', or 'dot'");
    return -1;
}

typedef struct {
    PyObject_HEAD
    ETFormTextStyle style;
} ETFormTextObject;

typedef struct {
    PyObject_HEAD
    int mode;
    char *start_dir;
} ETFormFilePickerObject;

typedef struct {
    PyObject_HEAD
} ETFormCalendarObject;

typedef struct {
    PyObject_HEAD
    int format_12h;
    int include_seconds;
} ETFormClockObject;

typedef struct {
    PyObject_HEAD
    BrailleCanvas canvas;
    ETFormTextColor color;
    int padding;
} ETFormBrailleCanvasObject;

typedef struct {
    PyObject_HEAD
    char *path;
    BrailleCanvas canvas;
    ETFormTextColor color;
    int padding;
} ETFormImageObject;

typedef struct {
    PyObject_HEAD
    ETFormTable table;
} ETFormTableObject;

static PyObject *Text_new(PyTypeObject *type, PyObject *args, PyObject *kwargs) {
    (void)args;
    (void)kwargs;
    ETFormTextObject *self = (ETFormTextObject *)type->tp_alloc(type, 0);
    if (!self) {
        return NULL;
    }
    etform_text_style_init(&self->style);
    return (PyObject *)self;
}

static int Text_init(ETFormTextObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {
        "value",
        "align",
        "bold",
        "color",
        "wrap_text",
        "italicized",
        "dim",
        "underline",
        "blink",
        "strikethrough",
        NULL
    };
    const char *value = NULL;
    const char *align_text = "left";
    int bold = 0;
    const char *color_text = "default";
    int wrap_text = 0;
    int italicized = 0;
    int dim = 0;
    int underline = 0;
    int blink = 0;
    int strikethrough = 0;
    ETFormLabelAlign align = ETFORM_LABEL_ALIGN_LEFT;
    ETFormTextColor color = ETFORM_TEXT_COLOR_DEFAULT;
    if (!PyArg_ParseTupleAndKeywords(
            args,
            kwargs,
            "s|spspppppp",
            kwlist,
            &value,
            &align_text,
            &bold,
            &color_text,
            &wrap_text,
            &italicized,
            &dim,
            &underline,
            &blink,
            &strikethrough
        )) {
        return -1;
    }
    if (parse_label_align(align_text, &align) < 0 || parse_text_color(color_text, &color) < 0) {
        return -1;
    }
    if (etform_text_style_set_value(&self->style, value) < 0) {
        return -1;
    }
    self->style.align = align;
    self->style.bold = bold ? 1 : 0;
    self->style.italicized = italicized ? 1 : 0;
    self->style.dim = dim ? 1 : 0;
    self->style.underline = underline ? 1 : 0;
    self->style.blink = blink ? 1 : 0;
    self->style.strikethrough = strikethrough ? 1 : 0;
    self->style.color = color;
    self->style.wrap_text = wrap_text ? 1 : 0;
    return 0;
}

static void Text_dealloc(ETFormTextObject *self) {
    etform_text_style_free(&self->style);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *Text_repr(ETFormTextObject *self) {
    PyObject *value = PyUnicode_FromString(self->style.value ? self->style.value : "");
    if (!value) {
        return NULL;
    }
    PyObject *value_repr = PyObject_Repr(value);
    Py_DECREF(value);
    if (!value_repr) {
        return NULL;
    }
    const char *repr_text = PyUnicode_AsUTF8(value_repr);
    if (!repr_text) {
        Py_DECREF(value_repr);
        return NULL;
    }
    PyObject *repr = PyUnicode_FromFormat(
        "Text(%s, align='%s', bold=%s, color='%s', wrap_text=%s, italicized=%s, dim=%s, underline=%s, blink=%s, strikethrough=%s)",
        repr_text,
        align_name(self->style.align),
        self->style.bold ? "True" : "False",
        color_name(self->style.color),
        self->style.wrap_text ? "True" : "False",
        self->style.italicized ? "True" : "False",
        self->style.dim ? "True" : "False",
        self->style.underline ? "True" : "False",
        self->style.blink ? "True" : "False",
        self->style.strikethrough ? "True" : "False"
    );
    Py_DECREF(value_repr);
    return repr;
}

static PyObject *Text_get_value(ETFormTextObject *self, void *closure) {
    (void)closure;
    return PyUnicode_FromString(self->style.value ? self->style.value : "");
}

static PyObject *Text_get_align(ETFormTextObject *self, void *closure) {
    (void)closure;
    return PyUnicode_FromString(align_name(self->style.align));
}

static PyObject *Text_get_bold(ETFormTextObject *self, void *closure) {
    (void)closure;
    return PyBool_FromLong(self->style.bold);
}

static PyObject *Text_get_italicized(ETFormTextObject *self, void *closure) {
    (void)closure;
    return PyBool_FromLong(self->style.italicized);
}

static PyObject *Text_get_dim(ETFormTextObject *self, void *closure) {
    (void)closure;
    return PyBool_FromLong(self->style.dim);
}

static PyObject *Text_get_underline(ETFormTextObject *self, void *closure) {
    (void)closure;
    return PyBool_FromLong(self->style.underline);
}

static PyObject *Text_get_blink(ETFormTextObject *self, void *closure) {
    (void)closure;
    return PyBool_FromLong(self->style.blink);
}

static PyObject *Text_get_strikethrough(ETFormTextObject *self, void *closure) {
    (void)closure;
    return PyBool_FromLong(self->style.strikethrough);
}

static PyObject *Text_get_color(ETFormTextObject *self, void *closure) {
    (void)closure;
    return PyUnicode_FromString(color_name(self->style.color));
}

static PyObject *Text_get_wrap_text(ETFormTextObject *self, void *closure) {
    (void)closure;
    return PyBool_FromLong(self->style.wrap_text);
}

static PyGetSetDef Text_getset[] = {
    {"value", (getter)Text_get_value, NULL, "Text value.", NULL},
    {"align", (getter)Text_get_align, NULL, "Text alignment.", NULL},
    {"bold", (getter)Text_get_bold, NULL, "Whether the text is bold.", NULL},
    {"italicized", (getter)Text_get_italicized, NULL, "Whether the text is italicized.", NULL},
    {"dim", (getter)Text_get_dim, NULL, "Whether the text is dim.", NULL},
    {"underline", (getter)Text_get_underline, NULL, "Whether the text is underlined.", NULL},
    {"blink", (getter)Text_get_blink, NULL, "Whether the text blinks.", NULL},
    {"strikethrough", (getter)Text_get_strikethrough, NULL, "Whether the text has strikethrough.", NULL},
    {"color", (getter)Text_get_color, NULL, "Text color.", NULL},
    {"wrap_text", (getter)Text_get_wrap_text, NULL, "Whether the text wraps.", NULL},
    {NULL, NULL, NULL, NULL, NULL},
};

PyTypeObject ETFormTextType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "etform.Text",
    .tp_basicsize = sizeof(ETFormTextObject),
    .tp_itemsize = 0,
    .tp_dealloc = (destructor)Text_dealloc,
    .tp_repr = (reprfunc)Text_repr,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Styled label text.",
    .tp_getset = Text_getset,
    .tp_init = (initproc)Text_init,
    .tp_new = Text_new,
};

static int parse_file_picker_mode(const char *value, int *mode) {
    if (!value || !mode) {
        PyErr_SetString(PyExc_ValueError, "mode must be 'file' or 'directory'");
        return -1;
    }
    if (strcmp(value, "file") == 0) {
        *mode = ETFORM_FILE_PICKER_FILE;
        return 0;
    }
    if (strcmp(value, "directory") == 0 || strcmp(value, "dir") == 0) {
        *mode = ETFORM_FILE_PICKER_DIRECTORY;
        return 0;
    }
    PyErr_SetString(PyExc_ValueError, "mode must be 'file' or 'directory'");
    return -1;
}

static const char *file_picker_mode_name(int mode) {
    return mode == ETFORM_FILE_PICKER_DIRECTORY ? "directory" : "file";
}

static char *validate_file_picker_start_dir(const char *value) {
    if (!value || !*value) {
        PyErr_SetString(PyExc_ValueError, "start_dir must be a non-empty directory path");
        return NULL;
    }
    struct stat st;
    if (stat(value, &st) != 0 || !S_ISDIR(st.st_mode)) {
        PyErr_SetString(PyExc_ValueError, "start_dir must be an existing directory");
        return NULL;
    }
    char *copy = etform_strdup(value);
    return copy;
}

static int FilePicker_init(ETFormFilePickerObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"mode", "start_dir", NULL};
    const char *mode_text = "file";
    const char *start_dir = ".";
    int mode = ETFORM_FILE_PICKER_FILE;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|ss", kwlist, &mode_text, &start_dir)) {
        return -1;
    }
    if (parse_file_picker_mode(mode_text, &mode) < 0) {
        return -1;
    }
    char *start_dir_copy = validate_file_picker_start_dir(start_dir);
    if (!start_dir_copy) {
        return -1;
    }
    free(self->start_dir);
    self->mode = mode;
    self->start_dir = start_dir_copy;
    return 0;
}

static void FilePicker_dealloc(ETFormFilePickerObject *self) {
    free(self->start_dir);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *FilePicker_repr(ETFormFilePickerObject *self) {
    return PyUnicode_FromFormat(
        "FilePicker(mode='%s', start_dir='%s')",
        file_picker_mode_name(self->mode),
        self->start_dir ? self->start_dir : "."
    );
}

static PyObject *FilePicker_get_mode(ETFormFilePickerObject *self, void *closure) {
    (void)closure;
    return PyUnicode_FromString(file_picker_mode_name(self->mode));
}

static PyObject *FilePicker_get_start_dir(ETFormFilePickerObject *self, void *closure) {
    (void)closure;
    return PyUnicode_FromString(self->start_dir ? self->start_dir : ".");
}

static PyGetSetDef FilePicker_getset[] = {
    {"mode", (getter)FilePicker_get_mode, NULL, "Picker mode.", NULL},
    {"start_dir", (getter)FilePicker_get_start_dir, NULL, "Initial picker directory.", NULL},
    {NULL, NULL, NULL, NULL, NULL},
};

PyTypeObject ETFormFilePickerType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "etform.FilePicker",
    .tp_basicsize = sizeof(ETFormFilePickerObject),
    .tp_itemsize = 0,
    .tp_dealloc = (destructor)FilePicker_dealloc,
    .tp_repr = (reprfunc)FilePicker_repr,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Tool that opens a file or directory picker for compatible fields.",
    .tp_getset = FilePicker_getset,
    .tp_init = (initproc)FilePicker_init,
    .tp_new = PyType_GenericNew,
};

static PyObject *Calendar_repr(ETFormCalendarObject *self) {
    (void)self;
    return PyUnicode_FromString("Calendar()");
}

PyTypeObject ETFormCalendarType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "etform.Calendar",
    .tp_basicsize = sizeof(ETFormCalendarObject),
    .tp_itemsize = 0,
    .tp_repr = (reprfunc)Calendar_repr,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Tool that opens an interactive calendar for compatible fields.",
    .tp_new = PyType_GenericNew,
};

static int Clock_init(ETFormClockObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"format", "include_seconds", NULL};
    const char *format = "24h";
    int include_seconds = 0;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|sp", kwlist, &format, &include_seconds)) {
        return -1;
    }
    if (strcmp(format, "24h") != 0 && strcmp(format, "12h") != 0) {
        PyErr_SetString(PyExc_ValueError, "format must be '24h' or '12h'");
        return -1;
    }
    self->format_12h = strcmp(format, "12h") == 0;
    self->include_seconds = include_seconds ? 1 : 0;
    return 0;
}

static PyObject *Clock_repr(ETFormClockObject *self) {
    return PyUnicode_FromFormat(
        "Clock(format='%s', include_seconds=%s)",
        self->format_12h ? "12h" : "24h",
        self->include_seconds ? "True" : "False"
    );
}

static PyObject *Clock_get_format(ETFormClockObject *self, void *closure) {
    (void)closure;
    return PyUnicode_FromString(self->format_12h ? "12h" : "24h");
}

static PyObject *Clock_get_include_seconds(ETFormClockObject *self, void *closure) {
    (void)closure;
    return PyBool_FromLong(self->include_seconds);
}

static PyGetSetDef Clock_getset[] = {
    {"format", (getter)Clock_get_format, NULL, "Clock display format.", NULL},
    {"include_seconds", (getter)Clock_get_include_seconds, NULL, "Whether seconds are returned.", NULL},
    {NULL, NULL, NULL, NULL, NULL},
};

PyTypeObject ETFormClockType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "etform.Clock",
    .tp_basicsize = sizeof(ETFormClockObject),
    .tp_itemsize = 0,
    .tp_repr = (reprfunc)Clock_repr,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Tool that opens an interactive time picker for compatible fields.",
    .tp_getset = Clock_getset,
    .tp_init = (initproc)Clock_init,
    .tp_new = PyType_GenericNew,
};

static PyObject *BrailleCanvas_new(PyTypeObject *type, PyObject *args, PyObject *kwargs) {
    (void)args;
    (void)kwargs;
    ETFormBrailleCanvasObject *self = (ETFormBrailleCanvasObject *)type->tp_alloc(type, 0);
    if (!self) return NULL;
    memset(&self->canvas, 0, sizeof(self->canvas));
    self->color = ETFORM_TEXT_COLOR_DEFAULT;
    self->padding = ETFORM_DEFAULT_PADDING;
    return (PyObject *)self;
}

static int BrailleCanvas_init(ETFormBrailleCanvasObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"width", "height", "color", "padding", NULL};
    int width;
    int height;
    const char *color_text = "default";
    PyObject *padding_obj = Py_None;
    ETFormTextColor color;
    int padding;
    BrailleCanvas canvas;
    memset(&canvas, 0, sizeof(canvas));
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "ii|sO", kwlist,
                                     &width, &height, &color_text, &padding_obj)) {
        return -1;
    }
    if (parse_text_color(color_text, &color) < 0 ||
        parse_padding(padding_obj, &padding) < 0) {
        return -1;
    }
    if (braille_canvas_init(&canvas, width, height) < 0) {
        PyErr_SetString(PyExc_ValueError, "width and height must be positive supported canvas dimensions");
        return -1;
    }
    braille_canvas_free(&self->canvas);
    self->canvas = canvas;
    self->color = color;
    self->padding = padding;
    return 0;
}

static void BrailleCanvas_dealloc(ETFormBrailleCanvasObject *self) {
    braille_canvas_free(&self->canvas);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *BrailleCanvas_repr(ETFormBrailleCanvasObject *self) {
    return PyUnicode_FromFormat(
        "BrailleCanvas(width=%d, height=%d, color='%s', padding=%d)",
        self->canvas.char_width,
        self->canvas.char_height,
        color_name(self->color),
        self->padding
    );
}

static PyObject *BrailleCanvas_set_dot(ETFormBrailleCanvasObject *self, PyObject *args) {
    int x;
    int y;
    if (!PyArg_ParseTuple(args, "ii", &x, &y)) return NULL;
    if (!braille_canvas_set_dot(&self->canvas, x, y)) {
        PyErr_SetString(PyExc_ValueError, "dot coordinates are outside the canvas");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *BrailleCanvas_draw_line(ETFormBrailleCanvasObject *self, PyObject *args) {
    int x0;
    int y0;
    int x1;
    int y1;
    if (!PyArg_ParseTuple(args, "iiii", &x0, &y0, &x1, &y1)) return NULL;
    if (braille_canvas_draw_line(&self->canvas, x0, y0, x1, y1) < 0) {
        PyErr_SetString(PyExc_RuntimeError, "canvas is not initialized");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *BrailleCanvas_draw_circle(ETFormBrailleCanvasObject *self, PyObject *args) {
    int center_x;
    int center_y;
    int radius;
    if (!PyArg_ParseTuple(args, "iii", &center_x, &center_y, &radius)) return NULL;
    if (radius < 0) {
        PyErr_SetString(PyExc_ValueError, "radius must not be negative");
        return NULL;
    }
    if (braille_canvas_draw_circle(&self->canvas, center_x, center_y, radius) < 0) {
        PyErr_SetString(PyExc_RuntimeError, "canvas is not initialized");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *BrailleCanvas_draw_image(ETFormBrailleCanvasObject *self, PyObject *args) {
    PyObject *path_bytes = NULL;
    const char *path;
    int dots;
    struct stat path_stat;
    if (!PyArg_ParseTuple(args, "O&", PyUnicode_FSConverter, &path_bytes)) {
        return NULL;
    }
    path = PyBytes_AS_STRING(path_bytes);
    if (stat(path, &path_stat) != 0) {
        PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);
        Py_DECREF(path_bytes);
        return NULL;
    }
    dots = braille_canvas_draw_image(&self->canvas, path);
    if (dots < 0) {
        PyErr_Format(
            PyExc_ValueError,
            "unable to draw image '%s': %s",
            path,
            braille_canvas_image_error()
        );
        Py_DECREF(path_bytes);
        return NULL;
    }
    Py_DECREF(path_bytes);
    Py_RETURN_NONE;
}

static PyObject *BrailleCanvas_clear(ETFormBrailleCanvasObject *self, PyObject *Py_UNUSED(ignored)) {
    braille_canvas_clear(&self->canvas);
    Py_RETURN_NONE;
}

static PyObject *BrailleCanvas_get_width(ETFormBrailleCanvasObject *self, void *closure) {
    (void)closure;
    return PyLong_FromLong(self->canvas.char_width);
}

static PyObject *BrailleCanvas_get_height(ETFormBrailleCanvasObject *self, void *closure) {
    (void)closure;
    return PyLong_FromLong(self->canvas.char_height);
}

static PyObject *BrailleCanvas_get_dot_width(ETFormBrailleCanvasObject *self, void *closure) {
    (void)closure;
    return PyLong_FromLong(braille_canvas_dot_width(&self->canvas));
}

static PyObject *BrailleCanvas_get_dot_height(ETFormBrailleCanvasObject *self, void *closure) {
    (void)closure;
    return PyLong_FromLong(braille_canvas_dot_height(&self->canvas));
}

static PyObject *BrailleCanvas_get_color(ETFormBrailleCanvasObject *self, void *closure) {
    (void)closure;
    return PyUnicode_FromString(color_name(self->color));
}

static PyObject *BrailleCanvas_get_padding(ETFormBrailleCanvasObject *self, void *closure) {
    (void)closure;
    return PyLong_FromLong(self->padding);
}

static PyMethodDef BrailleCanvas_methods[] = {
    {"set_dot", (PyCFunction)BrailleCanvas_set_dot, METH_VARARGS, "Set one Braille dot."},
    {"draw_line", (PyCFunction)BrailleCanvas_draw_line, METH_VARARGS, "Draw a line in dot coordinates."},
    {"draw_circle", (PyCFunction)BrailleCanvas_draw_circle, METH_VARARGS, "Draw a circle in dot coordinates."},
    {"draw_image", (PyCFunction)BrailleCanvas_draw_image, METH_VARARGS, "Draw an image fitted to the canvas."},
    {"clear", (PyCFunction)BrailleCanvas_clear, METH_NOARGS, "Clear all dots."},
    {NULL, NULL, 0, NULL},
};

static PyGetSetDef BrailleCanvas_getset[] = {
    {"width", (getter)BrailleCanvas_get_width, NULL, "Canvas width in terminal cells.", NULL},
    {"height", (getter)BrailleCanvas_get_height, NULL, "Canvas height in terminal cells.", NULL},
    {"dot_width", (getter)BrailleCanvas_get_dot_width, NULL, "Canvas width in Braille dots.", NULL},
    {"dot_height", (getter)BrailleCanvas_get_dot_height, NULL, "Canvas height in Braille dots.", NULL},
    {"color", (getter)BrailleCanvas_get_color, NULL, "Canvas color.", NULL},
    {"padding", (getter)BrailleCanvas_get_padding, NULL, "Canvas vertical padding.", NULL},
    {NULL, NULL, NULL, NULL, NULL},
};

PyTypeObject ETFormBrailleCanvasType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "etform.BrailleCanvas",
    .tp_basicsize = sizeof(ETFormBrailleCanvasObject),
    .tp_itemsize = 0,
    .tp_dealloc = (destructor)BrailleCanvas_dealloc,
    .tp_repr = (reprfunc)BrailleCanvas_repr,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Static Unicode Braille canvas for form sections.",
    .tp_methods = BrailleCanvas_methods,
    .tp_getset = BrailleCanvas_getset,
    .tp_init = (initproc)BrailleCanvas_init,
    .tp_new = BrailleCanvas_new,
};

static PyObject *Image_new(PyTypeObject *type, PyObject *args, PyObject *kwargs) {
    (void)args;
    (void)kwargs;
    ETFormImageObject *self = (ETFormImageObject *)type->tp_alloc(type, 0);
    if (!self) return NULL;
    self->path = NULL;
    memset(&self->canvas, 0, sizeof(self->canvas));
    self->color = ETFORM_TEXT_COLOR_DEFAULT;
    self->padding = ETFORM_DEFAULT_PADDING;
    return (PyObject *)self;
}

static int Image_init(ETFormImageObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"path", "width", "height", "color", "padding", NULL};
    PyObject *path_bytes = NULL;
    int width = 36;
    PyObject *height_obj = Py_None;
    const char *color_text = "default";
    PyObject *padding_obj = Py_None;
    const char *path;
    struct stat path_stat;
    int source_width;
    int source_height;
    int height;
    int padding;
    ETFormTextColor color;
    BrailleCanvas canvas;
    char *path_copy;

    memset(&canvas, 0, sizeof(canvas));
    if (!PyArg_ParseTupleAndKeywords(
            args,
            kwargs,
            "O&|iOsO",
            kwlist,
            PyUnicode_FSConverter,
            &path_bytes,
            &width,
            &height_obj,
            &color_text,
            &padding_obj
        )) {
        return -1;
    }
    path = PyBytes_AS_STRING(path_bytes);
    if (stat(path, &path_stat) != 0) {
        PyErr_SetFromErrnoWithFilename(PyExc_OSError, path);
        Py_DECREF(path_bytes);
        return -1;
    }
    if (width <= 0) {
        PyErr_SetString(PyExc_ValueError, "width must be positive");
        Py_DECREF(path_bytes);
        return -1;
    }
    if (parse_text_color(color_text, &color) < 0 ||
        parse_padding(padding_obj, &padding) < 0) {
        Py_DECREF(path_bytes);
        return -1;
    }
    if (braille_canvas_image_size(path, &source_width, &source_height) < 0) {
        PyErr_Format(
            PyExc_ValueError,
            "unable to load image '%s': %s",
            path,
            braille_canvas_image_error()
        );
        Py_DECREF(path_bytes);
        return -1;
    }
    if (height_obj == Py_None) {
        long long numerator = (long long)source_height * (long long)width;
        long long denominator = (long long)source_width * 2LL;
        height = (int)((numerator + denominator - 1) / denominator);
        if (height < 1) height = 1;
    } else {
        long parsed_height = PyLong_AsLong(height_obj);
        if (parsed_height == -1 && PyErr_Occurred()) {
            Py_DECREF(path_bytes);
            return -1;
        }
        if (parsed_height <= 0 || parsed_height > INT_MAX) {
            PyErr_SetString(PyExc_ValueError, "height must be positive or None");
            Py_DECREF(path_bytes);
            return -1;
        }
        height = (int)parsed_height;
    }
    if (braille_canvas_init(&canvas, width, height) < 0) {
        PyErr_SetString(PyExc_ValueError, "image dimensions are too large");
        Py_DECREF(path_bytes);
        return -1;
    }
    if (braille_canvas_draw_image(&canvas, path) < 0) {
        PyErr_Format(
            PyExc_ValueError,
            "unable to draw image '%s': %s",
            path,
            braille_canvas_image_error()
        );
        braille_canvas_free(&canvas);
        Py_DECREF(path_bytes);
        return -1;
    }
    path_copy = etform_strdup(path);
    Py_DECREF(path_bytes);
    if (!path_copy) {
        braille_canvas_free(&canvas);
        return -1;
    }
    free(self->path);
    braille_canvas_free(&self->canvas);
    self->path = path_copy;
    self->canvas = canvas;
    self->color = color;
    self->padding = padding;
    return 0;
}

static void Image_dealloc(ETFormImageObject *self) {
    free(self->path);
    braille_canvas_free(&self->canvas);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *Image_repr(ETFormImageObject *self) {
    return PyUnicode_FromFormat(
        "Image(path='%s', width=%d, height=%d, color='%s', padding=%d)",
        self->path ? self->path : "",
        self->canvas.char_width,
        self->canvas.char_height,
        color_name(self->color),
        self->padding
    );
}

static PyObject *Image_get_path(ETFormImageObject *self, void *closure) {
    (void)closure;
    return PyUnicode_DecodeFSDefault(self->path ? self->path : "");
}

static PyObject *Image_get_width(ETFormImageObject *self, void *closure) {
    (void)closure;
    return PyLong_FromLong(self->canvas.char_width);
}

static PyObject *Image_get_height(ETFormImageObject *self, void *closure) {
    (void)closure;
    return PyLong_FromLong(self->canvas.char_height);
}

static PyObject *Image_get_color(ETFormImageObject *self, void *closure) {
    (void)closure;
    return PyUnicode_FromString(color_name(self->color));
}

static PyObject *Image_get_padding(ETFormImageObject *self, void *closure) {
    (void)closure;
    return PyLong_FromLong(self->padding);
}

static PyGetSetDef Image_getset[] = {
    {"path", (getter)Image_get_path, NULL, "Source image path.", NULL},
    {"width", (getter)Image_get_width, NULL, "Rendered width in terminal cells.", NULL},
    {"height", (getter)Image_get_height, NULL, "Rendered height in terminal cells.", NULL},
    {"color", (getter)Image_get_color, NULL, "Fallback image color.", NULL},
    {"padding", (getter)Image_get_padding, NULL, "Image vertical padding.", NULL},
    {NULL, NULL, NULL, NULL, NULL},
};

PyTypeObject ETFormImageType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "etform.Image",
    .tp_basicsize = sizeof(ETFormImageObject),
    .tp_itemsize = 0,
    .tp_dealloc = (destructor)Image_dealloc,
    .tp_repr = (reprfunc)Image_repr,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Portable image rendered with Unicode Braille cells.",
    .tp_getset = Image_getset,
    .tp_init = (initproc)Image_init,
    .tp_new = Image_new,
};

static PyObject *Table_new(PyTypeObject *type, PyObject *args, PyObject *kwargs) {
    (void)args;
    (void)kwargs;
    ETFormTableObject *self = (ETFormTableObject *)type->tp_alloc(type, 0);
    if (!self) {
        return NULL;
    }
    etform_table_init(&self->table);
    return (PyObject *)self;
}

static int Table_init(ETFormTableObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"title", "show_lines", "color", "padding", NULL};
    PyObject *title_obj = NULL;
    int show_lines = 0;
    const char *color_text = "default";
    PyObject *padding_obj = Py_None;
    ETFormTextColor color = ETFORM_TEXT_COLOR_DEFAULT;
    int padding = ETFORM_DEFAULT_PADDING;
    ETFormTextStyle title_style;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|OpsO", kwlist, &title_obj, &show_lines, &color_text, &padding_obj)) {
        return -1;
    }
    if (parse_text_color(color_text, &color) < 0 || parse_padding(padding_obj, &padding) < 0) {
        return -1;
    }
    if (title_obj) {
        if (parse_label_object(title_obj, 1, &title_style) < 0) {
            return -1;
        }
    } else {
        etform_text_style_init(&title_style);
        if (etform_text_style_set_value(&title_style, "") < 0) {
            return -1;
        }
    }
    etform_table_free(&self->table);
    etform_table_init(&self->table);
    if (etform_table_set_title(&self->table, &title_style) < 0) {
        etform_text_style_free(&title_style);
        return -1;
    }
    etform_text_style_free(&title_style);
    self->table.show_lines = show_lines ? 1 : 0;
    self->table.color = color;
    self->table.padding = padding;
    return 0;
}

static void Table_dealloc(ETFormTableObject *self) {
    etform_table_free(&self->table);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *Table_repr(ETFormTableObject *self) {
    return PyUnicode_FromFormat(
        "Table(title='%s', columns=%d, rows=%d)",
        self->table.title.value ? self->table.title.value : "",
        self->table.column_count,
        self->table.row_count
    );
}

static PyObject *Table_add_column(ETFormTableObject *self, PyObject *args) {
    const char *name = NULL;
    if (!PyArg_ParseTuple(args, "s", &name)) {
        return NULL;
    }
    if (etform_table_add_column_value(&self->table, name) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *Table_add_row(ETFormTableObject *self, PyObject *args) {
    if (etform_table_add_row_values(&self->table, args) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *Table_add_separator_row(ETFormTableObject *self, PyObject *args) {
    PyObject *label_obj = NULL;
    ETFormTextStyle label_style;
    if (!PyArg_ParseTuple(args, "|O", &label_obj)) {
        return NULL;
    }
    if (label_obj) {
        if (parse_label_object(label_obj, 1, &label_style) < 0) {
            return NULL;
        }
    } else {
        etform_text_style_init(&label_style);
        if (etform_text_style_set_value(&label_style, "") < 0) {
            return NULL;
        }
    }
    if (etform_table_add_separator_value(&self->table, &label_style) < 0) {
        etform_text_style_free(&label_style);
        return NULL;
    }
    etform_text_style_free(&label_style);
    Py_RETURN_NONE;
}

static PyObject *Table_get_title(ETFormTableObject *self, void *closure) {
    (void)closure;
    return PyUnicode_FromString(self->table.title.value ? self->table.title.value : "");
}

static PyObject *Table_get_show_lines(ETFormTableObject *self, void *closure) {
    (void)closure;
    return PyBool_FromLong(self->table.show_lines);
}

static PyObject *Table_get_color(ETFormTableObject *self, void *closure) {
    (void)closure;
    return PyUnicode_FromString(color_name(self->table.color));
}

static PyObject *Table_get_padding(ETFormTableObject *self, void *closure) {
    (void)closure;
    return PyLong_FromLong(self->table.padding);
}

static PyMethodDef Table_methods[] = {
    {"add_column", (PyCFunction)Table_add_column, METH_VARARGS, "Add a table column."},
    {"add_row", (PyCFunction)Table_add_row, METH_VARARGS, "Add a table row."},
    {"add_separator_row", (PyCFunction)Table_add_separator_row, METH_VARARGS, "Add a separator row."},
    {NULL, NULL, 0, NULL},
};

static PyGetSetDef Table_getset[] = {
    {"title", (getter)Table_get_title, NULL, "Table title.", NULL},
    {"show_lines", (getter)Table_get_show_lines, NULL, "Whether inner grid lines are shown.", NULL},
    {"color", (getter)Table_get_color, NULL, "Table border color.", NULL},
    {"padding", (getter)Table_get_padding, NULL, "Table padding.", NULL},
    {NULL, NULL, NULL, NULL, NULL},
};

PyTypeObject ETFormTableType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "etform.Table",
    .tp_basicsize = sizeof(ETFormTableObject),
    .tp_itemsize = 0,
    .tp_dealloc = (destructor)Table_dealloc,
    .tp_repr = (reprfunc)Table_repr,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Static table for form sections.",
    .tp_methods = Table_methods,
    .tp_getset = Table_getset,
    .tp_init = (initproc)Table_init,
    .tp_new = Table_new,
};

static int apply_tool(ETFormField *field, PyObject *tool) {
    if (PyObject_TypeCheck(tool, &ETFormFilePickerType)) {
        ETFormFilePickerObject *picker = (ETFormFilePickerObject *)tool;
        char *start_dir = etform_strdup(picker->start_dir ? picker->start_dir : ".");
        if (!start_dir) {
            return -1;
        }
        free(field->file_picker_start_dir);
        field->has_file_picker = 1;
        field->file_picker_mode = picker->mode;
        field->file_picker_start_dir = start_dir;
        return 0;
    }
    if (PyObject_TypeCheck(tool, &ETFormCalendarType)) {
        field->has_calendar = 1;
        return 0;
    }
    if (PyObject_TypeCheck(tool, &ETFormClockType)) {
        ETFormClockObject *clock = (ETFormClockObject *)tool;
        field->has_clock = 1;
        field->clock_format_12h = clock->format_12h;
        field->clock_include_seconds = clock->include_seconds;
        return 0;
    }
    PyErr_SetString(PyExc_TypeError, "tools must contain FilePicker, Calendar, or Clock instances");
    return -1;
}

static int apply_tools(ETFormField *field, PyObject *tools) {
    if (!tools || tools == Py_None) {
        return 0;
    }
    if (PyObject_TypeCheck(tools, &ETFormFilePickerType) ||
        PyObject_TypeCheck(tools, &ETFormCalendarType) ||
        PyObject_TypeCheck(tools, &ETFormClockType)) {
        return apply_tool(field, tools);
    }
    PyObject *seq = PySequence_Fast(tools, "tools must be a FilePicker, Calendar, Clock, or a sequence of tools");
    if (!seq) {
        return -1;
    }
    Py_ssize_t count = PySequence_Fast_GET_SIZE(seq);
    PyObject **items = PySequence_Fast_ITEMS(seq);
    for (Py_ssize_t i = 0; i < count; i++) {
        if (apply_tool(field, items[i]) < 0) {
            Py_DECREF(seq);
            return -1;
        }
    }
    Py_DECREF(seq);
    return 0;
}

static int parse_label_object(
    PyObject *label_obj,
    int allow_empty,
    ETFormTextStyle *style
) {
    if (!style) {
        return -1;
    }
    etform_text_style_init(style);
    if (PyObject_TypeCheck(label_obj, &ETFormTextType)) {
        ETFormTextObject *text = (ETFormTextObject *)label_obj;
        if (!allow_empty && (!text->style.value || !*text->style.value)) {
            PyErr_SetString(PyExc_ValueError, "field label must not be empty");
            return -1;
        }
        if (etform_text_style_set_value(style, text->style.value ? text->style.value : "") < 0) {
            return -1;
        }
        style->align = text->style.align;
        style->bold = text->style.bold;
        style->italicized = text->style.italicized;
        style->dim = text->style.dim;
        style->underline = text->style.underline;
        style->blink = text->style.blink;
        style->strikethrough = text->style.strikethrough;
        style->color = text->style.color;
        style->wrap_text = text->style.wrap_text;
        return 0;
    }
    const char *label = PyUnicode_AsUTF8(label_obj);
    if (!label) {
        PyErr_SetString(PyExc_TypeError, "label must be str or Text");
        return -1;
    }
    if (!allow_empty && !*label) {
        PyErr_SetString(PyExc_ValueError, "field label must not be empty");
        return -1;
    }
    if (etform_text_style_set_value(style, label) < 0) {
        return -1;
    }
    style->align = ETFORM_LABEL_ALIGN_LEFT;
    style->bold = 0;
    style->italicized = 0;
    style->dim = 0;
    style->underline = 0;
    style->blink = 0;
    style->strikethrough = 0;
    style->color = ETFORM_TEXT_COLOR_DEFAULT;
    style->wrap_text = 0;
    return 0;
}

int etform_text_style_from_object(
    PyObject *object,
    int allow_empty,
    ETFormTextStyle *style
) {
    return parse_label_object(object, allow_empty, style);
}

static int etform_section_ensure_capacity(ETFormField *field, int needed) {
    if (!field || needed < 0) {
        return -1;
    }
    if (field->section_element_capacity >= needed) {
        return 0;
    }
    int next_capacity = field->section_element_capacity ? field->section_element_capacity * 2 : 4;
    while (next_capacity < needed) {
        next_capacity *= 2;
    }
    ETFormSectionElement *next = realloc(field->section_elements, (size_t)next_capacity * sizeof(ETFormSectionElement));
    if (!next) {
        PyErr_NoMemory();
        return -1;
    }
    memset(
        next + field->section_element_capacity,
        0,
        (size_t)(next_capacity - field->section_element_capacity) * sizeof(ETFormSectionElement)
    );
    field->section_elements = next;
    field->section_element_capacity = next_capacity;
    return 0;
}

static int etform_section_append_text(ETFormField *field, const ETFormTextStyle *style) {
    if (!field || !style) {
        return -1;
    }
    if (etform_section_ensure_capacity(field, field->section_element_count + 1) < 0) {
        return -1;
    }
    ETFormSectionElement *element = &field->section_elements[field->section_element_count];
    memset(element, 0, sizeof(*element));
    element->kind = ETFORM_SECTION_ELEMENT_TEXT;
    if (etform_text_style_copy(&element->text, style, style->value ? style->value : "") < 0) {
        return -1;
    }
    field->section_element_count++;
    return 0;
}

static int etform_section_append_table(ETFormField *field, const ETFormTable *table) {
    if (!field || !table) {
        return -1;
    }
    if (etform_section_ensure_capacity(field, field->section_element_count + 1) < 0) {
        return -1;
    }
    ETFormSectionElement *element = &field->section_elements[field->section_element_count];
    memset(element, 0, sizeof(*element));
    element->kind = ETFORM_SECTION_ELEMENT_TABLE;
    if (etform_table_copy(&element->table, table) < 0) {
        return -1;
    }
    field->section_element_count++;
    return 0;
}

static int etform_section_append_canvas(
    ETFormField *field,
    const BrailleCanvas *canvas,
    ETFormTextColor color,
    int padding
) {
    if (!field || !canvas) return -1;
    if (etform_section_ensure_capacity(field, field->section_element_count + 1) < 0) return -1;
    ETFormSectionElement *element = &field->section_elements[field->section_element_count];
    memset(element, 0, sizeof(*element));
    element->kind = ETFORM_SECTION_ELEMENT_BRAILLE_CANVAS;
    if (braille_canvas_copy(&element->canvas, canvas) < 0) {
        PyErr_NoMemory();
        return -1;
    }
    element->canvas_color = color;
    element->canvas_padding = padding;
    field->section_element_count++;
    return 0;
}

static int etform_section_append_object(ETFormField *field, PyObject *item) {
    if (PyObject_TypeCheck(item, &ETFormTableType)) {
        ETFormTableObject *table = (ETFormTableObject *)item;
        return etform_section_append_table(field, &table->table);
    }
    if (PyObject_TypeCheck(item, &ETFormBrailleCanvasType)) {
        ETFormBrailleCanvasObject *canvas = (ETFormBrailleCanvasObject *)item;
        return etform_section_append_canvas(field, &canvas->canvas, canvas->color, canvas->padding);
    }
    if (PyObject_TypeCheck(item, &ETFormImageType)) {
        ETFormImageObject *image = (ETFormImageObject *)item;
        return etform_section_append_canvas(field, &image->canvas, image->color, image->padding);
    }
    ETFormTextStyle text_style;
    if (parse_label_object(item, 1, &text_style) < 0) {
        PyErr_SetString(PyExc_TypeError, "section items must be str, Text, Table, BrailleCanvas, or Image");
        return -1;
    }
    int result = etform_section_append_text(field, &text_style);
    etform_text_style_free(&text_style);
    return result;
}

int etform_section_element_from_object(
    ETFormSectionElement *element,
    PyObject *item
) {
    if (!element || !item) {
        PyErr_SetString(PyExc_TypeError, "document item must not be None");
        return -1;
    }
    memset(element, 0, sizeof(*element));
    if (PyObject_TypeCheck(item, &ETFormTableType)) {
        ETFormTableObject *table = (ETFormTableObject *)item;
        element->kind = ETFORM_SECTION_ELEMENT_TABLE;
        return etform_table_copy(&element->table, &table->table);
    }
    if (PyObject_TypeCheck(item, &ETFormBrailleCanvasType)) {
        ETFormBrailleCanvasObject *canvas = (ETFormBrailleCanvasObject *)item;
        element->kind = ETFORM_SECTION_ELEMENT_BRAILLE_CANVAS;
        if (braille_canvas_copy(&element->canvas, &canvas->canvas) < 0) {
            PyErr_NoMemory();
            return -1;
        }
        element->canvas_color = canvas->color;
        element->canvas_padding = canvas->padding;
        return 0;
    }
    if (PyObject_TypeCheck(item, &ETFormImageType)) {
        ETFormImageObject *image = (ETFormImageObject *)item;
        element->kind = ETFORM_SECTION_ELEMENT_BRAILLE_CANVAS;
        if (braille_canvas_copy(&element->canvas, &image->canvas) < 0) {
            PyErr_NoMemory();
            return -1;
        }
        element->canvas_color = image->color;
        element->canvas_padding = image->padding;
        return 0;
    }
    element->kind = ETFORM_SECTION_ELEMENT_TEXT;
    if (parse_label_object(item, 1, &element->text) < 0) {
        PyErr_SetString(
            PyExc_TypeError,
            "document items must be str, Text, Table, BrailleCanvas, or Image"
        );
        return -1;
    }
    return 0;
}

static PyObject *Form_new(PyTypeObject *type, PyObject *args, PyObject *kwargs) {
    (void)args;
    (void)kwargs;
    ETFormObject *self = (ETFormObject *)type->tp_alloc(type, 0);
    if (!self) {
        return NULL;
    }
    self->title = NULL;
    self->fields = NULL;
    self->field_count = 0;
    self->field_capacity = 0;
    return (PyObject *)self;
}

static int Form_init(ETFormObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"title", NULL};
    const char *title = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s", kwlist, &title)) {
        return -1;
    }
    free(self->title);
    self->title = etform_strdup(title);
    return self->title ? 0 : -1;
}

static void Form_dealloc(ETFormObject *self) {
    free(self->title);
    for (int i = 0; i < self->field_count; i++) {
        etform_field_free(&self->fields[i]);
    }
    free(self->fields);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *Form_add_text(PyObject *self_obj, PyObject *args, PyObject *kwargs) {
    ETFormObject *self = (ETFormObject *)self_obj;
    static char *kwlist[] = {"key", "label", "default", "placeholder", "description", "field_color", "wrap_text", "tools", "padding", NULL};
    const char *key = NULL;
    PyObject *label_obj = NULL;
    const char *default_value = "";
    PyObject *placeholder_obj = NULL;
    const char *description = "";
    const char *field_color_text = "default";
    PyObject *wrap_text_obj = Py_False;
    PyObject *tools_obj = Py_None;
    PyObject *padding_obj = Py_None;
    ETFormTextColor field_color = ETFORM_TEXT_COLOR_DEFAULT;
    int padding = ETFORM_DEFAULT_PADDING;
    ETFormTextStyle label_style;
    ETFormTextStyle placeholder_style;
    if (!PyArg_ParseTupleAndKeywords(
            args,
            kwargs,
            "sO|sOssOOO",
            kwlist,
            &key,
            &label_obj,
            &default_value,
            &placeholder_obj,
            &description,
            &field_color_text,
            &wrap_text_obj,
            &tools_obj,
            &padding_obj
        )) {
        return NULL;
    }
    if (!key || !*key) {
        PyErr_SetString(PyExc_ValueError, "field key must not be empty");
        return NULL;
    }
    if (parse_text_color(field_color_text, &field_color) < 0) {
        return NULL;
    }
    if (parse_padding(padding_obj, &padding) < 0) {
        return NULL;
    }
    if (parse_label_object(label_obj, 0, &label_style) < 0) {
        return NULL;
    }
    if (placeholder_obj && placeholder_obj != Py_None) {
        if (parse_label_object(placeholder_obj, 1, &placeholder_style) < 0) {
            etform_text_style_free(&label_style);
            return NULL;
        }
    } else {
        etform_text_style_init(&placeholder_style);
        if (etform_text_style_set_value(&placeholder_style, "") < 0) {
            etform_text_style_free(&label_style);
            return NULL;
        }
    }
    int wrap_text = PyObject_IsTrue(wrap_text_obj);
    if (wrap_text < 0) {
        etform_text_style_free(&label_style);
        etform_text_style_free(&placeholder_style);
        return NULL;
    }
    ETFormField *field = append_field(self, key, description, ETFORM_FIELD_TEXT);
    if (!field) {
        etform_text_style_free(&label_style);
        etform_text_style_free(&placeholder_style);
        return NULL;
    }
    field->label = label_style;
    etform_text_style_init(&label_style);
    field->placeholder = placeholder_style;
    etform_text_style_init(&placeholder_style);
    field->field_color = field_color;
    field->wrap_text = wrap_text;
    field->padding = padding;
    if (apply_tools(field, tools_obj) < 0) {
        self->field_count--;
        etform_field_free(field);
        return NULL;
    }
    field->text_value = etform_strdup("");
    field->default_value = etform_strdup(default_value);
    if (!field->text_value || !field->default_value) {
        self->field_count--;
        etform_field_free(field);
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *Form_add_text_section(PyObject *self_obj, PyObject *args, PyObject *kwargs) {
    ETFormObject *self = (ETFormObject *)self_obj;
    static char *kwlist[] = {"text", "padding", NULL};
    PyObject *text_obj = NULL;
    PyObject *padding_obj = Py_None;
    int padding = ETFORM_DEFAULT_PADDING;
    ETFormTextStyle text_style;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|O", kwlist, &text_obj, &padding_obj)) {
        return NULL;
    }
    if (parse_padding(padding_obj, &padding) < 0) {
        return NULL;
    }
    if (parse_label_object(text_obj, 0, &text_style) < 0) {
        return NULL;
    }
    ETFormField *field = append_field(self, "", "", ETFORM_FIELD_TEXT_SECTION);
    if (!field) {
        etform_text_style_free(&text_style);
        return NULL;
    }
    field->label = text_style;
    etform_text_style_init(&text_style);
    field->wrap_text = field->label.wrap_text;
    field->padding = padding;
    Py_RETURN_NONE;
}

static PyObject *Form_add_section(PyObject *self_obj, PyObject *args, PyObject *kwargs) {
    ETFormObject *self = (ETFormObject *)self_obj;
    int boxed = 0;
    int padding = ETFORM_DEFAULT_PADDING;
    if (kwargs) {
        PyObject *key;
        PyObject *value;
        Py_ssize_t pos = 0;
        while (PyDict_Next(kwargs, &pos, &key, &value)) {
            const char *name = PyUnicode_AsUTF8(key);
            if (!name) {
                return NULL;
            }
            if (strcmp(name, "boxed") == 0) {
                int truth = PyObject_IsTrue(value);
                if (truth < 0) {
                    return NULL;
                }
                boxed = truth ? 1 : 0;
            } else if (strcmp(name, "padding") == 0) {
                if (parse_padding(value, &padding) < 0) {
                    return NULL;
                }
            } else {
                PyErr_SetString(PyExc_TypeError, "add_section() got an unexpected keyword argument");
                return NULL;
            }
        }
    }
    Py_ssize_t item_count = PyTuple_GET_SIZE(args);
    if (item_count <= 0) {
        PyErr_SetString(PyExc_ValueError, "section must contain at least one item");
        return NULL;
    }
    ETFormField *field = append_field(self, "", "", ETFORM_FIELD_SECTION);
    if (!field) {
        return NULL;
    }
    field->padding = padding;
    field->section_boxed = boxed;
    for (Py_ssize_t i = 0; i < item_count; i++) {
        if (etform_section_append_object(field, PyTuple_GET_ITEM(args, i)) < 0) {
            self->field_count--;
            etform_field_free(field);
            return NULL;
        }
    }
    Py_RETURN_NONE;
}

static PyObject *Form_add_separator(PyObject *self_obj, PyObject *args, PyObject *kwargs) {
    ETFormObject *self = (ETFormObject *)self_obj;
    static char *kwlist[] = {"label", "color", "padding", NULL};
    PyObject *label_obj = NULL;
    const char *color_text = "default";
    PyObject *padding_obj = Py_None;
    ETFormTextColor separator_color = ETFORM_TEXT_COLOR_DEFAULT;
    int padding = ETFORM_DEFAULT_PADDING;
    ETFormTextStyle label_style;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|OsO", kwlist, &label_obj, &color_text, &padding_obj)) {
        return NULL;
    }
    if (parse_text_color(color_text, &separator_color) < 0) {
        return NULL;
    }
    if (parse_padding(padding_obj, &padding) < 0) {
        return NULL;
    }
    if (!label_obj) {
        label_obj = PyUnicode_FromString("");
        if (!label_obj) {
            return NULL;
        }
        if (parse_label_object(label_obj, 1, &label_style) < 0) {
            Py_DECREF(label_obj);
            return NULL;
        }
        Py_DECREF(label_obj);
    } else if (parse_label_object(label_obj, 1, &label_style) < 0) {
        return NULL;
    }
    ETFormField *field = append_field(self, "", "", ETFORM_FIELD_SEPARATOR);
    if (!field) {
        etform_text_style_free(&label_style);
        return NULL;
    }
    field->label = label_style;
    etform_text_style_init(&label_style);
    field->separator_color = separator_color;
    field->padding = padding;
    Py_RETURN_NONE;
}

static PyObject *Form_add_bool(PyObject *self_obj, PyObject *args, PyObject *kwargs) {
    ETFormObject *self = (ETFormObject *)self_obj;
    static char *kwlist[] = {"key", "label", "default", "description", "field_color", "padding", NULL};
    const char *key = NULL;
    PyObject *label_obj = NULL;
    int default_value = 0;
    const char *description = "";
    const char *field_color_text = "default";
    PyObject *padding_obj = Py_None;
    ETFormTextColor field_color = ETFORM_TEXT_COLOR_DEFAULT;
    int padding = ETFORM_DEFAULT_PADDING;
    ETFormTextStyle label_style;
    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "sO|pssO", kwlist, &key, &label_obj, &default_value, &description, &field_color_text, &padding_obj
        )) {
        return NULL;
    }
    if (!key || !*key) {
        PyErr_SetString(PyExc_ValueError, "field key must not be empty");
        return NULL;
    }
    if (parse_text_color(field_color_text, &field_color) < 0) {
        return NULL;
    }
    if (parse_padding(padding_obj, &padding) < 0) {
        return NULL;
    }
    if (parse_label_object(label_obj, 0, &label_style) < 0) {
        return NULL;
    }
    ETFormField *field = append_field(self, key, description, ETFORM_FIELD_BOOL);
    if (!field) {
        etform_text_style_free(&label_style);
        return NULL;
    }
    field->label = label_style;
    etform_text_style_init(&label_style);
    field->field_color = field_color;
    field->bool_value = default_value ? 1 : 0;
    field->padding = padding;
    Py_RETURN_NONE;
}

static PyObject *Form_add_int(PyObject *self_obj, PyObject *args, PyObject *kwargs) {
    ETFormObject *self = (ETFormObject *)self_obj;
    static char *kwlist[] = {"key", "label", "default", "description", "scrollable", "min", "max", "field_color", "padding", NULL};
    const char *key = NULL;
    PyObject *label_obj = NULL;
    int default_value = 0;
    const char *description = "";
    PyObject *scrollable_obj = Py_False;
    PyObject *min_obj = Py_None;
    PyObject *max_obj = Py_None;
    const char *field_color_text = "default";
    PyObject *padding_obj = Py_None;
    ETFormTextColor field_color = ETFORM_TEXT_COLOR_DEFAULT;
    int padding = ETFORM_DEFAULT_PADDING;
    ETFormTextStyle label_style;
    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "sO|isOOOsO", kwlist, &key, &label_obj, &default_value, &description, &scrollable_obj, &min_obj, &max_obj, &field_color_text, &padding_obj
        )) {
        return NULL;
    }
    if (!key || !*key) {
        PyErr_SetString(PyExc_ValueError, "field key must not be empty");
        return NULL;
    }
    if (parse_text_color(field_color_text, &field_color) < 0) {
        return NULL;
    }
    if (parse_padding(padding_obj, &padding) < 0) {
        return NULL;
    }
    if (parse_label_object(label_obj, 0, &label_style) < 0) {
        return NULL;
    }
    int has_min = min_obj && min_obj != Py_None;
    int has_max = max_obj && max_obj != Py_None;
    int min_value = 0;
    int max_value = 0;
    if (has_min) {
        long parsed = PyLong_AsLong(min_obj);
        if (PyErr_Occurred()) {
            etform_text_style_free(&label_style);
            return NULL;
        }
        if (parsed < INT_MIN || parsed > INT_MAX) {
            etform_text_style_free(&label_style);
            PyErr_SetString(PyExc_OverflowError, "min is outside the C int range");
            return NULL;
        }
        min_value = (int)parsed;
    }
    if (has_max) {
        long parsed = PyLong_AsLong(max_obj);
        if (PyErr_Occurred()) {
            etform_text_style_free(&label_style);
            return NULL;
        }
        if (parsed < INT_MIN || parsed > INT_MAX) {
            etform_text_style_free(&label_style);
            PyErr_SetString(PyExc_OverflowError, "max is outside the C int range");
            return NULL;
        }
        max_value = (int)parsed;
    }
    if (has_min && has_max && min_value > max_value) {
        etform_text_style_free(&label_style);
        PyErr_SetString(PyExc_ValueError, "min must be less than or equal to max");
        return NULL;
    }
    if ((has_min && default_value < min_value) || (has_max && default_value > max_value)) {
        etform_text_style_free(&label_style);
        PyErr_SetString(PyExc_ValueError, "default must be within min and max bounds");
        return NULL;
    }
    int scrollable = PyObject_IsTrue(scrollable_obj);
    if (scrollable < 0) {
        etform_text_style_free(&label_style);
        return NULL;
    }
    ETFormField *field = append_field(self, key, description, ETFORM_FIELD_INT);
    if (!field) {
        etform_text_style_free(&label_style);
        return NULL;
    }
    field->int_value = default_value;
    field->label = label_style;
    etform_text_style_init(&label_style);
    field->field_color = field_color;
    field->scrollable = scrollable;
    field->padding = padding;
    field->has_min = has_min;
    field->min_value = min_value;
    field->has_max = has_max;
    field->max_value = max_value;
    Py_RETURN_NONE;
}

static int choices_from_sequence(ETFormField *field, PyObject *choices, const char *default_value) {
    PyObject *seq = PySequence_Fast(choices, "choices must be a sequence of strings");
    if (!seq) {
        return -1;
    }
    Py_ssize_t count = PySequence_Fast_GET_SIZE(seq);
    if (count <= 0 || count > INT_MAX) {
        Py_DECREF(seq);
        PyErr_SetString(PyExc_ValueError, "choices must contain at least one string");
        return -1;
    }
    field->choices = calloc((size_t)count, sizeof(char *));
    if (!field->choices) {
        Py_DECREF(seq);
        PyErr_NoMemory();
        return -1;
    }
    field->choice_count = (int)count;
    PyObject **items = PySequence_Fast_ITEMS(seq);
    for (Py_ssize_t i = 0; i < count; i++) {
        const char *choice = PyUnicode_AsUTF8(items[i]);
        if (!choice || !*choice) {
            Py_DECREF(seq);
            PyErr_SetString(PyExc_ValueError, "choices must be non-empty strings");
            return -1;
        }
        field->choices[i] = etform_strdup(choice);
        if (!field->choices[i]) {
            Py_DECREF(seq);
            return -1;
        }
    }
    const char *selected = default_value && *default_value ? default_value : field->choices[0];
    int found = 0;
    for (int i = 0; i < field->choice_count; i++) {
        if (strcmp(field->choices[i], selected) == 0) {
            found = 1;
            break;
        }
    }
    if (!found) {
        Py_DECREF(seq);
        PyErr_SetString(PyExc_ValueError, "default must be one of choices");
        return -1;
    }
    field->text_value = etform_strdup(selected);
    Py_DECREF(seq);
    return field->text_value ? 0 : -1;
}

static PyObject *Form_add_choice(PyObject *self_obj, PyObject *args, PyObject *kwargs) {
    ETFormObject *self = (ETFormObject *)self_obj;
    static char *kwlist[] = {"key", "label", "choices", "default", "description", "field_color", "padding", NULL};
    const char *key = NULL;
    PyObject *label_obj = NULL;
    PyObject *choices = NULL;
    const char *default_value = NULL;
    const char *description = "";
    const char *field_color_text = "default";
    PyObject *padding_obj = Py_None;
    ETFormTextColor field_color = ETFORM_TEXT_COLOR_DEFAULT;
    int padding = ETFORM_DEFAULT_PADDING;
    ETFormTextStyle label_style;
    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "sOO|zssO", kwlist, &key, &label_obj, &choices, &default_value, &description, &field_color_text, &padding_obj
        )) {
        return NULL;
    }
    if (!key || !*key) {
        PyErr_SetString(PyExc_ValueError, "field key must not be empty");
        return NULL;
    }
    if (parse_text_color(field_color_text, &field_color) < 0) {
        return NULL;
    }
    if (parse_padding(padding_obj, &padding) < 0) {
        return NULL;
    }
    if (parse_label_object(label_obj, 0, &label_style) < 0) {
        return NULL;
    }
    ETFormField *field = append_field(self, key, description, ETFORM_FIELD_CHOICE);
    if (!field) {
        etform_text_style_free(&label_style);
        return NULL;
    }
    field->label = label_style;
    etform_text_style_init(&label_style);
    field->field_color = field_color;
    field->padding = padding;
    if (choices_from_sequence(field, choices, default_value) < 0) {
        self->field_count--;
        etform_field_free(field);
        return NULL;
    }
    Py_RETURN_NONE;
}

static int checkbox_default_contains(PyObject *seq, const char *value) {
    if (!seq || !value) {
        return 0;
    }
    Py_ssize_t count = PySequence_Fast_GET_SIZE(seq);
    PyObject **items = PySequence_Fast_ITEMS(seq);
    for (Py_ssize_t i = 0; i < count; i++) {
        const char *item = PyUnicode_AsUTF8(items[i]);
        if (item && strcmp(item, value) == 0) {
            return 1;
        }
    }
    return 0;
}

static int checkbox_options_from_sequence(ETFormField *field, PyObject *options, PyObject *default_value) {
    PyObject *seq = PySequence_Fast(options, "options must be a sequence of str or Text");
    if (!seq) {
        return -1;
    }
    Py_ssize_t count = PySequence_Fast_GET_SIZE(seq);
    if (count <= 0 || count > INT_MAX) {
        Py_DECREF(seq);
        PyErr_SetString(PyExc_ValueError, "options must contain at least one item");
        return -1;
    }
    PyObject *defaults = NULL;
    if (default_value && default_value != Py_None) {
        defaults = PySequence_Fast(default_value, "default must be a sequence of option labels");
        if (!defaults) {
            Py_DECREF(seq);
            return -1;
        }
        Py_ssize_t default_count = PySequence_Fast_GET_SIZE(defaults);
        PyObject **default_items = PySequence_Fast_ITEMS(defaults);
        for (Py_ssize_t i = 0; i < default_count; i++) {
            if (!PyUnicode_AsUTF8(default_items[i])) {
                Py_DECREF(defaults);
                Py_DECREF(seq);
                PyErr_SetString(PyExc_TypeError, "default must contain strings");
                return -1;
            }
        }
    }
    field->checkbox_options = calloc((size_t)count, sizeof(ETFormTextStyle));
    field->checkbox_checked = calloc((size_t)count, sizeof(int));
    if (!field->checkbox_options || !field->checkbox_checked) {
        Py_XDECREF(defaults);
        Py_DECREF(seq);
        PyErr_NoMemory();
        return -1;
    }
    field->checkbox_count = (int)count;
    PyObject **items = PySequence_Fast_ITEMS(seq);
    for (Py_ssize_t i = 0; i < count; i++) {
        if (parse_label_object(items[i], 0, &field->checkbox_options[i]) < 0) {
            Py_XDECREF(defaults);
            Py_DECREF(seq);
            return -1;
        }
        for (Py_ssize_t j = 0; j < i; j++) {
            if (strcmp(field->checkbox_options[j].value, field->checkbox_options[i].value) == 0) {
                Py_XDECREF(defaults);
                Py_DECREF(seq);
                PyErr_SetString(PyExc_ValueError, "checkbox option labels must be unique");
                return -1;
            }
        }
        field->checkbox_checked[i] = checkbox_default_contains(defaults, field->checkbox_options[i].value);
    }
    if (defaults) {
        Py_ssize_t default_count = PySequence_Fast_GET_SIZE(defaults);
        PyObject **default_items = PySequence_Fast_ITEMS(defaults);
        for (Py_ssize_t i = 0; i < default_count; i++) {
            const char *default_label = PyUnicode_AsUTF8(default_items[i]);
            int found = 0;
            if (!default_label) {
                Py_DECREF(defaults);
                Py_DECREF(seq);
                return -1;
            }
            for (int j = 0; j < field->checkbox_count; j++) {
                if (strcmp(field->checkbox_options[j].value, default_label) == 0) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                Py_DECREF(defaults);
                Py_DECREF(seq);
                PyErr_SetString(PyExc_ValueError, "default entries must match checkbox options");
                return -1;
            }
        }
        Py_DECREF(defaults);
    }
    Py_DECREF(seq);
    return 0;
}

static PyObject *Form_add_checkbox(PyObject *self_obj, PyObject *args, PyObject *kwargs) {
    ETFormObject *self = (ETFormObject *)self_obj;
    static char *kwlist[] = {"key", "label", "options", "default", "description", "field_color", "tick", "padding", NULL};
    const char *key = NULL;
    PyObject *label_obj = NULL;
    PyObject *options = NULL;
    PyObject *default_value = Py_None;
    const char *description = "";
    const char *field_color_text = "default";
    const char *tick_text = "x";
    PyObject *padding_obj = Py_None;
    ETFormTextColor field_color = ETFORM_TEXT_COLOR_DEFAULT;
    ETFormCheckboxTick checkbox_tick = ETFORM_CHECKBOX_TICK_X;
    int padding = ETFORM_DEFAULT_PADDING;
    ETFormTextStyle label_style;
    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "sOO|OsssO", kwlist, &key, &label_obj, &options, &default_value, &description, &field_color_text, &tick_text, &padding_obj
        )) {
        return NULL;
    }
    if (!key || !*key) {
        PyErr_SetString(PyExc_ValueError, "field key must not be empty");
        return NULL;
    }
    if (parse_text_color(field_color_text, &field_color) < 0) {
        return NULL;
    }
    if (parse_checkbox_tick(tick_text, &checkbox_tick) < 0) {
        return NULL;
    }
    if (parse_padding(padding_obj, &padding) < 0) {
        return NULL;
    }
    if (parse_label_object(label_obj, 0, &label_style) < 0) {
        return NULL;
    }
    ETFormField *field = append_field(self, key, description, ETFORM_FIELD_CHECKBOX);
    if (!field) {
        etform_text_style_free(&label_style);
        return NULL;
    }
    field->label = label_style;
    etform_text_style_init(&label_style);
    field->field_color = field_color;
    field->checkbox_tick = checkbox_tick;
    field->padding = padding;
    if (checkbox_options_from_sequence(field, options, default_value) < 0) {
        self->field_count--;
        etform_field_free(field);
        return NULL;
    }
    Py_RETURN_NONE;
}

static int list_from_sequence(ETFormField *field, PyObject *default_value) {
    if (!default_value || default_value == Py_None) {
        return 0;
    }
    PyObject *seq = PySequence_Fast(default_value, "default must be a sequence of str or Text");
    if (!seq) {
        return -1;
    }
    Py_ssize_t count = PySequence_Fast_GET_SIZE(seq);
    PyObject **items = PySequence_Fast_ITEMS(seq);
    for (Py_ssize_t i = 0; i < count; i++) {
        ETFormTextStyle style;
        if (parse_label_object(items[i], 1, &style) < 0) {
            Py_DECREF(seq);
            return -1;
        }
        char *copy = etform_strdup(style.value ? style.value : "");
        if (!copy) {
            etform_text_style_free(&style);
            Py_DECREF(seq);
            return -1;
        }
        trim_in_place(copy);
        if (*copy) {
            if (etform_text_style_set_value(&style, copy) < 0 || etform_list_append_value(field, copy, &style) < 0) {
                free(copy);
                etform_text_style_free(&style);
                Py_DECREF(seq);
                PyErr_NoMemory();
                return -1;
            }
        }
        etform_text_style_free(&style);
        free(copy);
    }
    Py_DECREF(seq);
    return 0;
}

static PyObject *Form_add_list(PyObject *self_obj, PyObject *args, PyObject *kwargs) {
    ETFormObject *self = (ETFormObject *)self_obj;
    static char *kwlist[] = {"key", "label", "default", "description", "field_color", "tools", "padding", NULL};
    const char *key = NULL;
    PyObject *label_obj = NULL;
    PyObject *default_value = Py_None;
    const char *description = "";
    const char *field_color_text = "default";
    PyObject *tools_obj = Py_None;
    PyObject *padding_obj = Py_None;
    ETFormTextColor field_color = ETFORM_TEXT_COLOR_DEFAULT;
    int padding = ETFORM_DEFAULT_PADDING;
    ETFormTextStyle label_style;
    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "sO|OssOO", kwlist, &key, &label_obj, &default_value, &description, &field_color_text, &tools_obj, &padding_obj
        )) {
        return NULL;
    }
    if (!key || !*key) {
        PyErr_SetString(PyExc_ValueError, "field key must not be empty");
        return NULL;
    }
    if (parse_text_color(field_color_text, &field_color) < 0) {
        return NULL;
    }
    if (parse_padding(padding_obj, &padding) < 0) {
        return NULL;
    }
    if (parse_label_object(label_obj, 0, &label_style) < 0) {
        return NULL;
    }
    ETFormField *field = append_field(self, key, description, ETFORM_FIELD_LIST);
    if (!field) {
        etform_text_style_free(&label_style);
        return NULL;
    }
    field->label = label_style;
    etform_text_style_init(&label_style);
    field->field_color = field_color;
    field->padding = padding;
    if (apply_tools(field, tools_obj) < 0) {
        self->field_count--;
        etform_field_free(field);
        return NULL;
    }
    if (list_from_sequence(field, default_value) < 0) {
        self->field_count--;
        etform_field_free(field);
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *Form_run(PyObject *self_obj, PyObject *Py_UNUSED(ignored)) {
    ETFormObject *self = (ETFormObject *)self_obj;
    if (self->field_count <= 0) {
        PyErr_SetString(PyExc_ValueError, "form must contain at least one field");
        return NULL;
    }
    int result = etform_run_form(self);
    if (result < 0) {
        PyErr_SetString(PyExc_RuntimeError, "could not run form");
        return NULL;
    }
    if (result == 0) {
        Py_RETURN_NONE;
    }
    return etform_build_result(self);
}

PyObject *etform_build_result(ETFormObject *self) {
    PyObject *dict = PyDict_New();
    if (!dict) {
        return NULL;
    }
    for (int i = 0; i < self->field_count; i++) {
        ETFormField *field = &self->fields[i];
        PyObject *value = NULL;
        switch (field->kind) {
            case ETFORM_FIELD_TEXT:
                value = PyUnicode_FromString(
                    field->text_value && *field->text_value
                        ? field->text_value
                        : field->default_value ? field->default_value : ""
                );
                break;
            case ETFORM_FIELD_CHOICE:
                value = PyUnicode_FromString(field->text_value ? field->text_value : "");
                break;
            case ETFORM_FIELD_BOOL:
                value = PyBool_FromLong(field->bool_value);
                break;
            case ETFORM_FIELD_INT:
                value = PyLong_FromLong(field->int_value);
                break;
            case ETFORM_FIELD_LIST:
                value = PyList_New(field->list_value.count);
                if (value) {
                    for (int j = 0; j < field->list_value.count; j++) {
                        PyObject *item = PyUnicode_FromString(field->list_value.items[j]);
                        if (!item) {
                            Py_DECREF(value);
                            value = NULL;
                            break;
                        }
                        PyList_SET_ITEM(value, j, item);
                    }
                }
                break;
            case ETFORM_FIELD_CHECKBOX:
                value = PyDict_New();
                if (value) {
                    for (int j = 0; j < field->checkbox_count; j++) {
                        PyObject *checked = PyBool_FromLong(field->checkbox_checked[j]);
                        if (!checked || PyDict_SetItemString(value, field->checkbox_options[j].value, checked) < 0) {
                            Py_XDECREF(checked);
                            Py_DECREF(value);
                            value = NULL;
                            break;
                        }
                        Py_DECREF(checked);
                    }
                }
                break;
            case ETFORM_FIELD_TEXT_SECTION:
            case ETFORM_FIELD_SEPARATOR:
            case ETFORM_FIELD_SECTION:
                continue;
        }
        if (!value || PyDict_SetItemString(dict, field->key, value) < 0) {
            Py_XDECREF(value);
            Py_DECREF(dict);
            return NULL;
        }
        Py_DECREF(value);
    }
    return dict;
}

static PyMethodDef Form_methods[] = {
    {"add_text_input", (PyCFunction)Form_add_text, METH_VARARGS | METH_KEYWORDS, "Add a text input field."},
    {"add_section", (PyCFunction)Form_add_section, METH_VARARGS | METH_KEYWORDS, "Add a static section."},
    {"add_text_section", (PyCFunction)Form_add_text_section, METH_VARARGS | METH_KEYWORDS, "Add static text to the form."},
    {"add_separator", (PyCFunction)Form_add_separator, METH_VARARGS | METH_KEYWORDS, "Add a visual separator."},
    {"add_bool_input", (PyCFunction)Form_add_bool, METH_VARARGS | METH_KEYWORDS, "Add a boolean input field."},
    {"add_choice_input", (PyCFunction)Form_add_choice, METH_VARARGS | METH_KEYWORDS, "Add a choice input field."},
    {"add_checkbox_input", (PyCFunction)Form_add_checkbox, METH_VARARGS | METH_KEYWORDS, "Add a checkbox group input field."},
    {"add_int_input", (PyCFunction)Form_add_int, METH_VARARGS | METH_KEYWORDS, "Add an integer input field."},
    {"add_list_input", (PyCFunction)Form_add_list, METH_VARARGS | METH_KEYWORDS, "Add a list input field."},
    {"run", (PyCFunction)Form_run, METH_NOARGS, "Run the native ncurses form."},
    {NULL, NULL, 0, NULL},
};

PyTypeObject ETFormType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "etform.Form",
    .tp_basicsize = sizeof(ETFormObject),
    .tp_itemsize = 0,
    .tp_dealloc = (destructor)Form_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Native ETForm-style ncurses form.",
    .tp_methods = Form_methods,
    .tp_init = (initproc)Form_init,
    .tp_new = Form_new,
};
