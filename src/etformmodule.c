#include "form_object.h"
#include "doc_object.h"

#include <Python.h>

static PyModuleDef etform_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "_etform",
    .m_doc = "Native ncurses form runtime.",
    .m_size = -1,
};

PyMODINIT_FUNC PyInit__etform(void) {
    if (PyType_Ready(&ETFormType) < 0) {
        return NULL;
    }
    if (PyType_Ready(&ETFormTextType) < 0) {
        return NULL;
    }
    if (PyType_Ready(&ETFormFilePickerType) < 0) {
        return NULL;
    }
    if (PyType_Ready(&ETFormCalendarType) < 0) {
        return NULL;
    }
    if (PyType_Ready(&ETFormClockType) < 0) {
        return NULL;
    }
    if (PyType_Ready(&ETFormBrailleCanvasType) < 0) {
        return NULL;
    }
    if (PyType_Ready(&ETFormImageType) < 0) {
        return NULL;
    }
    if (PyType_Ready(&ETFormTableType) < 0) {
        return NULL;
    }
    if (PyType_Ready(&ETFormITableType) < 0) {
        return NULL;
    }
    if (PyType_Ready(&ETFormCodeBlockType) < 0) {
        return NULL;
    }
    if (PyType_Ready(&ETFormParagraphType) < 0) {
        return NULL;
    }
    if (PyType_Ready(&ETFormOrderedListType) < 0) {
        return NULL;
    }
    if (PyType_Ready(&ETFormUnorderedListType) < 0) {
        return NULL;
    }
    if (PyType_Ready(&ETFormDocGroupType) < 0) {
        return NULL;
    }
    if (PyType_Ready(&ETFormDocType) < 0) {
        return NULL;
    }

    PyObject *module = PyModule_Create(&etform_module);
    if (!module) {
        return NULL;
    }

    Py_INCREF(&ETFormType);
    if (PyModule_AddObject(module, "Form", (PyObject *)&ETFormType) < 0) {
        Py_DECREF(&ETFormType);
        Py_DECREF(module);
        return NULL;
    }
    Py_INCREF(&ETFormTextType);
    if (PyModule_AddObject(module, "Text", (PyObject *)&ETFormTextType) < 0) {
        Py_DECREF(&ETFormTextType);
        Py_DECREF(module);
        return NULL;
    }
    Py_INCREF(&ETFormFilePickerType);
    if (PyModule_AddObject(module, "FilePicker", (PyObject *)&ETFormFilePickerType) < 0) {
        Py_DECREF(&ETFormFilePickerType);
        Py_DECREF(module);
        return NULL;
    }
    Py_INCREF(&ETFormCalendarType);
    if (PyModule_AddObject(module, "Calendar", (PyObject *)&ETFormCalendarType) < 0) {
        Py_DECREF(&ETFormCalendarType);
        Py_DECREF(module);
        return NULL;
    }
    Py_INCREF(&ETFormClockType);
    if (PyModule_AddObject(module, "Clock", (PyObject *)&ETFormClockType) < 0) {
        Py_DECREF(&ETFormClockType);
        Py_DECREF(module);
        return NULL;
    }
    Py_INCREF(&ETFormBrailleCanvasType);
    if (PyModule_AddObject(module, "BrailleCanvas", (PyObject *)&ETFormBrailleCanvasType) < 0) {
        Py_DECREF(&ETFormBrailleCanvasType);
        Py_DECREF(module);
        return NULL;
    }
    Py_INCREF(&ETFormImageType);
    if (PyModule_AddObject(module, "Image", (PyObject *)&ETFormImageType) < 0) {
        Py_DECREF(&ETFormImageType);
        Py_DECREF(module);
        return NULL;
    }
    Py_INCREF(&ETFormTableType);
    if (PyModule_AddObject(module, "Table", (PyObject *)&ETFormTableType) < 0) {
        Py_DECREF(&ETFormTableType);
        Py_DECREF(module);
        return NULL;
    }
    Py_INCREF(&ETFormTableType);
    if (PyModule_AddObject(module, "STable", (PyObject *)&ETFormTableType) < 0) {
        Py_DECREF(&ETFormTableType);
        Py_DECREF(module);
        return NULL;
    }
    Py_INCREF(&ETFormITableType);
    if (PyModule_AddObject(module, "ITable", (PyObject *)&ETFormITableType) < 0) {
        Py_DECREF(&ETFormITableType);
        Py_DECREF(module);
        return NULL;
    }
    Py_INCREF(&ETFormCodeBlockType);
    if (PyModule_AddObject(module, "CodeBlock", (PyObject *)&ETFormCodeBlockType) < 0) {
        Py_DECREF(&ETFormCodeBlockType);
        Py_DECREF(module);
        return NULL;
    }
    Py_INCREF(&ETFormParagraphType);
    if (PyModule_AddObject(module, "Paragraph", (PyObject *)&ETFormParagraphType) < 0) {
        Py_DECREF(&ETFormParagraphType);
        Py_DECREF(module);
        return NULL;
    }
    Py_INCREF(&ETFormOrderedListType);
    if (PyModule_AddObject(module, "OrderedList", (PyObject *)&ETFormOrderedListType) < 0) {
        Py_DECREF(&ETFormOrderedListType);
        Py_DECREF(module);
        return NULL;
    }
    Py_INCREF(&ETFormUnorderedListType);
    if (PyModule_AddObject(module, "UnorderedList", (PyObject *)&ETFormUnorderedListType) < 0) {
        Py_DECREF(&ETFormUnorderedListType);
        Py_DECREF(module);
        return NULL;
    }
    Py_INCREF(&ETFormDocGroupType);
    if (PyModule_AddObject(module, "Group", (PyObject *)&ETFormDocGroupType) < 0) {
        Py_DECREF(&ETFormDocGroupType);
        Py_DECREF(module);
        return NULL;
    }
    Py_INCREF(&ETFormDocType);
    if (PyModule_AddObject(module, "Doc", (PyObject *)&ETFormDocType) < 0) {
        Py_DECREF(&ETFormDocType);
        Py_DECREF(module);
        return NULL;
    }
    return module;
}
