#include "doc_object.h"

#include "doc_runtime.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static char *doc_strdup(const char *value) {
    char *copy = strdup(value ? value : "");
    if (!copy) PyErr_NoMemory();
    return copy;
}

static int parse_nonnegative(PyObject *value, int fallback, const char *name, int *result) {
    if (value == Py_None) {
        *result = fallback;
        return 0;
    }
    long parsed = PyLong_AsLong(value);
    if ((parsed == -1 && PyErr_Occurred()) || parsed < 0 || parsed > INT_MAX) {
        if (!PyErr_Occurred()) {
            PyErr_Format(PyExc_ValueError, "%s must be a non-negative integer", name);
        }
        return -1;
    }
    *result = (int)parsed;
    return 0;
}

static int paragraph_ensure_capacity(ETFormParagraphObject *self, Py_ssize_t needed) {
    if (self->run_capacity >= needed) return 0;
    Py_ssize_t capacity = self->run_capacity ? self->run_capacity * 2 : 4;
    while (capacity < needed) capacity *= 2;
    ETFormParagraphRun *runs = realloc(self->runs, (size_t)capacity * sizeof(*runs));
    if (!runs) {
        PyErr_NoMemory();
        return -1;
    }
    memset(
        runs + self->run_capacity,
        0,
        (size_t)(capacity - self->run_capacity) * sizeof(*runs)
    );
    self->runs = runs;
    self->run_capacity = capacity;
    return 0;
}

static int paragraph_append_run(ETFormParagraphObject *self, PyObject *object) {
    if (!PyUnicode_Check(object) && !PyObject_TypeCheck(object, &ETFormTextType)) {
        PyErr_SetString(PyExc_TypeError, "Paragraph parts must be str or Text");
        return -1;
    }
    if (paragraph_ensure_capacity(self, self->run_count + 1) < 0) return -1;
    ETFormParagraphRun *run = &self->runs[self->run_count++];
    run->kind = PyObject_TypeCheck(object, &ETFormTextType)
        ? ETFORM_PARAGRAPH_RUN_TEXT
        : ETFORM_PARAGRAPH_RUN_STRING;
    Py_INCREF(object);
    run->object = object;
    return 0;
}

static PyObject *Paragraph_new(PyTypeObject *type, PyObject *args, PyObject *kwargs) {
    (void)args;
    (void)kwargs;
    ETFormParagraphObject *self = (ETFormParagraphObject *)type->tp_alloc(type, 0);
    if (!self) return NULL;
    self->runs = NULL;
    self->run_count = 0;
    self->run_capacity = 0;
    return (PyObject *)self;
}

static int Paragraph_init(ETFormParagraphObject *self, PyObject *args, PyObject *kwargs) {
    if (kwargs && PyDict_Size(kwargs) > 0) {
        PyErr_SetString(PyExc_TypeError, "Paragraph does not accept keyword arguments");
        return -1;
    }
    for (Py_ssize_t i = 0; i < self->run_count; i++) {
        Py_CLEAR(self->runs[i].object);
    }
    self->run_count = 0;
    Py_ssize_t count = PyTuple_GET_SIZE(args);
    for (Py_ssize_t i = 0; i < count; i++) {
        if (paragraph_append_run(self, PyTuple_GET_ITEM(args, i)) < 0) {
            return -1;
        }
    }
    return 0;
}

static int Paragraph_traverse(ETFormParagraphObject *self, visitproc visit, void *arg) {
    for (Py_ssize_t i = 0; i < self->run_count; i++) {
        Py_VISIT(self->runs[i].object);
    }
    return 0;
}

static int Paragraph_clear(ETFormParagraphObject *self) {
    for (Py_ssize_t i = 0; i < self->run_count; i++) {
        Py_CLEAR(self->runs[i].object);
    }
    self->run_count = 0;
    return 0;
}

static void Paragraph_dealloc(ETFormParagraphObject *self) {
    PyObject_GC_UnTrack(self);
    Paragraph_clear(self);
    free(self->runs);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *Paragraph_repr(ETFormParagraphObject *self) {
    return PyUnicode_FromFormat("Paragraph(parts=%zd)", self->run_count);
}

PyTypeObject ETFormParagraphType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "etform.Paragraph",
    .tp_basicsize = sizeof(ETFormParagraphObject),
    .tp_dealloc = (destructor)Paragraph_dealloc,
    .tp_repr = (reprfunc)Paragraph_repr,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_doc = "Block-level Doc paragraph containing inline str and Text runs.",
    .tp_traverse = (traverseproc)Paragraph_traverse,
    .tp_clear = (inquiry)Paragraph_clear,
    .tp_init = (initproc)Paragraph_init,
    .tp_new = Paragraph_new,
};

static PyObject *paragraph_from_object(PyObject *object) {
    if (PyObject_TypeCheck(object, &ETFormParagraphType)) {
        return Py_NewRef(object);
    }
    if (PyUnicode_Check(object) || PyObject_TypeCheck(object, &ETFormTextType)) {
        return PyObject_CallFunctionObjArgs((PyObject *)&ETFormParagraphType, object, NULL);
    }
    PyErr_SetString(PyExc_TypeError, "value must be str, Text, or Paragraph");
    return NULL;
}

static int list_ensure_capacity(ETFormDocListObject *self, Py_ssize_t needed) {
    if (self->item_capacity >= needed) return 0;
    Py_ssize_t capacity = self->item_capacity ? self->item_capacity * 2 : 4;
    while (capacity < needed) capacity *= 2;
    ETFormDocListItem *items = realloc(self->items, (size_t)capacity * sizeof(*items));
    if (!items) {
        PyErr_NoMemory();
        return -1;
    }
    memset(
        items + self->item_capacity,
        0,
        (size_t)(capacity - self->item_capacity) * sizeof(*items)
    );
    self->items = items;
    self->item_capacity = capacity;
    return 0;
}

static int list_append_item(ETFormDocListObject *self, PyObject *object) {
    ETFormDocListItemKind kind;
    PyObject *stored = NULL;
    if (PyObject_TypeCheck(object, &ETFormOrderedListType)) {
        kind = ETFORM_DOC_LIST_ITEM_ORDERED_LIST;
        stored = Py_NewRef(object);
    } else if (PyObject_TypeCheck(object, &ETFormUnorderedListType)) {
        kind = ETFORM_DOC_LIST_ITEM_UNORDERED_LIST;
        stored = Py_NewRef(object);
    } else {
        kind = ETFORM_DOC_LIST_ITEM_PARAGRAPH;
        stored = paragraph_from_object(object);
    }
    if (!stored) return -1;
    if (list_ensure_capacity(self, self->item_count + 1) < 0) {
        Py_DECREF(stored);
        return -1;
    }
    ETFormDocListItem *item = &self->items[self->item_count++];
    item->kind = kind;
    item->object = stored;
    return 0;
}

static PyObject *DocList_new(PyTypeObject *type, PyObject *args, PyObject *kwargs) {
    (void)args;
    (void)kwargs;
    ETFormDocListObject *self = (ETFormDocListObject *)type->tp_alloc(type, 0);
    if (!self) return NULL;
    self->items = NULL;
    self->item_count = 0;
    self->item_capacity = 0;
    self->ordered = type == &ETFormOrderedListType;
    return (PyObject *)self;
}

static int DocList_init(ETFormDocListObject *self, PyObject *args, PyObject *kwargs) {
    if (kwargs && PyDict_Size(kwargs) > 0) {
        PyErr_SetString(PyExc_TypeError, "list renderables do not accept keyword arguments");
        return -1;
    }
    for (Py_ssize_t i = 0; i < self->item_count; i++) {
        Py_CLEAR(self->items[i].object);
    }
    self->item_count = 0;
    Py_ssize_t count = PyTuple_GET_SIZE(args);
    for (Py_ssize_t i = 0; i < count; i++) {
        if (list_append_item(self, PyTuple_GET_ITEM(args, i)) < 0) {
            return -1;
        }
    }
    return 0;
}

static int DocList_traverse(ETFormDocListObject *self, visitproc visit, void *arg) {
    for (Py_ssize_t i = 0; i < self->item_count; i++) {
        Py_VISIT(self->items[i].object);
    }
    return 0;
}

static int DocList_clear(ETFormDocListObject *self) {
    for (Py_ssize_t i = 0; i < self->item_count; i++) {
        Py_CLEAR(self->items[i].object);
    }
    self->item_count = 0;
    return 0;
}

static void DocList_dealloc(ETFormDocListObject *self) {
    PyObject_GC_UnTrack(self);
    DocList_clear(self);
    free(self->items);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *DocList_repr(ETFormDocListObject *self) {
    return PyUnicode_FromFormat(
        "%s(items=%zd)",
        self->ordered ? "OrderedList" : "UnorderedList",
        self->item_count
    );
}

PyTypeObject ETFormOrderedListType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "etform.OrderedList",
    .tp_basicsize = sizeof(ETFormDocListObject),
    .tp_dealloc = (destructor)DocList_dealloc,
    .tp_repr = (reprfunc)DocList_repr,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_doc = "Ordered static Doc list renderable.",
    .tp_traverse = (traverseproc)DocList_traverse,
    .tp_clear = (inquiry)DocList_clear,
    .tp_init = (initproc)DocList_init,
    .tp_new = DocList_new,
};

PyTypeObject ETFormUnorderedListType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "etform.UnorderedList",
    .tp_basicsize = sizeof(ETFormDocListObject),
    .tp_dealloc = (destructor)DocList_dealloc,
    .tp_repr = (reprfunc)DocList_repr,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_doc = "Unordered static Doc list renderable.",
    .tp_traverse = (traverseproc)DocList_traverse,
    .tp_clear = (inquiry)DocList_clear,
    .tp_init = (initproc)DocList_init,
    .tp_new = DocList_new,
};

static int doc_node_array_ensure_capacity(
    ETFormDocNode **children,
    Py_ssize_t *capacity,
    Py_ssize_t needed
) {
    if (*capacity >= needed) return 0;
    Py_ssize_t next_capacity = *capacity ? *capacity * 2 : 4;
    while (next_capacity < needed) next_capacity *= 2;
    ETFormDocNode *next = realloc(*children, (size_t)next_capacity * sizeof(*next));
    if (!next) {
        PyErr_NoMemory();
        return -1;
    }
    memset(
        next + *capacity,
        0,
        (size_t)(next_capacity - *capacity) * sizeof(*next)
    );
    *children = next;
    *capacity = next_capacity;
    return 0;
}

static int doc_normalize_renderable(
    PyObject *object,
    ETFormDocNodeKind *kind,
    PyObject **stored
) {
    if (PyObject_TypeCheck(object, &ETFormDocGroupType)) {
        *kind = ETFORM_DOC_NODE_GROUP;
        *stored = Py_NewRef(object);
    } else if (PyObject_TypeCheck(object, &ETFormCodeBlockType)) {
        *kind = ETFORM_DOC_NODE_CODE_BLOCK;
        *stored = Py_NewRef(object);
    } else if (PyObject_TypeCheck(object, &ETFormTableType)) {
        *kind = ETFORM_DOC_NODE_TABLE;
        *stored = Py_NewRef(object);
    } else if (PyObject_TypeCheck(object, &ETFormBrailleCanvasType)) {
        *kind = ETFORM_DOC_NODE_BRAILLE_CANVAS;
        *stored = Py_NewRef(object);
    } else if (PyObject_TypeCheck(object, &ETFormImageType)) {
        *kind = ETFORM_DOC_NODE_IMAGE;
        *stored = Py_NewRef(object);
    } else if (PyObject_TypeCheck(object, &ETFormOrderedListType)) {
        *kind = ETFORM_DOC_NODE_ORDERED_LIST;
        *stored = Py_NewRef(object);
    } else if (PyObject_TypeCheck(object, &ETFormUnorderedListType)) {
        *kind = ETFORM_DOC_NODE_UNORDERED_LIST;
        *stored = Py_NewRef(object);
    } else {
        *kind = ETFORM_DOC_NODE_PARAGRAPH;
        *stored = paragraph_from_object(object);
    }
    if (!*stored) {
        PyErr_SetString(
            PyExc_TypeError,
            "document items must be str, Text, Paragraph, Group, Table, "
            "CodeBlock, BrailleCanvas, Image, OrderedList, or UnorderedList"
        );
        return -1;
    }
    return 0;
}

static int doc_node_array_append(
    ETFormDocNode **children,
    Py_ssize_t *count,
    Py_ssize_t *capacity,
    PyObject *object
) {
    ETFormDocNodeKind kind;
    PyObject *stored = NULL;
    if (doc_normalize_renderable(object, &kind, &stored) < 0) return -1;
    if (doc_node_array_ensure_capacity(children, capacity, *count + 1) < 0) {
        Py_DECREF(stored);
        return -1;
    }
    ETFormDocNode *node = &(*children)[(*count)++];
    node->kind = kind;
    node->object = stored;
    return 0;
}

static PyObject *CodeBlock_new(PyTypeObject *type, PyObject *args, PyObject *kwargs) {
    (void)args;
    (void)kwargs;
    ETFormCodeBlockObject *self = (ETFormCodeBlockObject *)type->tp_alloc(type, 0);
    if (!self) return NULL;
    self->code = NULL;
    self->lang = NULL;
    self->background = 1;
    return (PyObject *)self;
}

static int CodeBlock_init(ETFormCodeBlockObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"code", "lang", "background", NULL};
    const char *code = NULL;
    PyObject *lang_obj = Py_None;
    int background = 1;
    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "s|Op", kwlist, &code, &lang_obj, &background)) {
        return -1;
    }
    const char *lang = NULL;
    if (lang_obj != Py_None) {
        lang = PyUnicode_AsUTF8(lang_obj);
        if (!lang) {
            PyErr_SetString(PyExc_TypeError, "lang must be str or None");
            return -1;
        }
    }
    char *code_copy = doc_strdup(code);
    char *lang_copy = lang ? doc_strdup(lang) : NULL;
    if (!code_copy || (lang && !lang_copy)) {
        free(code_copy);
        free(lang_copy);
        return -1;
    }
    free(self->code);
    free(self->lang);
    self->code = code_copy;
    self->lang = lang_copy;
    self->background = background ? 1 : 0;
    return 0;
}

static void CodeBlock_dealloc(ETFormCodeBlockObject *self) {
    free(self->code);
    free(self->lang);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *CodeBlock_repr(ETFormCodeBlockObject *self) {
    if (self->lang) {
        return PyUnicode_FromFormat(
            "CodeBlock(lang='%s', background=%s)",
            self->lang,
            self->background ? "True" : "False"
        );
    }
    return PyUnicode_FromFormat(
        "CodeBlock(lang=None, background=%s)",
        self->background ? "True" : "False"
    );
}

static PyObject *CodeBlock_get_code(ETFormCodeBlockObject *self, void *closure) {
    (void)closure;
    return PyUnicode_FromString(self->code ? self->code : "");
}

static PyObject *CodeBlock_get_lang(ETFormCodeBlockObject *self, void *closure) {
    (void)closure;
    if (!self->lang) Py_RETURN_NONE;
    return PyUnicode_FromString(self->lang);
}

static PyObject *CodeBlock_get_background(ETFormCodeBlockObject *self, void *closure) {
    (void)closure;
    return PyBool_FromLong(self->background);
}

static PyGetSetDef CodeBlock_getset[] = {
    {"code", (getter)CodeBlock_get_code, NULL, "Code text.", NULL},
    {"lang", (getter)CodeBlock_get_lang, NULL, "Language name.", NULL},
    {"background", (getter)CodeBlock_get_background, NULL, "Whether to draw a background.", NULL},
    {NULL, NULL, NULL, NULL, NULL},
};

PyTypeObject ETFormCodeBlockType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "etform.CodeBlock",
    .tp_basicsize = sizeof(ETFormCodeBlockObject),
    .tp_dealloc = (destructor)CodeBlock_dealloc,
    .tp_repr = (reprfunc)CodeBlock_repr,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Read-only code block for Doc sections.",
    .tp_getset = CodeBlock_getset,
    .tp_init = (initproc)CodeBlock_init,
    .tp_new = CodeBlock_new,
};

static PyObject *Group_new(PyTypeObject *type, PyObject *args, PyObject *kwargs) {
    (void)args;
    (void)kwargs;
    ETFormDocGroupObject *self =
        (ETFormDocGroupObject *)type->tp_alloc(type, 0);
    if (!self) return NULL;
    self->label = NULL;
    self->children = NULL;
    self->child_count = 0;
    self->child_capacity = 0;
    self->boxed = 0;
    self->padding = ETFORM_DEFAULT_PADDING;
    return (PyObject *)self;
}

static int Group_init(ETFormDocGroupObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"label", "boxed", "padding", NULL};
    const char *label = NULL;
    int boxed = 0;
    PyObject *padding_obj = Py_None;
    int padding;
    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "s|pO", kwlist, &label, &boxed, &padding_obj)) {
        return -1;
    }
    if (!*label) {
        PyErr_SetString(PyExc_ValueError, "group label must not be empty");
        return -1;
    }
    if (parse_nonnegative(
            padding_obj, ETFORM_DEFAULT_PADDING, "padding", &padding) < 0) {
        return -1;
    }
    char *copy = doc_strdup(label);
    if (!copy) return -1;
    free(self->label);
    self->label = copy;
    self->boxed = boxed ? 1 : 0;
    self->padding = padding;
    return 0;
}

static int Group_traverse(ETFormDocGroupObject *self, visitproc visit, void *arg) {
    for (Py_ssize_t i = 0; i < self->child_count; i++) {
        Py_VISIT(self->children[i].object);
    }
    return 0;
}

static int Group_clear(ETFormDocGroupObject *self) {
    for (Py_ssize_t i = 0; i < self->child_count; i++) {
        Py_CLEAR(self->children[i].object);
    }
    self->child_count = 0;
    return 0;
}

static void Group_dealloc(ETFormDocGroupObject *self) {
    PyObject_GC_UnTrack(self);
    Group_clear(self);
    free(self->children);
    free(self->label);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *Group_repr(ETFormDocGroupObject *self) {
    return PyUnicode_FromFormat(
        "Group('%s', boxed=%s, padding=%d, items=%zd)",
        self->label ? self->label : "",
        self->boxed ? "True" : "False",
        self->padding,
        self->child_count
    );
}

static PyObject *Group_add(ETFormDocGroupObject *self, PyObject *args) {
    Py_ssize_t count = PyTuple_GET_SIZE(args);
    for (Py_ssize_t i = 0; i < count; i++) {
        if (doc_node_array_append(
                &self->children,
                &self->child_count,
                &self->child_capacity,
                PyTuple_GET_ITEM(args, i)) < 0) {
            return NULL;
        }
    }
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *Group_get_label(ETFormDocGroupObject *self, void *closure) {
    (void)closure;
    return PyUnicode_FromString(self->label ? self->label : "");
}

static PyObject *Group_get_boxed(ETFormDocGroupObject *self, void *closure) {
    (void)closure;
    return PyBool_FromLong(self->boxed);
}

static PyObject *Group_get_padding(ETFormDocGroupObject *self, void *closure) {
    (void)closure;
    return PyLong_FromLong(self->padding);
}

static PyMethodDef Group_methods[] = {
    {"add", (PyCFunction)Group_add, METH_VARARGS, "Add renderables to the group."},
    {NULL, NULL, 0, NULL},
};

static PyGetSetDef Group_getset[] = {
    {"label", (getter)Group_get_label, NULL, "Group label.", NULL},
    {"boxed", (getter)Group_get_boxed, NULL, "Whether the group is boxed.", NULL},
    {"padding", (getter)Group_get_padding, NULL, "Group padding.", NULL},
    {NULL, NULL, NULL, NULL, NULL},
};

PyTypeObject ETFormDocGroupType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "etform.Group",
    .tp_basicsize = sizeof(ETFormDocGroupObject),
    .tp_dealloc = (destructor)Group_dealloc,
    .tp_repr = (reprfunc)Group_repr,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_doc = "Read-only document group for related renderables.",
    .tp_traverse = (traverseproc)Group_traverse,
    .tp_clear = (inquiry)Group_clear,
    .tp_methods = Group_methods,
    .tp_getset = Group_getset,
    .tp_init = (initproc)Group_init,
    .tp_new = Group_new,
};

static PyObject *Doc_new(PyTypeObject *type, PyObject *args, PyObject *kwargs) {
    (void)args;
    (void)kwargs;
    ETFormDocObject *self = (ETFormDocObject *)type->tp_alloc(type, 0);
    if (!self) return NULL;
    self->title = NULL;
    self->children = NULL;
    self->child_count = 0;
    self->child_capacity = 0;
    self->margin_left = 0;
    self->margin_right = 0;
    self->margin_top = 0;
    self->margin_bottom = 0;
    self->syntax_highlight = 1;
    self->show_border = 1;
    return (PyObject *)self;
}

static int Doc_init(ETFormDocObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {
        "title", "margin_x", "margin_y", "margin_left", "margin_right",
        "margin_top", "margin_bottom", "syntax_highlight", "show_border", NULL
    };
    const char *title = NULL;
    PyObject *margin_x_obj = Py_None;
    PyObject *margin_y_obj = Py_None;
    PyObject *margin_left_obj = Py_None;
    PyObject *margin_right_obj = Py_None;
    PyObject *margin_top_obj = Py_None;
    PyObject *margin_bottom_obj = Py_None;
    int syntax_highlight = 1;
    int show_border = 1;
    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "s|OOOOOOpp", kwlist,
            &title,
            &margin_x_obj,
            &margin_y_obj,
            &margin_left_obj,
            &margin_right_obj,
            &margin_top_obj,
            &margin_bottom_obj,
            &syntax_highlight,
            &show_border)) {
        return -1;
    }
    int margin_x;
    int margin_y;
    int margin_left;
    int margin_right;
    int margin_top;
    int margin_bottom;
    if (parse_nonnegative(margin_x_obj, 0, "margin_x", &margin_x) < 0 ||
        parse_nonnegative(margin_y_obj, 0, "margin_y", &margin_y) < 0 ||
        parse_nonnegative(margin_left_obj, margin_x, "margin_left", &margin_left) < 0 ||
        parse_nonnegative(margin_right_obj, margin_x, "margin_right", &margin_right) < 0 ||
        parse_nonnegative(margin_top_obj, margin_y, "margin_top", &margin_top) < 0 ||
        parse_nonnegative(margin_bottom_obj, margin_y, "margin_bottom", &margin_bottom) < 0) {
        return -1;
    }
    char *copy = doc_strdup(title);
    if (!copy) return -1;
    free(self->title);
    self->title = copy;
    self->margin_left = margin_left;
    self->margin_right = margin_right;
    self->margin_top = margin_top;
    self->margin_bottom = margin_bottom;
    self->syntax_highlight = syntax_highlight ? 1 : 0;
    self->show_border = show_border ? 1 : 0;
    return 0;
}

static int Doc_traverse(ETFormDocObject *self, visitproc visit, void *arg) {
    for (Py_ssize_t i = 0; i < self->child_count; i++) {
        Py_VISIT(self->children[i].object);
    }
    return 0;
}

static int Doc_clear(ETFormDocObject *self) {
    for (Py_ssize_t i = 0; i < self->child_count; i++) {
        Py_CLEAR(self->children[i].object);
    }
    self->child_count = 0;
    return 0;
}

static void Doc_dealloc(ETFormDocObject *self) {
    PyObject_GC_UnTrack(self);
    Doc_clear(self);
    free(self->children);
    free(self->title);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static int doc_append_child(ETFormDocObject *self, PyObject *object) {
    return doc_node_array_append(
        &self->children,
        &self->child_count,
        &self->child_capacity,
        object
    );
}

static PyObject *Doc_add(ETFormDocObject *self, PyObject *args) {
    Py_ssize_t count = PyTuple_GET_SIZE(args);
    for (Py_ssize_t i = 0; i < count; i++) {
        if (doc_append_child(self, PyTuple_GET_ITEM(args, i)) < 0) {
            return NULL;
        }
    }
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *Doc_run(ETFormDocObject *self, PyObject *Py_UNUSED(ignored)) {
    if (self->child_count <= 0) {
        PyErr_SetString(PyExc_ValueError, "document must contain at least one item");
        return NULL;
    }
    if (etform_run_doc(self) < 0) {
        PyErr_SetString(PyExc_RuntimeError, "could not run document");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef Doc_methods[] = {
    {"add", (PyCFunction)Doc_add, METH_VARARGS, "Add renderables to the document flow."},
    {"run", (PyCFunction)Doc_run, METH_NOARGS, "Open the read-only document viewer."},
    {NULL, NULL, 0, NULL},
};

PyTypeObject ETFormDocType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "etform.Doc",
    .tp_basicsize = sizeof(ETFormDocObject),
    .tp_dealloc = (destructor)Doc_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_doc = "Read-only structured terminal document.",
    .tp_traverse = (traverseproc)Doc_traverse,
    .tp_clear = (inquiry)Doc_clear,
    .tp_methods = Doc_methods,
    .tp_init = (initproc)Doc_init,
    .tp_new = Doc_new,
};
