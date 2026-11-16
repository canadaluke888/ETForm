#ifndef ETFORM_FORM_OBJECT_H
#define ETFORM_FORM_OBJECT_H

#include "braille_canvas.h"
#include "form_helpers.h"

#include <Python.h>

#define ETFORM_DEFAULT_PADDING 1

typedef enum {
    ETFORM_FIELD_TEXT,
    ETFORM_FIELD_CHOICE,
    ETFORM_FIELD_BOOL,
    ETFORM_FIELD_INT,
    ETFORM_FIELD_LIST,
    ETFORM_FIELD_CHECKBOX,
    ETFORM_FIELD_TEXT_SECTION,
    ETFORM_FIELD_SEPARATOR,
    ETFORM_FIELD_SECTION,
    ETFORM_FIELD_ITABLE
} ETFormFieldKind;

typedef enum {
    ETFORM_LABEL_ALIGN_LEFT,
    ETFORM_LABEL_ALIGN_CENTER,
    ETFORM_LABEL_ALIGN_RIGHT
} ETFormLabelAlign;

typedef enum {
    ETFORM_TEXT_COLOR_DEFAULT,
    ETFORM_TEXT_COLOR_WHITE,
    ETFORM_TEXT_COLOR_CYAN,
    ETFORM_TEXT_COLOR_YELLOW,
    ETFORM_TEXT_COLOR_GREEN,
    ETFORM_TEXT_COLOR_BLUE,
    ETFORM_TEXT_COLOR_MAGENTA,
    ETFORM_TEXT_COLOR_RED
} ETFormTextColor;

typedef enum {
    ETFORM_CHECKBOX_TICK_X,
    ETFORM_CHECKBOX_TICK_BLOCK,
    ETFORM_CHECKBOX_TICK_DOT
} ETFormCheckboxTick;

typedef struct {
    char *value;
    ETFormLabelAlign align;
    int bold;
    int italicized;
    int dim;
    int underline;
    int blink;
    int strikethrough;
    ETFormTextColor color;
    int wrap_text;
} ETFormTextStyle;

typedef struct {
    int is_separator;
    char **cells;
    ETFormTextStyle separator;
} ETFormTableRow;

typedef struct {
    ETFormTextStyle title;
    int show_lines;
    int show_gutter;
    ETFormTextColor color;
    int padding;
    char **columns;
    int column_count;
    int column_capacity;
    ETFormTableRow *rows;
    int row_count;
    int row_capacity;
} ETFormTable;

typedef enum {
    ETFORM_SECTION_ELEMENT_TEXT,
    ETFORM_SECTION_ELEMENT_TABLE,
    ETFORM_SECTION_ELEMENT_BRAILLE_CANVAS
} ETFormSectionElementKind;

typedef struct {
    ETFormSectionElementKind kind;
    ETFormTextStyle text;
    ETFormTable table;
    BrailleCanvas canvas;
    ETFormTextColor canvas_color;
    int canvas_padding;
} ETFormSectionElement;

typedef struct {
    char *key;
    ETFormTextStyle label;
    char *description;
    ETFormFieldKind kind;
    int padding;
    int scrollable;
    int wrap_text;
    int has_file_picker;
    int file_picker_mode;
    char *file_picker_start_dir;
    int has_calendar;
    int has_clock;
    int clock_format_12h;
    int clock_include_seconds;
    int has_min;
    int min_value;
    int has_max;
    int max_value;
    char *text_value;
    char *default_value;
    ETFormTextStyle placeholder;
    int bool_value;
    int int_value;
    ETFormTextColor field_color;
    ETFormTextColor separator_color;
    char **choices;
    int choice_count;
    ETFormStringList list_value;
    ETFormTextStyle *list_styles;
    int list_style_capacity;
    ETFormTextStyle *checkbox_options;
    int *checkbox_checked;
    int checkbox_count;
    ETFormCheckboxTick checkbox_tick;
    ETFormSectionElement *section_elements;
    int section_element_count;
    int section_element_capacity;
    int section_boxed;
    ETFormTable interactive_table;
    unsigned char *table_column_colors;
} ETFormField;

typedef struct {
    PyObject_HEAD
    char *title;
    ETFormField *fields;
    int field_count;
    int field_capacity;
} ETFormObject;

extern PyTypeObject ETFormType;
extern PyTypeObject ETFormTextType;
extern PyTypeObject ETFormFilePickerType;
extern PyTypeObject ETFormCalendarType;
extern PyTypeObject ETFormClockType;
extern PyTypeObject ETFormBrailleCanvasType;
extern PyTypeObject ETFormImageType;
extern PyTypeObject ETFormTableType;
extern PyTypeObject ETFormITableType;

void etform_text_style_init(ETFormTextStyle *style);
void etform_text_style_free(ETFormTextStyle *style);
int etform_text_style_copy(ETFormTextStyle *dest, const ETFormTextStyle *src, const char *fallback_value);
int etform_text_style_from_object(PyObject *object, int allow_empty, ETFormTextStyle *style);
void etform_table_init(ETFormTable *table);
void etform_table_free(ETFormTable *table);
int etform_table_copy(ETFormTable *dest, const ETFormTable *src);
int etform_table_add_empty_row(ETFormTable *table);
int etform_table_delete_row(ETFormTable *table, int index);
void etform_section_element_free(ETFormSectionElement *element);
int etform_section_element_from_object(ETFormSectionElement *element, PyObject *item);
void etform_field_free(ETFormField *field);
int etform_list_append_value(ETFormField *field, const char *value, const ETFormTextStyle *style);
int etform_list_set_value(ETFormField *field, int index, const char *value, const ETFormTextStyle *style);
int etform_list_delete_value(ETFormField *field, int index);
PyObject *etform_build_result(ETFormObject *self);

#endif
