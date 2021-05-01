/* Copyright (C) 2007 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#include <Python.h>
#include <structmember.h>
#include <bytearrayobject.h>

#include <strings.h>

#include <string>
#include <memory>

#include "log.h"
#include "rcldoc.h"
#include "internfile.h"
#include "rclconfig.h"
#include "rclinit.h"

#include "pyrecoll.h"

using namespace std;

// Imported from pyrecoll
static PyObject *recoll_DocType;

//////////////////////////////////////////////////////////////////////
/// Extractor object code
typedef struct {
    PyObject_HEAD
    /* Type-specific fields go here. */
    FileInterner *xtr;
    std::shared_ptr<RclConfig> rclconfig;
    recoll_DocObject *docobject;
} rclx_ExtractorObject;

static void 
Extractor_dealloc(rclx_ExtractorObject *self)
{
    LOGDEB("Extractor_dealloc\n" );
    if (self->docobject) {
        Py_DECREF(&self->docobject);
    }
    self->rclconfig.reset();
    delete self->xtr;
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject *
Extractor_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    LOGDEB("Extractor_new\n" );
    rclx_ExtractorObject *self = 
	(rclx_ExtractorObject *)type->tp_alloc(type, 0);
    if (self == 0) 
	return 0;
    self->xtr = 0;
    self->docobject = 0;
    return (PyObject *)self;
}

static int
Extractor_init(rclx_ExtractorObject *self, PyObject *args, PyObject *kwargs)
{
    LOGDEB("Extractor_init\n" );
    static const char* kwlist[] = {"doc", NULL};
    PyObject *pdobj;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O!", (char**)kwlist, 
				     recoll_DocType, &pdobj))
	return -1;
    recoll_DocObject *dobj = (recoll_DocObject *)pdobj;
    if (dobj->doc == 0) {
        PyErr_SetString(PyExc_AttributeError, "Null Doc ?");
	return -1;
    }
    self->docobject = dobj;
    Py_INCREF(dobj);

    self->rclconfig = dobj->rclconfig;
    self->xtr = new FileInterner(*dobj->doc, self->rclconfig.get(), 
				 FileInterner::FIF_forPreview);
    return 0;
}

PyDoc_STRVAR(doc_Extractor_textextract,
"textextract(ipath)\n"
"Extract document defined by ipath and return a doc object. The doc.text\n"
"field has the document text as either text/plain or text/html\n"
"according to doc.mimetype.\n"
);

static PyObject *
Extractor_textextract(rclx_ExtractorObject* self, PyObject *args, 
		      PyObject *kwargs)
{
    LOGDEB("Extractor_textextract\n" );
    static const char* kwlist[] = {"ipath", NULL};
    char *sipath = 0;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "es:Extractor_textextract",
				     (char**)kwlist, 
				     "utf-8", &sipath))
	return 0;

    string ipath(sipath);
    PyMem_Free(sipath);

    if (self->xtr == 0) {
        PyErr_SetString(PyExc_AttributeError, "extract: null object");
	return 0;
    }
    /* Call the doc class object to create a new doc. */
    recoll_DocObject *result = 
       (recoll_DocObject *)PyObject_CallObject((PyObject *)recoll_DocType, 0);
    if (!result) {
        PyErr_SetString(PyExc_AttributeError, "extract: doc create failed");
	return 0;
    }
    FileInterner::Status status = self->xtr->internfile(*(result->doc), ipath);
    if (status != FileInterner::FIDone && status != FileInterner::FIAgain) {
        PyErr_SetString(PyExc_AttributeError, "internfile failure");
        return 0;
    }

    string html = self->xtr->get_html();
    if (!html.empty()) {
	result->doc->text = html;
	result->doc->mimetype = "text/html";
    }

    // Is this actually needed ? Useful for url which is also formatted .
    Rcl::Doc *doc = result->doc;
    printableUrl(self->rclconfig->getDefCharset(), doc->url, 
		 doc->meta[Rcl::Doc::keyurl]);
    doc->meta[Rcl::Doc::keytp] = doc->mimetype;
    doc->meta[Rcl::Doc::keyipt] = doc->ipath;
    doc->meta[Rcl::Doc::keyfs] = doc->fbytes;
    doc->meta[Rcl::Doc::keyds] = doc->dbytes;
    return (PyObject *)result;
}

PyDoc_STRVAR(doc_Extractor_idoctofile,
"idoctofile(ipath='', mimetype='', ofilename='')\n"
"Extract document defined by ipath into a file, in its native format.\n"
);
static PyObject *
Extractor_idoctofile(rclx_ExtractorObject* self, PyObject *args, 
		     PyObject *kwargs)
{
    LOGDEB("Extractor_idoctofile\n" );
    static const char* kwlist[] = {"ipath", "mimetype", "ofilename", NULL};
    char *sipath = 0;
    char *smt = 0;
    char *soutfile = 0; // no freeing
    if (!PyArg_ParseTupleAndKeywords(args,kwargs, "eses|s:Extractor_idoctofile",
				     (char**)kwlist, 
				     "utf-8", &sipath,
				     "utf-8", &smt,
				     &soutfile))
	return 0;

    string ipath(sipath);
    PyMem_Free(sipath);
    string mimetype(smt);
    PyMem_Free(smt);
    string outfile;
    if (soutfile && *soutfile)
	outfile.assign(soutfile); 
    
    if (self->xtr == 0) {
        PyErr_SetString(PyExc_AttributeError, "idoctofile: null object");
	return 0;
    }

    // If ipath is empty and we want the original mimetype, we can't
    // use FileInterner::internToFile() because the first conversion
    // was performed by the FileInterner constructor, so that we can't
    // reach the original object this way. Instead, if the data comes
    // from a file (m_fn set), we just copy it, else, we call
    // idoctofile, which will call topdoctofile (and re-fetch the
    // data, yes, wastefull)
    TempFile temp;
    bool status = false;
    LOGDEB("Extractor_idoctofile: ipath [" << ipath << "] mimetype [" <<
           mimetype << "] doc mimetype [" << self->docobject->doc->mimetype <<
           "\n");
    if (ipath.empty() && !mimetype.compare(self->docobject->doc->mimetype)) {
        status = FileInterner::idocToFile(temp, outfile, self->rclconfig.get(),
                                               *self->docobject->doc);
    } else {
        self->xtr->setTargetMType(mimetype);
        status = self->xtr->interntofile(temp, outfile, ipath, mimetype);
    }
    if (!status) {
        PyErr_SetString(PyExc_AttributeError, "interntofile failure");
        return 0;
    }
    if (outfile.empty())
	temp.setnoremove(1);
    PyObject *result = outfile.empty() ? PyBytes_FromString(temp.filename()) :
	PyBytes_FromString(outfile.c_str());
    return (PyObject *)result;
}

static PyMethodDef Extractor_methods[] = {
    {"textextract", (PyCFunction)Extractor_textextract, 
     METH_VARARGS|METH_KEYWORDS, doc_Extractor_textextract},
    {"idoctofile", (PyCFunction)Extractor_idoctofile, 
     METH_VARARGS|METH_KEYWORDS, doc_Extractor_idoctofile},
    {NULL}  /* Sentinel */
};

PyDoc_STRVAR(doc_ExtractorObject,
"Extractor()\n"
"\n"
"An Extractor object can extract data from a native simple or compound\n"
"object.\n"
);
static PyTypeObject rclx_ExtractorType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "rclextract.Extractor",             /*tp_name*/
    sizeof(rclx_ExtractorObject), /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    (destructor)Extractor_dealloc,    /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,  /*tp_flags*/
    doc_ExtractorObject,      /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    Extractor_methods,        /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)Extractor_init, /* tp_init */
    0,                         /* tp_alloc */
    Extractor_new,            /* tp_new */
};

///////////////////////////////////// Module-level stuff
static PyMethodDef rclextract_methods[] = {
    {NULL, NULL, 0, NULL}        /* Sentinel */
};
PyDoc_STRVAR(rclx_doc_string,
	     "This is an interface to the Recoll text extraction features.");

struct module_state {
    PyObject *error;
};

#if PY_MAJOR_VERSION >= 3
#define GETSTATE(m) ((struct module_state*)PyModule_GetState(m))
#else
#define GETSTATE(m) (&_state)
static struct module_state _state;
#endif

#if PY_MAJOR_VERSION >= 3
static int rclextract_traverse(PyObject *m, visitproc visit, void *arg) {
    Py_VISIT(GETSTATE(m)->error);
    return 0;
}

static int rclextract_clear(PyObject *m) {
    Py_CLEAR(GETSTATE(m)->error);
    return 0;
}

static struct PyModuleDef moduledef = {
        PyModuleDef_HEAD_INIT,
        "rclextract",
        NULL,
        sizeof(struct module_state),
        rclextract_methods,
        NULL,
        rclextract_traverse,
        rclextract_clear,
        NULL
};

#define INITERROR return NULL

extern "C" PyObject *
PyInit_rclextract(void)

#else
#define INITERROR return
PyMODINIT_FUNC
initrclextract(void)
#endif
{
    // We run recollinit. It's responsible for initializing some static data
    // which is distinct from pyrecoll's as we're separately dlopened.
    // The rclconfig object is not used, we'll get the config
    // data from the objects out of the recoll module.
    // Unfortunately, as we're not getting the actual config directory
    // from pyrecoll (we could, through a capsule), this needs at
    // least an empty default configuration directory to work.
    string reason;
    RclConfig *rclconfig = recollinit(RCLINIT_PYTHON, 0, 0, reason, 0);
    if (rclconfig == 0) {
	PyErr_SetString(PyExc_EnvironmentError, reason.c_str());
	INITERROR;
    } else {
        delete rclconfig;
    }
        
#if PY_MAJOR_VERSION >= 3
    PyObject *module = PyModule_Create(&moduledef);
#else
    PyObject *module = Py_InitModule("rclextract", rclextract_methods);
#endif
    if (module == NULL)
        INITERROR;

    struct module_state *st = GETSTATE(module);
    // The first parameter is a char *. Hopefully we don't initialize
    // modules too often...
    st->error = PyErr_NewException(strdup("rclextract.Error"), NULL, NULL);
    if (st->error == NULL) {
        Py_DECREF(module);
        INITERROR;
    }

    PyModule_AddStringConstant(module, "__doc__", rclx_doc_string);

    if (PyType_Ready(&rclx_ExtractorType) < 0)
        INITERROR;
    Py_INCREF(&rclx_ExtractorType);
    PyModule_AddObject(module, "Extractor", (PyObject *)&rclx_ExtractorType);

#if PY_MAJOR_VERSION >= 3 || (PY_MAJOR_VERSION >= 2 && PY_MINOR_VERSION >= 7)
    recoll_DocType = (PyObject*)PyCapsule_Import(PYRECOLL_PACKAGE "recoll.doctypeptr", 0);
#else
    PyObject *module1 = PyImport_ImportModule(PYRECOLL_PACKAGE "recoll");
    if (module1 != NULL) {
        PyObject *cobject = PyObject_GetAttrString(module1, "doctypeptr");
        if (cobject == NULL)
            INITERROR;
        if (PyCObject_Check(cobject))
            recoll_DocType = (PyObject*)PyCObject_AsVoidPtr(cobject);
        Py_DECREF(cobject);
    }
#endif

#if PY_MAJOR_VERSION >= 3
    return module;
#endif
}

