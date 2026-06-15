#ifndef TFORM_DOC_OBJECT_H
#define TFORM_DOC_OBJECT_H

#include "form_object.h"

#include <Python.h>

typedef enum {
    TFORM_DOC_ITEM_STATIC,
    TFORM_DOC_ITEM_PARAGRAPH,
    TFORM_DOC_ITEM_HEADING,
    TFORM_DOC_ITEM_CODE_BLOCK,
    TFORM_DOC_ITEM_SEPARATOR,
    TFORM_DOC_ITEM_SECTION
} TFormDocItemType;

typedef struct {
    TFormDocItemType type;
    TFormSectionElement element;
    TFormTextStyle text;
    int heading_level;
    char *code;
    char *lang;
    int code_background;
    PyObject *section;
} TFormDocItem;

typedef struct {
    PyObject_HEAD
    char *title;
    TFormDocItem *items;
    int item_count;
    int item_capacity;
    int boxed;
    int padding;
} TFormDocSectionObject;

typedef struct {
    PyObject_HEAD
    char *code;
    char *lang;
    int background;
} TFormCodeBlockObject;

typedef struct {
    PyObject_HEAD
    char *title;
    PyObject **sections;
    int section_count;
    int section_capacity;
    int margin_left;
    int margin_right;
    int margin_top;
    int margin_bottom;
    int syntax_highlight;
} TFormDocObject;

extern PyTypeObject TFormDocType;
extern PyTypeObject TFormDocSectionType;
extern PyTypeObject TFormCodeBlockType;

void tform_doc_item_free(TFormDocItem *item);

#endif
