#include "form_object.h"

#include <Python.h>

static PyModuleDef tform_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "_tform",
    .m_doc = "Native ncurses form runtime.",
    .m_size = -1,
};

PyMODINIT_FUNC PyInit__tform(void) {
    if (PyType_Ready(&TFormType) < 0) {
        return NULL;
    }
    if (PyType_Ready(&TFormTextType) < 0) {
        return NULL;
    }
    if (PyType_Ready(&TFormFilePickerType) < 0) {
        return NULL;
    }
    if (PyType_Ready(&TFormCalendarType) < 0) {
        return NULL;
    }
    if (PyType_Ready(&TFormClockType) < 0) {
        return NULL;
    }
    if (PyType_Ready(&TFormBrailleCanvasType) < 0) {
        return NULL;
    }
    if (PyType_Ready(&TFormImageType) < 0) {
        return NULL;
    }
    if (PyType_Ready(&TFormTableType) < 0) {
        return NULL;
    }

    PyObject *module = PyModule_Create(&tform_module);
    if (!module) {
        return NULL;
    }

    Py_INCREF(&TFormType);
    if (PyModule_AddObject(module, "Form", (PyObject *)&TFormType) < 0) {
        Py_DECREF(&TFormType);
        Py_DECREF(module);
        return NULL;
    }
    Py_INCREF(&TFormTextType);
    if (PyModule_AddObject(module, "Text", (PyObject *)&TFormTextType) < 0) {
        Py_DECREF(&TFormTextType);
        Py_DECREF(module);
        return NULL;
    }
    Py_INCREF(&TFormFilePickerType);
    if (PyModule_AddObject(module, "FilePicker", (PyObject *)&TFormFilePickerType) < 0) {
        Py_DECREF(&TFormFilePickerType);
        Py_DECREF(module);
        return NULL;
    }
    Py_INCREF(&TFormCalendarType);
    if (PyModule_AddObject(module, "Calendar", (PyObject *)&TFormCalendarType) < 0) {
        Py_DECREF(&TFormCalendarType);
        Py_DECREF(module);
        return NULL;
    }
    Py_INCREF(&TFormClockType);
    if (PyModule_AddObject(module, "Clock", (PyObject *)&TFormClockType) < 0) {
        Py_DECREF(&TFormClockType);
        Py_DECREF(module);
        return NULL;
    }
    Py_INCREF(&TFormBrailleCanvasType);
    if (PyModule_AddObject(module, "BrailleCanvas", (PyObject *)&TFormBrailleCanvasType) < 0) {
        Py_DECREF(&TFormBrailleCanvasType);
        Py_DECREF(module);
        return NULL;
    }
    Py_INCREF(&TFormImageType);
    if (PyModule_AddObject(module, "Image", (PyObject *)&TFormImageType) < 0) {
        Py_DECREF(&TFormImageType);
        Py_DECREF(module);
        return NULL;
    }
    Py_INCREF(&TFormTableType);
    if (PyModule_AddObject(module, "Table", (PyObject *)&TFormTableType) < 0) {
        Py_DECREF(&TFormTableType);
        Py_DECREF(module);
        return NULL;
    }
    return module;
}
