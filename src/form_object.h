#ifndef TFORM_FORM_OBJECT_H
#define TFORM_FORM_OBJECT_H

#include "form_helpers.h"

#include <Python.h>

#define TFORM_DEFAULT_PADDING 1

typedef enum {
    TFORM_FIELD_TEXT,
    TFORM_FIELD_CHOICE,
    TFORM_FIELD_BOOL,
    TFORM_FIELD_INT,
    TFORM_FIELD_LIST,
    TFORM_FIELD_CHECKBOX,
    TFORM_FIELD_TEXT_SECTION,
    TFORM_FIELD_SEPARATOR,
    TFORM_FIELD_SECTION
} TFormFieldKind;

typedef enum {
    TFORM_LABEL_ALIGN_LEFT,
    TFORM_LABEL_ALIGN_CENTER,
    TFORM_LABEL_ALIGN_RIGHT
} TFormLabelAlign;

typedef enum {
    TFORM_TEXT_COLOR_DEFAULT,
    TFORM_TEXT_COLOR_WHITE,
    TFORM_TEXT_COLOR_CYAN,
    TFORM_TEXT_COLOR_YELLOW,
    TFORM_TEXT_COLOR_GREEN,
    TFORM_TEXT_COLOR_BLUE,
    TFORM_TEXT_COLOR_MAGENTA,
    TFORM_TEXT_COLOR_RED
} TFormTextColor;

typedef enum {
    TFORM_CHECKBOX_TICK_X,
    TFORM_CHECKBOX_TICK_BLOCK,
    TFORM_CHECKBOX_TICK_DOT
} TFormCheckboxTick;

typedef struct {
    char *value;
    TFormLabelAlign align;
    int bold;
    int italicized;
    TFormTextColor color;
    int wrap_text;
} TFormTextStyle;

typedef struct {
    int is_separator;
    char **cells;
} TFormTableRow;

typedef struct {
    TFormTextStyle title;
    int show_lines;
    TFormTextColor color;
    int padding;
    char **columns;
    int column_count;
    int column_capacity;
    TFormTableRow *rows;
    int row_count;
    int row_capacity;
} TFormTable;

typedef enum {
    TFORM_SECTION_ELEMENT_TEXT,
    TFORM_SECTION_ELEMENT_TABLE
} TFormSectionElementKind;

typedef struct {
    TFormSectionElementKind kind;
    TFormTextStyle text;
    TFormTable table;
} TFormSectionElement;

typedef struct {
    char *key;
    TFormTextStyle label;
    char *description;
    TFormFieldKind kind;
    int padding;
    int scrollable;
    int wrap_text;
    int has_file_picker;
    int file_picker_mode;
    char *file_picker_start_dir;
    int has_min;
    int min_value;
    int has_max;
    int max_value;
    char *text_value;
    char *default_value;
    TFormTextStyle placeholder;
    int bool_value;
    int int_value;
    TFormTextColor field_color;
    TFormTextColor separator_color;
    char **choices;
    int choice_count;
    TFormStringList list_value;
    TFormTextStyle *list_styles;
    int list_style_capacity;
    TFormTextStyle *checkbox_options;
    int *checkbox_checked;
    int checkbox_count;
    TFormCheckboxTick checkbox_tick;
    TFormSectionElement *section_elements;
    int section_element_count;
    int section_element_capacity;
    int section_boxed;
} TFormField;

typedef struct {
    PyObject_HEAD
    char *title;
    TFormField *fields;
    int field_count;
    int field_capacity;
} TFormObject;

extern PyTypeObject TFormType;
extern PyTypeObject TFormTextType;
extern PyTypeObject TFormFilePickerType;
extern PyTypeObject TFormTableType;

void tform_text_style_init(TFormTextStyle *style);
void tform_text_style_free(TFormTextStyle *style);
void tform_table_init(TFormTable *table);
void tform_table_free(TFormTable *table);
int tform_table_copy(TFormTable *dest, const TFormTable *src);
void tform_field_free(TFormField *field);
int tform_list_append_value(TFormField *field, const char *value, const TFormTextStyle *style);
int tform_list_set_value(TFormField *field, int index, const char *value, const TFormTextStyle *style);
int tform_list_delete_value(TFormField *field, int index);
PyObject *tform_build_result(TFormObject *self);

#endif
