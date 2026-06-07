#include "form_object.h"

#include "dialogs.h"
#include "form_runtime.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static char *tform_strdup(const char *value) {
    char *copy = strdup(value ? value : "");
    if (!copy) {
        PyErr_NoMemory();
    }
    return copy;
}

static const char *align_name(TFormLabelAlign align) {
    switch (align) {
        case TFORM_LABEL_ALIGN_CENTER:
            return "center";
        case TFORM_LABEL_ALIGN_RIGHT:
            return "right";
        case TFORM_LABEL_ALIGN_LEFT:
        default:
            return "left";
    }
}

static const char *color_name(TFormTextColor color) {
    switch (color) {
        case TFORM_TEXT_COLOR_WHITE:
            return "white";
        case TFORM_TEXT_COLOR_CYAN:
            return "cyan";
        case TFORM_TEXT_COLOR_YELLOW:
            return "yellow";
        case TFORM_TEXT_COLOR_GREEN:
            return "green";
        case TFORM_TEXT_COLOR_BLUE:
            return "blue";
        case TFORM_TEXT_COLOR_MAGENTA:
            return "magenta";
        case TFORM_TEXT_COLOR_RED:
            return "red";
        case TFORM_TEXT_COLOR_DEFAULT:
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

void tform_field_free(TFormField *field) {
    if (!field) {
        return;
    }
    free(field->key);
    tform_text_style_free(&field->label);
    free(field->description);
    free(field->file_picker_start_dir);
    free(field->text_value);
    for (int i = 0; i < field->choice_count; i++) {
        free(field->choices[i]);
    }
    free(field->choices);
    for (int i = 0; i < field->list_value.count; i++) {
        tform_text_style_free(&field->list_styles[i]);
    }
    free(field->list_styles);
    tform_string_list_free(&field->list_value);
    for (int i = 0; i < field->checkbox_count; i++) {
        tform_text_style_free(&field->checkbox_options[i]);
    }
    free(field->checkbox_options);
    free(field->checkbox_checked);
    memset(field, 0, sizeof(*field));
}

void tform_text_style_init(TFormTextStyle *style) {
    if (!style) {
        return;
    }
    style->value = NULL;
    style->align = TFORM_LABEL_ALIGN_LEFT;
    style->bold = 0;
    style->color = TFORM_TEXT_COLOR_DEFAULT;
    style->wrap_text = 0;
}

void tform_text_style_free(TFormTextStyle *style) {
    if (!style) {
        return;
    }
    free(style->value);
    tform_text_style_init(style);
}

static int tform_text_style_set_value(TFormTextStyle *style, const char *value) {
    if (!style) {
        return -1;
    }
    char *copy = tform_strdup(value);
    if (!copy) {
        return -1;
    }
    free(style->value);
    style->value = copy;
    return 0;
}

static int tform_text_style_copy(TFormTextStyle *dest, const TFormTextStyle *src, const char *fallback_value) {
    if (!dest) {
        return -1;
    }
    tform_text_style_init(dest);
    if (tform_text_style_set_value(dest, src && src->value ? src->value : fallback_value ? fallback_value : "") < 0) {
        return -1;
    }
    if (src) {
        dest->align = src->align;
        dest->bold = src->bold;
        dest->color = src->color;
        dest->wrap_text = src->wrap_text;
    }
    return 0;
}

static int tform_list_styles_ensure_capacity(TFormField *field, int needed) {
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
    TFormTextStyle *next = realloc(field->list_styles, (size_t)next_capacity * sizeof(TFormTextStyle));
    if (!next) {
        PyErr_NoMemory();
        return -1;
    }
    for (int i = field->list_style_capacity; i < next_capacity; i++) {
        tform_text_style_init(&next[i]);
    }
    field->list_styles = next;
    field->list_style_capacity = next_capacity;
    return 0;
}

int tform_list_append_value(TFormField *field, const char *value, const TFormTextStyle *style) {
    if (!field || !value || tform_list_styles_ensure_capacity(field, field->list_value.count + 1) < 0) {
        return -1;
    }
    if (tform_string_list_append(&field->list_value, value) < 0) {
        return -1;
    }
    TFormTextStyle row_style;
    if (tform_text_style_copy(&row_style, style, value) < 0) {
        tform_string_list_delete(&field->list_value, field->list_value.count - 1);
        return -1;
    }
    field->list_styles[field->list_value.count - 1] = row_style;
    return 0;
}

int tform_list_set_value(TFormField *field, int index, const char *value, const TFormTextStyle *style) {
    if (!field || index < 0 || index >= field->list_value.count || !value) {
        return -1;
    }
    if (tform_string_list_set(&field->list_value, index, value) < 0) {
        return -1;
    }
    if (style) {
        TFormTextStyle next_style;
        if (tform_text_style_copy(&next_style, style, value) < 0) {
            return -1;
        }
        tform_text_style_free(&field->list_styles[index]);
        field->list_styles[index] = next_style;
    } else if (field->list_styles && index < field->list_style_capacity) {
        if (tform_text_style_set_value(&field->list_styles[index], value) < 0) {
            return -1;
        }
    }
    return 0;
}

int tform_list_delete_value(TFormField *field, int index) {
    if (!field || index < 0 || index >= field->list_value.count) {
        return -1;
    }
    tform_text_style_free(&field->list_styles[index]);
    if (tform_string_list_delete(&field->list_value, index) < 0) {
        return -1;
    }
    for (int i = index; i < field->list_value.count; i++) {
        field->list_styles[i] = field->list_styles[i + 1];
    }
    if (field->list_styles && field->list_value.count < field->list_style_capacity) {
        tform_text_style_init(&field->list_styles[field->list_value.count]);
    }
    return 0;
}

static int ensure_field_capacity(TFormObject *self) {
    if (self->field_count < self->field_capacity) {
        return 0;
    }
    int next_capacity = self->field_capacity ? self->field_capacity * 2 : 8;
    TFormField *next = realloc(self->fields, (size_t)next_capacity * sizeof(TFormField));
    if (!next) {
        PyErr_NoMemory();
        return -1;
    }
    memset(next + self->field_capacity, 0, (size_t)(next_capacity - self->field_capacity) * sizeof(TFormField));
    self->fields = next;
    self->field_capacity = next_capacity;
    return 0;
}

static TFormField *append_field(
    TFormObject *self,
    const char *key,
    const char *description,
    TFormFieldKind kind
) {
    if (ensure_field_capacity(self) < 0) {
        return NULL;
    }
    TFormField *field = &self->fields[self->field_count];
    memset(field, 0, sizeof(*field));
    tform_text_style_init(&field->label);
    tform_string_list_init(&field->list_value);
    field->padding = TFORM_DEFAULT_PADDING;
    field->key = tform_strdup(key);
    field->description = tform_strdup(description ? description : "");
    field->kind = kind;
    if (!field->key || !field->description) {
        tform_field_free(field);
        return NULL;
    }
    self->field_count++;
    return field;
}

static int parse_label_align(const char *value, TFormLabelAlign *align) {
    if (!value || !align) {
        PyErr_SetString(PyExc_ValueError, "align must be 'left', 'center', or 'right'");
        return -1;
    }
    if (strcmp(value, "left") == 0) {
        *align = TFORM_LABEL_ALIGN_LEFT;
        return 0;
    }
    if (strcmp(value, "center") == 0) {
        *align = TFORM_LABEL_ALIGN_CENTER;
        return 0;
    }
    if (strcmp(value, "right") == 0) {
        *align = TFORM_LABEL_ALIGN_RIGHT;
        return 0;
    }
    PyErr_SetString(PyExc_ValueError, "align must be 'left', 'center', or 'right'");
    return -1;
}

static int parse_padding(PyObject *value, int *padding) {
    if (!padding) {
        return -1;
    }
    *padding = TFORM_DEFAULT_PADDING;
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

static int parse_text_color(const char *value, TFormTextColor *color) {
    if (!value || !color) {
        PyErr_SetString(PyExc_ValueError, "color must be one of default, white, cyan, yellow, green, blue, magenta, or red");
        return -1;
    }
    if (strcmp(value, "default") == 0) {
        *color = TFORM_TEXT_COLOR_DEFAULT;
        return 0;
    }
    if (strcmp(value, "white") == 0) {
        *color = TFORM_TEXT_COLOR_WHITE;
        return 0;
    }
    if (strcmp(value, "cyan") == 0) {
        *color = TFORM_TEXT_COLOR_CYAN;
        return 0;
    }
    if (strcmp(value, "yellow") == 0) {
        *color = TFORM_TEXT_COLOR_YELLOW;
        return 0;
    }
    if (strcmp(value, "green") == 0) {
        *color = TFORM_TEXT_COLOR_GREEN;
        return 0;
    }
    if (strcmp(value, "blue") == 0) {
        *color = TFORM_TEXT_COLOR_BLUE;
        return 0;
    }
    if (strcmp(value, "magenta") == 0) {
        *color = TFORM_TEXT_COLOR_MAGENTA;
        return 0;
    }
    if (strcmp(value, "red") == 0) {
        *color = TFORM_TEXT_COLOR_RED;
        return 0;
    }
    PyErr_SetString(PyExc_ValueError, "color must be one of default, white, cyan, yellow, green, blue, magenta, or red");
    return -1;
}

static int parse_checkbox_tick(const char *value, TFormCheckboxTick *tick) {
    if (!value || !tick) {
        PyErr_SetString(PyExc_ValueError, "tick must be 'x', 'block', or 'dot'");
        return -1;
    }
    if (strcmp(value, "x") == 0) {
        *tick = TFORM_CHECKBOX_TICK_X;
        return 0;
    }
    if (strcmp(value, "block") == 0) {
        *tick = TFORM_CHECKBOX_TICK_BLOCK;
        return 0;
    }
    if (strcmp(value, "dot") == 0) {
        *tick = TFORM_CHECKBOX_TICK_DOT;
        return 0;
    }
    PyErr_SetString(PyExc_ValueError, "tick must be 'x', 'block', or 'dot'");
    return -1;
}

typedef struct {
    PyObject_HEAD
    TFormTextStyle style;
} TFormTextObject;

typedef struct {
    PyObject_HEAD
    int mode;
    char *start_dir;
} TFormFilePickerObject;

static PyObject *Text_new(PyTypeObject *type, PyObject *args, PyObject *kwargs) {
    (void)args;
    (void)kwargs;
    TFormTextObject *self = (TFormTextObject *)type->tp_alloc(type, 0);
    if (!self) {
        return NULL;
    }
    tform_text_style_init(&self->style);
    return (PyObject *)self;
}

static int Text_init(TFormTextObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"value", "align", "bold", "color", "wrap_text", NULL};
    const char *value = NULL;
    const char *align_text = "left";
    int bold = 0;
    const char *color_text = "default";
    int wrap_text = 0;
    TFormLabelAlign align = TFORM_LABEL_ALIGN_LEFT;
    TFormTextColor color = TFORM_TEXT_COLOR_DEFAULT;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|spsp", kwlist, &value, &align_text, &bold, &color_text, &wrap_text)) {
        return -1;
    }
    if (parse_label_align(align_text, &align) < 0 || parse_text_color(color_text, &color) < 0) {
        return -1;
    }
    if (tform_text_style_set_value(&self->style, value) < 0) {
        return -1;
    }
    self->style.align = align;
    self->style.bold = bold ? 1 : 0;
    self->style.color = color;
    self->style.wrap_text = wrap_text ? 1 : 0;
    return 0;
}

static void Text_dealloc(TFormTextObject *self) {
    tform_text_style_free(&self->style);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *Text_repr(TFormTextObject *self) {
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
        "Text(%s, align='%s', bold=%s, color='%s', wrap_text=%s)",
        repr_text,
        align_name(self->style.align),
        self->style.bold ? "True" : "False",
        color_name(self->style.color),
        self->style.wrap_text ? "True" : "False"
    );
    Py_DECREF(value_repr);
    return repr;
}

static PyObject *Text_get_value(TFormTextObject *self, void *closure) {
    (void)closure;
    return PyUnicode_FromString(self->style.value ? self->style.value : "");
}

static PyObject *Text_get_align(TFormTextObject *self, void *closure) {
    (void)closure;
    return PyUnicode_FromString(align_name(self->style.align));
}

static PyObject *Text_get_bold(TFormTextObject *self, void *closure) {
    (void)closure;
    return PyBool_FromLong(self->style.bold);
}

static PyObject *Text_get_color(TFormTextObject *self, void *closure) {
    (void)closure;
    return PyUnicode_FromString(color_name(self->style.color));
}

static PyObject *Text_get_wrap_text(TFormTextObject *self, void *closure) {
    (void)closure;
    return PyBool_FromLong(self->style.wrap_text);
}

static PyGetSetDef Text_getset[] = {
    {"value", (getter)Text_get_value, NULL, "Text value.", NULL},
    {"align", (getter)Text_get_align, NULL, "Text alignment.", NULL},
    {"bold", (getter)Text_get_bold, NULL, "Whether the text is bold.", NULL},
    {"color", (getter)Text_get_color, NULL, "Text color.", NULL},
    {"wrap_text", (getter)Text_get_wrap_text, NULL, "Whether the text wraps.", NULL},
    {NULL, NULL, NULL, NULL, NULL},
};

PyTypeObject TFormTextType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "tform.Text",
    .tp_basicsize = sizeof(TFormTextObject),
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
        *mode = TFORM_FILE_PICKER_FILE;
        return 0;
    }
    if (strcmp(value, "directory") == 0 || strcmp(value, "dir") == 0) {
        *mode = TFORM_FILE_PICKER_DIRECTORY;
        return 0;
    }
    PyErr_SetString(PyExc_ValueError, "mode must be 'file' or 'directory'");
    return -1;
}

static const char *file_picker_mode_name(int mode) {
    return mode == TFORM_FILE_PICKER_DIRECTORY ? "directory" : "file";
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
    char *copy = tform_strdup(value);
    return copy;
}

static int FilePicker_init(TFormFilePickerObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"mode", "start_dir", NULL};
    const char *mode_text = "file";
    const char *start_dir = ".";
    int mode = TFORM_FILE_PICKER_FILE;
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

static void FilePicker_dealloc(TFormFilePickerObject *self) {
    free(self->start_dir);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *FilePicker_repr(TFormFilePickerObject *self) {
    return PyUnicode_FromFormat(
        "FilePicker(mode='%s', start_dir='%s')",
        file_picker_mode_name(self->mode),
        self->start_dir ? self->start_dir : "."
    );
}

static PyObject *FilePicker_get_mode(TFormFilePickerObject *self, void *closure) {
    (void)closure;
    return PyUnicode_FromString(file_picker_mode_name(self->mode));
}

static PyObject *FilePicker_get_start_dir(TFormFilePickerObject *self, void *closure) {
    (void)closure;
    return PyUnicode_FromString(self->start_dir ? self->start_dir : ".");
}

static PyGetSetDef FilePicker_getset[] = {
    {"mode", (getter)FilePicker_get_mode, NULL, "Picker mode.", NULL},
    {"start_dir", (getter)FilePicker_get_start_dir, NULL, "Initial picker directory.", NULL},
    {NULL, NULL, NULL, NULL, NULL},
};

PyTypeObject TFormFilePickerType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "tform.FilePicker",
    .tp_basicsize = sizeof(TFormFilePickerObject),
    .tp_itemsize = 0,
    .tp_dealloc = (destructor)FilePicker_dealloc,
    .tp_repr = (reprfunc)FilePicker_repr,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Tool that opens a file or directory picker for compatible fields.",
    .tp_getset = FilePicker_getset,
    .tp_init = (initproc)FilePicker_init,
    .tp_new = PyType_GenericNew,
};

static int apply_tool(TFormField *field, PyObject *tool) {
    if (PyObject_TypeCheck(tool, &TFormFilePickerType)) {
        TFormFilePickerObject *picker = (TFormFilePickerObject *)tool;
        char *start_dir = tform_strdup(picker->start_dir ? picker->start_dir : ".");
        if (!start_dir) {
            return -1;
        }
        free(field->file_picker_start_dir);
        field->has_file_picker = 1;
        field->file_picker_mode = picker->mode;
        field->file_picker_start_dir = start_dir;
        return 0;
    }
    PyErr_SetString(PyExc_TypeError, "tools must contain FilePicker instances");
    return -1;
}

static int apply_tools(TFormField *field, PyObject *tools) {
    if (!tools || tools == Py_None) {
        return 0;
    }
    if (PyObject_TypeCheck(tools, &TFormFilePickerType)) {
        return apply_tool(field, tools);
    }
    PyObject *seq = PySequence_Fast(tools, "tools must be a FilePicker or a sequence of tools");
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
    TFormTextStyle *style
) {
    if (!style) {
        return -1;
    }
    tform_text_style_init(style);
    if (PyObject_TypeCheck(label_obj, &TFormTextType)) {
        TFormTextObject *text = (TFormTextObject *)label_obj;
        if (!allow_empty && (!text->style.value || !*text->style.value)) {
            PyErr_SetString(PyExc_ValueError, "field label must not be empty");
            return -1;
        }
        if (tform_text_style_set_value(style, text->style.value ? text->style.value : "") < 0) {
            return -1;
        }
        style->align = text->style.align;
        style->bold = text->style.bold;
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
    if (tform_text_style_set_value(style, label) < 0) {
        return -1;
    }
    style->align = TFORM_LABEL_ALIGN_LEFT;
    style->bold = 0;
    style->color = TFORM_TEXT_COLOR_DEFAULT;
    style->wrap_text = 0;
    return 0;
}

static PyObject *Form_new(PyTypeObject *type, PyObject *args, PyObject *kwargs) {
    (void)args;
    (void)kwargs;
    TFormObject *self = (TFormObject *)type->tp_alloc(type, 0);
    if (!self) {
        return NULL;
    }
    self->title = NULL;
    self->fields = NULL;
    self->field_count = 0;
    self->field_capacity = 0;
    return (PyObject *)self;
}

static int Form_init(TFormObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"title", NULL};
    const char *title = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s", kwlist, &title)) {
        return -1;
    }
    free(self->title);
    self->title = tform_strdup(title);
    return self->title ? 0 : -1;
}

static void Form_dealloc(TFormObject *self) {
    free(self->title);
    for (int i = 0; i < self->field_count; i++) {
        tform_field_free(&self->fields[i]);
    }
    free(self->fields);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *Form_add_text_common(TFormObject *self, PyObject *args, PyObject *kwargs, TFormFieldKind kind) {
    static char *kwlist[] = {"key", "label", "default", "description", "field_color", "wrap_text", "tools", "padding", NULL};
    const char *key = NULL;
    PyObject *label_obj = NULL;
    const char *default_value = "";
    const char *description = "";
    const char *field_color_text = "default";
    PyObject *wrap_text_obj = Py_False;
    PyObject *tools_obj = Py_None;
    PyObject *padding_obj = Py_None;
    TFormTextColor field_color = TFORM_TEXT_COLOR_DEFAULT;
    int padding = TFORM_DEFAULT_PADDING;
    TFormTextStyle label_style;
    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "sO|sssOOO", kwlist, &key, &label_obj, &default_value, &description, &field_color_text, &wrap_text_obj, &tools_obj, &padding_obj
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
    int wrap_text = PyObject_IsTrue(wrap_text_obj);
    if (wrap_text < 0) {
        tform_text_style_free(&label_style);
        return NULL;
    }
    TFormField *field = append_field(self, key, description, kind);
    if (!field) {
        tform_text_style_free(&label_style);
        return NULL;
    }
    field->label = label_style;
    tform_text_style_init(&label_style);
    field->field_color = field_color;
    field->wrap_text = wrap_text;
    field->padding = padding;
    if (apply_tools(field, tools_obj) < 0) {
        self->field_count--;
        tform_field_free(field);
        return NULL;
    }
    field->text_value = tform_strdup(default_value);
    if (!field->text_value) {
        self->field_count--;
        tform_field_free(field);
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *Form_add_text(PyObject *self, PyObject *args, PyObject *kwargs) {
    return Form_add_text_common((TFormObject *)self, args, kwargs, TFORM_FIELD_TEXT);
}

static PyObject *Form_add_optional_text(PyObject *self, PyObject *args, PyObject *kwargs) {
    return Form_add_text_common((TFormObject *)self, args, kwargs, TFORM_FIELD_OPTIONAL_TEXT);
}

static PyObject *Form_add_text_section(PyObject *self_obj, PyObject *args, PyObject *kwargs) {
    TFormObject *self = (TFormObject *)self_obj;
    static char *kwlist[] = {"text", "padding", NULL};
    PyObject *text_obj = NULL;
    PyObject *padding_obj = Py_None;
    int padding = TFORM_DEFAULT_PADDING;
    TFormTextStyle text_style;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|O", kwlist, &text_obj, &padding_obj)) {
        return NULL;
    }
    if (parse_padding(padding_obj, &padding) < 0) {
        return NULL;
    }
    if (parse_label_object(text_obj, 0, &text_style) < 0) {
        return NULL;
    }
    TFormField *field = append_field(self, "", "", TFORM_FIELD_TEXT_SECTION);
    if (!field) {
        tform_text_style_free(&text_style);
        return NULL;
    }
    field->label = text_style;
    tform_text_style_init(&text_style);
    field->wrap_text = field->label.wrap_text;
    field->padding = padding;
    Py_RETURN_NONE;
}

static PyObject *Form_add_separator(PyObject *self_obj, PyObject *args, PyObject *kwargs) {
    TFormObject *self = (TFormObject *)self_obj;
    static char *kwlist[] = {"label", "color", "padding", NULL};
    PyObject *label_obj = NULL;
    const char *color_text = "default";
    PyObject *padding_obj = Py_None;
    TFormTextColor separator_color = TFORM_TEXT_COLOR_DEFAULT;
    int padding = TFORM_DEFAULT_PADDING;
    TFormTextStyle label_style;
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
    TFormField *field = append_field(self, "", "", TFORM_FIELD_SEPARATOR);
    if (!field) {
        tform_text_style_free(&label_style);
        return NULL;
    }
    field->label = label_style;
    tform_text_style_init(&label_style);
    field->separator_color = separator_color;
    field->padding = padding;
    Py_RETURN_NONE;
}

static PyObject *Form_add_bool(PyObject *self_obj, PyObject *args, PyObject *kwargs) {
    TFormObject *self = (TFormObject *)self_obj;
    static char *kwlist[] = {"key", "label", "default", "description", "field_color", "padding", NULL};
    const char *key = NULL;
    PyObject *label_obj = NULL;
    int default_value = 0;
    const char *description = "";
    const char *field_color_text = "default";
    PyObject *padding_obj = Py_None;
    TFormTextColor field_color = TFORM_TEXT_COLOR_DEFAULT;
    int padding = TFORM_DEFAULT_PADDING;
    TFormTextStyle label_style;
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
    TFormField *field = append_field(self, key, description, TFORM_FIELD_BOOL);
    if (!field) {
        tform_text_style_free(&label_style);
        return NULL;
    }
    field->label = label_style;
    tform_text_style_init(&label_style);
    field->field_color = field_color;
    field->bool_value = default_value ? 1 : 0;
    field->padding = padding;
    Py_RETURN_NONE;
}

static PyObject *Form_add_int(PyObject *self_obj, PyObject *args, PyObject *kwargs) {
    TFormObject *self = (TFormObject *)self_obj;
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
    TFormTextColor field_color = TFORM_TEXT_COLOR_DEFAULT;
    int padding = TFORM_DEFAULT_PADDING;
    TFormTextStyle label_style;
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
            tform_text_style_free(&label_style);
            return NULL;
        }
        if (parsed < INT_MIN || parsed > INT_MAX) {
            tform_text_style_free(&label_style);
            PyErr_SetString(PyExc_OverflowError, "min is outside the C int range");
            return NULL;
        }
        min_value = (int)parsed;
    }
    if (has_max) {
        long parsed = PyLong_AsLong(max_obj);
        if (PyErr_Occurred()) {
            tform_text_style_free(&label_style);
            return NULL;
        }
        if (parsed < INT_MIN || parsed > INT_MAX) {
            tform_text_style_free(&label_style);
            PyErr_SetString(PyExc_OverflowError, "max is outside the C int range");
            return NULL;
        }
        max_value = (int)parsed;
    }
    if (has_min && has_max && min_value > max_value) {
        tform_text_style_free(&label_style);
        PyErr_SetString(PyExc_ValueError, "min must be less than or equal to max");
        return NULL;
    }
    if ((has_min && default_value < min_value) || (has_max && default_value > max_value)) {
        tform_text_style_free(&label_style);
        PyErr_SetString(PyExc_ValueError, "default must be within min and max bounds");
        return NULL;
    }
    int scrollable = PyObject_IsTrue(scrollable_obj);
    if (scrollable < 0) {
        tform_text_style_free(&label_style);
        return NULL;
    }
    TFormField *field = append_field(self, key, description, TFORM_FIELD_INT);
    if (!field) {
        tform_text_style_free(&label_style);
        return NULL;
    }
    field->int_value = default_value;
    field->label = label_style;
    tform_text_style_init(&label_style);
    field->field_color = field_color;
    field->scrollable = scrollable;
    field->padding = padding;
    field->has_min = has_min;
    field->min_value = min_value;
    field->has_max = has_max;
    field->max_value = max_value;
    Py_RETURN_NONE;
}

static int choices_from_sequence(TFormField *field, PyObject *choices, const char *default_value) {
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
        field->choices[i] = tform_strdup(choice);
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
    field->text_value = tform_strdup(selected);
    Py_DECREF(seq);
    return field->text_value ? 0 : -1;
}

static PyObject *Form_add_choice(PyObject *self_obj, PyObject *args, PyObject *kwargs) {
    TFormObject *self = (TFormObject *)self_obj;
    static char *kwlist[] = {"key", "label", "choices", "default", "description", "field_color", "padding", NULL};
    const char *key = NULL;
    PyObject *label_obj = NULL;
    PyObject *choices = NULL;
    const char *default_value = NULL;
    const char *description = "";
    const char *field_color_text = "default";
    PyObject *padding_obj = Py_None;
    TFormTextColor field_color = TFORM_TEXT_COLOR_DEFAULT;
    int padding = TFORM_DEFAULT_PADDING;
    TFormTextStyle label_style;
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
    TFormField *field = append_field(self, key, description, TFORM_FIELD_CHOICE);
    if (!field) {
        tform_text_style_free(&label_style);
        return NULL;
    }
    field->label = label_style;
    tform_text_style_init(&label_style);
    field->field_color = field_color;
    field->padding = padding;
    if (choices_from_sequence(field, choices, default_value) < 0) {
        self->field_count--;
        tform_field_free(field);
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

static int checkbox_options_from_sequence(TFormField *field, PyObject *options, PyObject *default_value) {
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
    field->checkbox_options = calloc((size_t)count, sizeof(TFormTextStyle));
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
    TFormObject *self = (TFormObject *)self_obj;
    static char *kwlist[] = {"key", "label", "options", "default", "description", "field_color", "tick", "padding", NULL};
    const char *key = NULL;
    PyObject *label_obj = NULL;
    PyObject *options = NULL;
    PyObject *default_value = Py_None;
    const char *description = "";
    const char *field_color_text = "default";
    const char *tick_text = "x";
    PyObject *padding_obj = Py_None;
    TFormTextColor field_color = TFORM_TEXT_COLOR_DEFAULT;
    TFormCheckboxTick checkbox_tick = TFORM_CHECKBOX_TICK_X;
    int padding = TFORM_DEFAULT_PADDING;
    TFormTextStyle label_style;
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
    TFormField *field = append_field(self, key, description, TFORM_FIELD_CHECKBOX);
    if (!field) {
        tform_text_style_free(&label_style);
        return NULL;
    }
    field->label = label_style;
    tform_text_style_init(&label_style);
    field->field_color = field_color;
    field->checkbox_tick = checkbox_tick;
    field->padding = padding;
    if (checkbox_options_from_sequence(field, options, default_value) < 0) {
        self->field_count--;
        tform_field_free(field);
        return NULL;
    }
    Py_RETURN_NONE;
}

static int list_from_sequence(TFormField *field, PyObject *default_value) {
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
        TFormTextStyle style;
        if (parse_label_object(items[i], 1, &style) < 0) {
            Py_DECREF(seq);
            return -1;
        }
        char *copy = tform_strdup(style.value ? style.value : "");
        if (!copy) {
            tform_text_style_free(&style);
            Py_DECREF(seq);
            return -1;
        }
        trim_in_place(copy);
        if (*copy) {
            if (tform_text_style_set_value(&style, copy) < 0 || tform_list_append_value(field, copy, &style) < 0) {
                free(copy);
                tform_text_style_free(&style);
                Py_DECREF(seq);
                PyErr_NoMemory();
                return -1;
            }
        }
        tform_text_style_free(&style);
        free(copy);
    }
    Py_DECREF(seq);
    return 0;
}

static PyObject *Form_add_list(PyObject *self_obj, PyObject *args, PyObject *kwargs) {
    TFormObject *self = (TFormObject *)self_obj;
    static char *kwlist[] = {"key", "label", "default", "description", "field_color", "tools", "padding", NULL};
    const char *key = NULL;
    PyObject *label_obj = NULL;
    PyObject *default_value = Py_None;
    const char *description = "";
    const char *field_color_text = "default";
    PyObject *tools_obj = Py_None;
    PyObject *padding_obj = Py_None;
    TFormTextColor field_color = TFORM_TEXT_COLOR_DEFAULT;
    int padding = TFORM_DEFAULT_PADDING;
    TFormTextStyle label_style;
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
    TFormField *field = append_field(self, key, description, TFORM_FIELD_LIST);
    if (!field) {
        tform_text_style_free(&label_style);
        return NULL;
    }
    field->label = label_style;
    tform_text_style_init(&label_style);
    field->field_color = field_color;
    field->padding = padding;
    if (apply_tools(field, tools_obj) < 0) {
        self->field_count--;
        tform_field_free(field);
        return NULL;
    }
    if (list_from_sequence(field, default_value) < 0) {
        self->field_count--;
        tform_field_free(field);
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *Form_run(PyObject *self_obj, PyObject *Py_UNUSED(ignored)) {
    TFormObject *self = (TFormObject *)self_obj;
    if (self->field_count <= 0) {
        PyErr_SetString(PyExc_ValueError, "form must contain at least one field");
        return NULL;
    }
    int result = tform_run_form(self);
    if (result < 0) {
        PyErr_SetString(PyExc_RuntimeError, "could not run form");
        return NULL;
    }
    if (result == 0) {
        Py_RETURN_NONE;
    }
    return tform_build_result(self);
}

PyObject *tform_build_result(TFormObject *self) {
    PyObject *dict = PyDict_New();
    if (!dict) {
        return NULL;
    }
    for (int i = 0; i < self->field_count; i++) {
        TFormField *field = &self->fields[i];
        PyObject *value = NULL;
        switch (field->kind) {
            case TFORM_FIELD_TEXT:
            case TFORM_FIELD_OPTIONAL_TEXT:
            case TFORM_FIELD_CHOICE:
                value = PyUnicode_FromString(field->text_value ? field->text_value : "");
                break;
            case TFORM_FIELD_BOOL:
                value = PyBool_FromLong(field->bool_value);
                break;
            case TFORM_FIELD_INT:
                value = PyLong_FromLong(field->int_value);
                break;
            case TFORM_FIELD_LIST:
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
            case TFORM_FIELD_CHECKBOX:
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
            case TFORM_FIELD_TEXT_SECTION:
            case TFORM_FIELD_SEPARATOR:
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
    {"add_optional_text_input", (PyCFunction)Form_add_optional_text, METH_VARARGS | METH_KEYWORDS, "Add an optional text input field."},
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

PyTypeObject TFormType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "tform.Form",
    .tp_basicsize = sizeof(TFormObject),
    .tp_itemsize = 0,
    .tp_dealloc = (destructor)Form_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Native Ghostcode-style ncurses form.",
    .tp_methods = Form_methods,
    .tp_init = (initproc)Form_init,
    .tp_new = Form_new,
};
