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

void tform_doc_item_free(TFormDocItem *item) {
    if (!item) return;
    if (item->type == TFORM_DOC_ITEM_STATIC) {
        tform_section_element_free(&item->element);
    } else if (item->type == TFORM_DOC_ITEM_PARAGRAPH ||
               item->type == TFORM_DOC_ITEM_HEADING ||
               item->type == TFORM_DOC_ITEM_SEPARATOR) {
        tform_text_style_free(&item->text);
    } else if (item->type == TFORM_DOC_ITEM_CODE_BLOCK) {
        free(item->code);
        free(item->lang);
    } else if (item->type == TFORM_DOC_ITEM_SECTION) {
        Py_XDECREF(item->section);
    }
    memset(item, 0, sizeof(*item));
}

static int section_ensure_capacity(TFormDocSectionObject *self, int needed) {
    if (self->item_capacity >= needed) return 0;
    int capacity = self->item_capacity ? self->item_capacity * 2 : 8;
    while (capacity < needed) capacity *= 2;
    TFormDocItem *items = realloc(self->items, (size_t)capacity * sizeof(*items));
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

static TFormDocItem *section_append_item(TFormDocSectionObject *self, TFormDocItemType type) {
    if (section_ensure_capacity(self, self->item_count + 1) < 0) return NULL;
    TFormDocItem *item = &self->items[self->item_count];
    memset(item, 0, sizeof(*item));
    item->type = type;
    self->item_count++;
    return item;
}

static int section_contains(
    TFormDocSectionObject *root,
    TFormDocSectionObject *target
) {
    if (root == target) return 1;
    for (int i = 0; i < root->item_count; i++) {
        TFormDocItem *item = &root->items[i];
        if (item->type == TFORM_DOC_ITEM_SECTION &&
            section_contains((TFormDocSectionObject *)item->section, target)) {
            return 1;
        }
    }
    return 0;
}

static PyObject *CodeBlock_new(PyTypeObject *type, PyObject *args, PyObject *kwargs) {
    (void)args;
    (void)kwargs;
    TFormCodeBlockObject *self = (TFormCodeBlockObject *)type->tp_alloc(type, 0);
    if (!self) return NULL;
    self->code = NULL;
    self->lang = NULL;
    self->background = 1;
    return (PyObject *)self;
}

static int CodeBlock_init(TFormCodeBlockObject *self, PyObject *args, PyObject *kwargs) {
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

static void CodeBlock_dealloc(TFormCodeBlockObject *self) {
    free(self->code);
    free(self->lang);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *CodeBlock_repr(TFormCodeBlockObject *self) {
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

static PyObject *CodeBlock_get_code(TFormCodeBlockObject *self, void *closure) {
    (void)closure;
    return PyUnicode_FromString(self->code ? self->code : "");
}

static PyObject *CodeBlock_get_lang(TFormCodeBlockObject *self, void *closure) {
    (void)closure;
    if (!self->lang) Py_RETURN_NONE;
    return PyUnicode_FromString(self->lang);
}

static PyObject *CodeBlock_get_background(TFormCodeBlockObject *self, void *closure) {
    (void)closure;
    return PyBool_FromLong(self->background);
}

static PyGetSetDef CodeBlock_getset[] = {
    {"code", (getter)CodeBlock_get_code, NULL, "Code text.", NULL},
    {"lang", (getter)CodeBlock_get_lang, NULL, "Language name.", NULL},
    {"background", (getter)CodeBlock_get_background, NULL, "Whether to draw a background.", NULL},
    {NULL, NULL, NULL, NULL, NULL},
};

PyTypeObject TFormCodeBlockType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "tform.CodeBlock",
    .tp_basicsize = sizeof(TFormCodeBlockObject),
    .tp_dealloc = (destructor)CodeBlock_dealloc,
    .tp_repr = (reprfunc)CodeBlock_repr,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_doc = "Read-only code block for Doc sections.",
    .tp_getset = CodeBlock_getset,
    .tp_init = (initproc)CodeBlock_init,
    .tp_new = CodeBlock_new,
};

static PyObject *Section_new(PyTypeObject *type, PyObject *args, PyObject *kwargs) {
    (void)args;
    (void)kwargs;
    TFormDocSectionObject *self =
        (TFormDocSectionObject *)type->tp_alloc(type, 0);
    if (!self) return NULL;
    self->title = NULL;
    self->items = NULL;
    self->item_count = 0;
    self->item_capacity = 0;
    self->boxed = 0;
    self->padding = TFORM_DEFAULT_PADDING;
    return (PyObject *)self;
}

static int Section_init(TFormDocSectionObject *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"title", "boxed", "padding", NULL};
    const char *title = NULL;
    int boxed = 0;
    PyObject *padding_obj = Py_None;
    int padding;
    if (!PyArg_ParseTupleAndKeywords(
            args, kwargs, "s|pO", kwlist, &title, &boxed, &padding_obj)) {
        return -1;
    }
    if (!*title) {
        PyErr_SetString(PyExc_ValueError, "section title must not be empty");
        return -1;
    }
    if (parse_nonnegative(
            padding_obj, TFORM_DEFAULT_PADDING, "padding", &padding) < 0) {
        return -1;
    }
    char *copy = doc_strdup(title);
    if (!copy) return -1;
    free(self->title);
    self->title = copy;
    self->boxed = boxed ? 1 : 0;
    self->padding = padding;
    return 0;
}

static int Section_traverse(TFormDocSectionObject *self, visitproc visit, void *arg) {
    for (int i = 0; i < self->item_count; i++) {
        if (self->items[i].type == TFORM_DOC_ITEM_SECTION) {
            Py_VISIT(self->items[i].section);
        }
    }
    return 0;
}

static int Section_clear(TFormDocSectionObject *self) {
    for (int i = 0; i < self->item_count; i++) {
        tform_doc_item_free(&self->items[i]);
    }
    self->item_count = 0;
    return 0;
}

static void Section_dealloc(TFormDocSectionObject *self) {
    PyObject_GC_UnTrack(self);
    Section_clear(self);
    free(self->items);
    free(self->title);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *Section_repr(TFormDocSectionObject *self) {
    return PyUnicode_FromFormat(
        "Section('%s', boxed=%s, padding=%d, items=%d)",
        self->title ? self->title : "",
        self->boxed ? "True" : "False",
        self->padding,
        self->item_count
    );
}

static PyObject *Section_add(TFormDocSectionObject *self, PyObject *item_obj) {
    if (PyObject_TypeCheck(item_obj, &TFormDocSectionType)) {
        PyErr_SetString(PyExc_TypeError, "use add_section() for nested sections");
        return NULL;
    }
    if (PyObject_TypeCheck(item_obj, &TFormCodeBlockType)) {
        PyErr_SetString(PyExc_TypeError, "use add_code_block() for code blocks");
        return NULL;
    }
    TFormDocItem *item = section_append_item(self, TFORM_DOC_ITEM_STATIC);
    if (!item) return NULL;
    if (tform_section_element_from_object(&item->element, item_obj) < 0) {
        self->item_count--;
        memset(item, 0, sizeof(*item));
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *Section_add_section(TFormDocSectionObject *self, PyObject *section_obj) {
    if (!PyObject_TypeCheck(section_obj, &TFormDocSectionType)) {
        PyErr_SetString(PyExc_TypeError, "section must be a Section");
        return NULL;
    }
    TFormDocSectionObject *child = (TFormDocSectionObject *)section_obj;
    if (section_contains(child, self)) {
        PyErr_SetString(PyExc_ValueError, "section hierarchy must not contain cycles");
        return NULL;
    }
    TFormDocItem *item = section_append_item(self, TFORM_DOC_ITEM_SECTION);
    if (!item) return NULL;
    Py_INCREF(section_obj);
    item->section = section_obj;
    Py_RETURN_NONE;
}

static PyObject *Section_add_heading(
    TFormDocSectionObject *self,
    PyObject *value,
    int level
) {
    TFormDocItem *item = section_append_item(self, TFORM_DOC_ITEM_HEADING);
    if (!item) return NULL;
    if (tform_text_style_from_object(value, 0, &item->text) < 0) {
        self->item_count--;
        memset(item, 0, sizeof(*item));
        return NULL;
    }
    item->heading_level = level;
    item->text.bold = 1;
    item->text.wrap_text = 1;
    if (item->text.color == TFORM_TEXT_COLOR_DEFAULT) {
        item->text.color = level == 1
            ? TFORM_TEXT_COLOR_CYAN
            : level == 2 ? TFORM_TEXT_COLOR_YELLOW : TFORM_TEXT_COLOR_GREEN;
    }
    Py_RETURN_NONE;
}

static PyObject *Section_add_h1(TFormDocSectionObject *self, PyObject *value) {
    return Section_add_heading(self, value, 1);
}

static PyObject *Section_add_h2(TFormDocSectionObject *self, PyObject *value) {
    return Section_add_heading(self, value, 2);
}

static PyObject *Section_add_h3(TFormDocSectionObject *self, PyObject *value) {
    return Section_add_heading(self, value, 3);
}

static PyObject *Section_add_p(TFormDocSectionObject *self, PyObject *value) {
    TFormDocItem *item = section_append_item(self, TFORM_DOC_ITEM_PARAGRAPH);
    if (!item) return NULL;
    if (tform_text_style_from_object(value, 1, &item->text) < 0) {
        self->item_count--;
        memset(item, 0, sizeof(*item));
        return NULL;
    }
    item->text.wrap_text = 1;
    if (item->text.color == TFORM_TEXT_COLOR_DEFAULT) {
        item->text.color = TFORM_TEXT_COLOR_WHITE;
    }
    Py_RETURN_NONE;
}

static int copy_code_block(TFormDocItem *item, TFormCodeBlockObject *block) {
    item->code = doc_strdup(block->code);
    item->lang = block->lang ? doc_strdup(block->lang) : NULL;
    item->code_background = block->background;
    if (!item->code || (block->lang && !item->lang)) {
        free(item->code);
        free(item->lang);
        item->code = NULL;
        item->lang = NULL;
        return -1;
    }
    return 0;
}

static PyObject *Section_add_code_block(
    TFormDocSectionObject *self,
    PyObject *args,
    PyObject *kwargs
) {
    static char *kwlist[] = {"code_or_block", "lang", NULL};
    PyObject *value = NULL;
    PyObject *lang_obj = Py_None;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|O", kwlist, &value, &lang_obj)) {
        return NULL;
    }
    TFormDocItem *item = section_append_item(self, TFORM_DOC_ITEM_CODE_BLOCK);
    if (!item) return NULL;
    if (PyObject_TypeCheck(value, &TFormCodeBlockType)) {
        if (lang_obj != Py_None) {
            self->item_count--;
            PyErr_SetString(PyExc_TypeError, "lang cannot be provided with a CodeBlock");
            return NULL;
        }
        if (copy_code_block(item, (TFormCodeBlockObject *)value) < 0) {
            self->item_count--;
            memset(item, 0, sizeof(*item));
            return NULL;
        }
    } else {
        const char *code = PyUnicode_AsUTF8(value);
        const char *lang = NULL;
        if (!code) {
            self->item_count--;
            PyErr_SetString(PyExc_TypeError, "code_or_block must be str or CodeBlock");
            return NULL;
        }
        if (lang_obj != Py_None) {
            lang = PyUnicode_AsUTF8(lang_obj);
            if (!lang) {
                self->item_count--;
                PyErr_SetString(PyExc_TypeError, "lang must be str or None");
                return NULL;
            }
        }
        item->code = doc_strdup(code);
        item->lang = lang ? doc_strdup(lang) : NULL;
        item->code_background = 1;
        if (!item->code || (lang && !item->lang)) {
            self->item_count--;
            tform_doc_item_free(item);
            return NULL;
        }
    }
    Py_RETURN_NONE;
}

static PyObject *Section_add_separator(
    TFormDocSectionObject *self,
    PyObject *args,
    PyObject *kwargs
) {
    static char *kwlist[] = {"text", NULL};
    PyObject *text = Py_None;
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|O", kwlist, &text)) {
        return NULL;
    }
    TFormDocItem *item = section_append_item(self, TFORM_DOC_ITEM_SEPARATOR);
    if (!item) return NULL;
    PyObject *value = text == Py_None ? PyUnicode_FromString("") : Py_NewRef(text);
    if (!value) {
        self->item_count--;
        return NULL;
    }
    int result = tform_text_style_from_object(value, 1, &item->text);
    Py_DECREF(value);
    if (result < 0) {
        self->item_count--;
        memset(item, 0, sizeof(*item));
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *Section_get_title(TFormDocSectionObject *self, void *closure) {
    (void)closure;
    return PyUnicode_FromString(self->title ? self->title : "");
}

static PyObject *Section_get_boxed(TFormDocSectionObject *self, void *closure) {
    (void)closure;
    return PyBool_FromLong(self->boxed);
}

static PyObject *Section_get_padding(TFormDocSectionObject *self, void *closure) {
    (void)closure;
    return PyLong_FromLong(self->padding);
}

static PyMethodDef Section_methods[] = {
    {"add", (PyCFunction)Section_add, METH_O, "Add reusable static content."},
    {"add_section", (PyCFunction)Section_add_section, METH_O, "Add a nested section."},
    {"add_h1", (PyCFunction)Section_add_h1, METH_O, "Add a level-one heading."},
    {"add_h2", (PyCFunction)Section_add_h2, METH_O, "Add a level-two heading."},
    {"add_h3", (PyCFunction)Section_add_h3, METH_O, "Add a level-three heading."},
    {"add_p", (PyCFunction)Section_add_p, METH_O, "Add a wrapped paragraph."},
    {"add_code_block", (PyCFunction)Section_add_code_block, METH_VARARGS | METH_KEYWORDS, "Add a code block."},
    {"add_separator", (PyCFunction)Section_add_separator, METH_VARARGS | METH_KEYWORDS, "Add a separator."},
    {NULL, NULL, 0, NULL},
};

static PyGetSetDef Section_getset[] = {
    {"title", (getter)Section_get_title, NULL, "Section title.", NULL},
    {"boxed", (getter)Section_get_boxed, NULL, "Whether the section is boxed.", NULL},
    {"padding", (getter)Section_get_padding, NULL, "Section padding.", NULL},
    {NULL, NULL, NULL, NULL, NULL},
};

PyTypeObject TFormDocSectionType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "tform.Section",
    .tp_basicsize = sizeof(TFormDocSectionObject),
    .tp_dealloc = (destructor)Section_dealloc,
    .tp_repr = (reprfunc)Section_repr,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_doc = "Hierarchical read-only document section.",
    .tp_traverse = (traverseproc)Section_traverse,
    .tp_clear = (inquiry)Section_clear,
    .tp_methods = Section_methods,
    .tp_getset = Section_getset,
    .tp_init = (initproc)Section_init,
    .tp_new = Section_new,
};

static PyObject *Doc_new(PyTypeObject *type, PyObject *args, PyObject *kwargs) {
    (void)args;
    (void)kwargs;
    TFormDocObject *self = (TFormDocObject *)type->tp_alloc(type, 0);
    if (!self) return NULL;
    self->title = NULL;
    self->sections = NULL;
    self->section_count = 0;
    self->section_capacity = 0;
    self->margin_left = 0;
    self->margin_right = 0;
    self->margin_top = 0;
    self->margin_bottom = 0;
    self->syntax_highlight = 1;
    self->show_border = 1;
    return (PyObject *)self;
}

static int Doc_init(TFormDocObject *self, PyObject *args, PyObject *kwargs) {
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

static int Doc_traverse(TFormDocObject *self, visitproc visit, void *arg) {
    for (int i = 0; i < self->section_count; i++) {
        Py_VISIT(self->sections[i]);
    }
    return 0;
}

static int Doc_clear(TFormDocObject *self) {
    for (int i = 0; i < self->section_count; i++) {
        Py_CLEAR(self->sections[i]);
    }
    self->section_count = 0;
    return 0;
}

static void Doc_dealloc(TFormDocObject *self) {
    PyObject_GC_UnTrack(self);
    Doc_clear(self);
    free(self->sections);
    free(self->title);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static int doc_ensure_capacity(TFormDocObject *self, int needed) {
    if (self->section_capacity >= needed) return 0;
    int capacity = self->section_capacity ? self->section_capacity * 2 : 4;
    while (capacity < needed) capacity *= 2;
    PyObject **sections = realloc(
        self->sections, (size_t)capacity * sizeof(*sections)
    );
    if (!sections) {
        PyErr_NoMemory();
        return -1;
    }
    self->sections = sections;
    self->section_capacity = capacity;
    return 0;
}

static PyObject *Doc_add_section(TFormDocObject *self, PyObject *section) {
    if (!PyObject_TypeCheck(section, &TFormDocSectionType)) {
        PyErr_SetString(PyExc_TypeError, "section must be a Section");
        return NULL;
    }
    if (doc_ensure_capacity(self, self->section_count + 1) < 0) return NULL;
    Py_INCREF(section);
    self->sections[self->section_count++] = section;
    Py_RETURN_NONE;
}

static PyObject *Doc_run(TFormDocObject *self, PyObject *Py_UNUSED(ignored)) {
    if (self->section_count <= 0) {
        PyErr_SetString(PyExc_ValueError, "document must contain at least one section");
        return NULL;
    }
    if (tform_run_doc(self) < 0) {
        PyErr_SetString(PyExc_RuntimeError, "could not run document");
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyMethodDef Doc_methods[] = {
    {"add_section", (PyCFunction)Doc_add_section, METH_O, "Add a root document section."},
    {"run", (PyCFunction)Doc_run, METH_NOARGS, "Open the read-only document viewer."},
    {NULL, NULL, 0, NULL},
};

PyTypeObject TFormDocType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "tform.Doc",
    .tp_basicsize = sizeof(TFormDocObject),
    .tp_dealloc = (destructor)Doc_dealloc,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,
    .tp_doc = "Read-only structured terminal document.",
    .tp_traverse = (traverseproc)Doc_traverse,
    .tp_clear = (inquiry)Doc_clear,
    .tp_methods = Doc_methods,
    .tp_init = (initproc)Doc_init,
    .tp_new = Doc_new,
};
