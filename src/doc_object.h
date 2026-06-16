#ifndef TFORM_DOC_OBJECT_H
#define TFORM_DOC_OBJECT_H

#include "form_object.h"

#include <Python.h>

typedef struct {
    PyObject_HEAD
    char *code;
    char *lang;
    int background;
} TFormCodeBlockObject;

typedef enum {
    TFORM_PARAGRAPH_RUN_STRING,
    TFORM_PARAGRAPH_RUN_TEXT
} TFormParagraphRunKind;

typedef struct {
    TFormParagraphRunKind kind;
    PyObject *object;
} TFormParagraphRun;

typedef struct {
    PyObject_HEAD
    TFormParagraphRun *runs;
    Py_ssize_t run_count;
    Py_ssize_t run_capacity;
} TFormParagraphObject;

typedef enum {
    TFORM_DOC_LIST_ITEM_PARAGRAPH,
    TFORM_DOC_LIST_ITEM_ORDERED_LIST,
    TFORM_DOC_LIST_ITEM_UNORDERED_LIST
} TFormDocListItemKind;

typedef struct {
    TFormDocListItemKind kind;
    PyObject *object;
} TFormDocListItem;

typedef struct {
    PyObject_HEAD
    TFormDocListItem *items;
    Py_ssize_t item_count;
    Py_ssize_t item_capacity;
    int ordered;
} TFormDocListObject;

typedef enum {
    TFORM_DOC_NODE_PARAGRAPH,
    TFORM_DOC_NODE_GROUP,
    TFORM_DOC_NODE_TABLE,
    TFORM_DOC_NODE_CODE_BLOCK,
    TFORM_DOC_NODE_BRAILLE_CANVAS,
    TFORM_DOC_NODE_IMAGE,
    TFORM_DOC_NODE_ORDERED_LIST,
    TFORM_DOC_NODE_UNORDERED_LIST
} TFormDocNodeKind;

typedef struct {
    TFormDocNodeKind kind;
    PyObject *object;
} TFormDocNode;

typedef struct {
    PyObject_HEAD
    char *label;
    TFormDocNode *children;
    Py_ssize_t child_count;
    Py_ssize_t child_capacity;
    int boxed;
    int padding;
} TFormDocGroupObject;

typedef struct {
    PyObject_HEAD
    char *title;
    TFormDocNode *children;
    Py_ssize_t child_count;
    Py_ssize_t child_capacity;
    int margin_left;
    int margin_right;
    int margin_top;
    int margin_bottom;
    int syntax_highlight;
    int show_border;
} TFormDocObject;

extern PyTypeObject TFormDocType;
extern PyTypeObject TFormDocGroupType;
extern PyTypeObject TFormCodeBlockType;
extern PyTypeObject TFormParagraphType;
extern PyTypeObject TFormOrderedListType;
extern PyTypeObject TFormUnorderedListType;

#endif
