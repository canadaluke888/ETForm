#ifndef ETFORM_DOC_OBJECT_H
#define ETFORM_DOC_OBJECT_H

#include "form_object.h"

#include <Python.h>

typedef struct {
    PyObject_HEAD
    char *code;
    char *lang;
    int background;
} ETFormCodeBlockObject;

typedef enum {
    ETFORM_PARAGRAPH_RUN_STRING,
    ETFORM_PARAGRAPH_RUN_TEXT
} ETFormParagraphRunKind;

typedef struct {
    ETFormParagraphRunKind kind;
    PyObject *object;
} ETFormParagraphRun;

typedef struct {
    PyObject_HEAD
    ETFormParagraphRun *runs;
    Py_ssize_t run_count;
    Py_ssize_t run_capacity;
} ETFormParagraphObject;

typedef enum {
    ETFORM_DOC_LIST_ITEM_PARAGRAPH,
    ETFORM_DOC_LIST_ITEM_ORDERED_LIST,
    ETFORM_DOC_LIST_ITEM_UNORDERED_LIST
} ETFormDocListItemKind;

typedef struct {
    ETFormDocListItemKind kind;
    PyObject *object;
} ETFormDocListItem;

typedef struct {
    PyObject_HEAD
    ETFormDocListItem *items;
    Py_ssize_t item_count;
    Py_ssize_t item_capacity;
    int ordered;
} ETFormDocListObject;

typedef enum {
    ETFORM_DOC_NODE_PARAGRAPH,
    ETFORM_DOC_NODE_GROUP,
    ETFORM_DOC_NODE_TABLE,
    ETFORM_DOC_NODE_CODE_BLOCK,
    ETFORM_DOC_NODE_BRAILLE_CANVAS,
    ETFORM_DOC_NODE_IMAGE,
    ETFORM_DOC_NODE_ORDERED_LIST,
    ETFORM_DOC_NODE_UNORDERED_LIST
} ETFormDocNodeKind;

typedef struct {
    ETFormDocNodeKind kind;
    PyObject *object;
} ETFormDocNode;

typedef struct {
    PyObject_HEAD
    char *label;
    ETFormDocNode *children;
    Py_ssize_t child_count;
    Py_ssize_t child_capacity;
    int boxed;
    int padding;
} ETFormDocGroupObject;

typedef struct {
    PyObject_HEAD
    char *title;
    ETFormDocNode *children;
    Py_ssize_t child_count;
    Py_ssize_t child_capacity;
    int margin_left;
    int margin_right;
    int margin_top;
    int margin_bottom;
    int syntax_highlight;
    int show_border;
} ETFormDocObject;

extern PyTypeObject ETFormDocType;
extern PyTypeObject ETFormDocGroupType;
extern PyTypeObject ETFormCodeBlockType;
extern PyTypeObject ETFormParagraphType;
extern PyTypeObject ETFormOrderedListType;
extern PyTypeObject ETFormUnorderedListType;

#endif
