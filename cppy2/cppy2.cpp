#include "cppy2.hpp"

#ifdef QT_CORE_LIB
 #include <QFile>
 #include <QStringList>
 #include <QList>
 #include <QStringList>
 #include <QDebug>
 #include <QFile>
 #include <QFileInfo>
#endif

#include <cassert>
#include <cstdlib>
#include <vector>

namespace cppy2 {


PythonVM::PythonVM(const std::string& programName) {

  setenv("PYTHONDONTWRITEBYTECODE", "1", 0);

#ifdef _WIN32
  // force utf-8 on windows
  setenv("PYTHONIOENCODING", "UTF-8");
#endif

  Py_SetProgramName(const_cast<char*>(programName.c_str()));

  // create CPython instance without registering signal handlers
  Py_InitializeEx(0);

  // initialize GIL
  PyEval_InitThreads();
}

PythonVM::~PythonVM() {
  if (!PyImport_AddModule("dummy_threading")) {
    PyErr_Clear();
  }
  Py_Finalize();
}


void setArgv(const std::list<std::string>& argv) {
  GILLocker lock;

  std::vector<const char*> cargv;
  for (std::list<std::string>::const_iterator it = argv.begin(); it != argv.end(); ++it) {
    cargv.push_back(it->data());
  }
  PySys_SetArgvEx(argv.size(), const_cast<char**>(&cargv[0]), 0);
}


Var createClassInstance(const QString& callable) {
  GILLocker lock;
  Var instance;

  instance.newRef(call(lookupCallable(getMainModule(), callable)));
  rethrowPythonException();
  if (instance.none()) {
    throw PythonException(QString("error instantiating '%1': %2").arg(callable).arg(getErrorObject().toString()));
  }
  return instance;
}


void appendToSysPath(const QStringList& paths) {
  GILLocker lock;

  Var sys = import("sys");
  List sysPath(lookupObject(sys, "path"));
  foreach (const QString& path, paths) {
    Var pyPath(convert(path));
    if (!sysPath.contains(pyPath)) {
      // append into the 'sys.path'
      sysPath.append(pyPath);
    }
  }
}

void interrupt() {
  PyErr_SetInterrupt();
}


Var exec(const char* pythonScript) {
  GILLocker lock;
  PyObject* mainDict = getMainDict();
  Var result;

  result.newRef(PyRun_String(pythonScript, Py_file_input, mainDict, mainDict));
  if (result.data() == NULL) {
    rethrowPythonException();
  }
  return result;
}


Var exec(const QString& pythonScript) {
  ScopedGILLock lock;

  // encode unicode QString to utf8
  QString script = "# -*- coding: utf-8 -*-\n";
  script += pythonScript;
  return exec(script.toUtf8().constData());
}


Var execScriptFile(const QString& path) {
  QString script;
  QFile file(path);
  if (file.open(QIODevice::ReadOnly)) {
    script = file.readAll();
  } else {
    throw PythonException(QObject::tr("no file: %1").arg(path));
  }
  return exec(script);
}


bool error() {
  GILLocker lock;
  return Py_IsInitialized() && PyErr_Occurred();
}


void rethrowPythonException() {
  if (error()) {
    const PyExceptionData excData = getErrorObject(true);
    throw PythonException(excData);
  }
}


PyExceptionData getErrorObject(const bool clearError) {
  GILLocker lock;
  QString exceptionType;
  QString exceptionMessage;
  QStringList exceptionTrace;
  if (PyErr_Occurred()) {
    // get error context
    PyObject* excType = NULL;
    PyObject* excValue = NULL;
    PyObject* excTraceback = NULL;
    PyErr_Fetch(&excType, &excValue, &excTraceback);
    PyErr_NormalizeException(&excType, &excValue, &excTraceback);

    // get traceback module
    PyObject* name = PyString_FromString("traceback");
    PyObject* tracebackModule = PyImport_Import(name);
    Py_DECREF(name);

    // write text type of exception
    PyObject* temp = PyObject_Str(excType);
    if (temp != NULL) {
      exceptionType = PyString_AsString(temp);
      Py_DECREF(temp);
    }

    // write text message of exception
    temp = PyObject_Str(excValue);
    if (temp != NULL) {
      exceptionMessage = PyString_AsString(temp);
      Py_DECREF(temp);
    }

    if (excTraceback != NULL && tracebackModule != NULL) {
      // get traceback.format_tb() function ptr
      PyObject* tbDict = PyModule_GetDict(tracebackModule);
      PyObject* format_tbFunc = PyDict_GetItemString(tbDict, "format_tb");
      if (format_tbFunc && PyCallable_Check(format_tbFunc)) {
        // build argument
        PyObject* excTbTupleArg = PyTuple_New(1);
        PyTuple_SetItem(excTbTupleArg, 0, excTraceback);
        Py_INCREF(excTraceback); // because PyTuple_SetItem() steals reference
        // call traceback.format_tb(excTraceback)
        PyObject* list = PyObject_CallObject(format_tbFunc, excTbTupleArg);
        if (list != NULL) {
          // parse list and extract traceback text lines
          const int len = PyList_Size(list);
          for (int i = 0; i < len; i++) {
            PyObject* tt = PyList_GetItem(list, i);
            PyObject* t = Py_BuildValue("(O)", tt);
            char* buffer = NULL;
            if (PyArg_ParseTuple(t, "s", &buffer)) {
              exceptionTrace.append(buffer);
            }
            Py_XDECREF(t);
          }
          Py_DECREF(list);
        }
        Py_XDECREF(excTbTupleArg);
      }
    }
    Py_XDECREF(tracebackModule);

    if (clearError) {
      Py_XDECREF(excType);
      Py_XDECREF(excValue);
      Py_XDECREF(excTraceback);
    } else {
      PyErr_Restore(excType, excValue, excTraceback);
    }
  }
  return PyExceptionData(exceptionType, exceptionMessage, exceptionTrace);
}


PyObject* convert(const int& value) {
  PyObject* o = PyInt_FromLong(value);
  assert(o);
  return o;
}


PyObject* convert(const double& value) {
  PyObject* o = PyFloat_FromDouble(value);
  assert(o);
  return o;
}


PyObject* convert(const char* value) {
  PyObject* o = PyString_FromString(value);
  return o;
}

PyObject* convert(const std::wstring& value) {
  PyObject* o = PyUnicode_FromWideChar(value.data(), value.size());
  return o;
}


PyObject* convert(const QString& value) {
#ifdef _MSC_VER
  std::wstring buffer(reinterpret_cast<const wchar_t *>(value.utf16()), value.length());
  PyObject* pyObj = PyUnicode_FromWideChar(buffer.data(), buffer.length());
#else
  QVector<wchar_t> buffer(value.length());
  value.toWCharArray(buffer.data());
  PyObject* pyObj = PyUnicode_FromWideChar(buffer.data(), buffer.size());
#endif
  return pyObj;
}


void extract(PyObject* o, QString& value) {
  Var str(o);
  if (!PyString_Check(o)) {
    // try cast to string
    str.newRef(PyObject_Str(o));
    if (!str.data()) {
      throw PythonException("variable has no string representation");
    }
  }
  value = PyString_AsString(str);
}


void extract(PyObject* o, double& value) {
  if (PyFloat_Check(o)) {
    value = PyFloat_AsDouble(o);
  } else {
    throw PythonException("variable is not a real");
  }
}


void extract(PyObject* o, int& value) {
  if (PyInt_Check(o)) {
    value = PyInt_AsLong(o);
  } else {
    throw PythonException("variable is not an int");
  }
}


QString Var::toString() const {
  return toString(_o);
}


QString Var::toString(PyObject* val) {
  assert(val);
  QString result;
  QTextStream stream(&result);
  // try str() operator
  PyObject* str = PyObject_Str(val);
  if (!str) {
    // try repr() operator
    str = PyObject_Repr(val);
  }
  if (str) {
    stream << PyString_AsString(str);
  } else {
    stream << "< type='" << val->ob_type->tp_name << "' has no string representation >";
  }
  return result;
}


Var::Type Var::type() const {
  if (PyInt_Check(_o)) {
    return INT;
  } else if (PyLong_Check(_o)) {
    return LONG;
  } else if (PyFloat_Check(_o)) {
    return FLOAT;
  } else if (PyString_Check(_o)) {
    return STRING;
  } else if (PyTuple_Check(_o)) {
    return TUPLE;
  } else if (PyDict_Check(_o)) {
    return DICT;
  } else if (PyList_Check(_o)) {
    return LIST;
  } else if (PyBool_Check(_o)) {
    return BOOL;
  }
#ifdef NPY_NDARRAYOBJECT_H
  else if (PyArray_Check(_o)) {
    return TYPE_NUMPY_NDARRAY;
  }
#endif
  else if (PyModule_Check(_o)) {
    return MODULE;
  } else {
    return UNKNOWN;
  }
}


Var import(const char* moduleName, PyObject* globals, PyObject* locals) {
  Var module;
  module.newRef(PyImport_ImportModuleEx(const_cast<char*>(moduleName), globals, locals, NULL));
  if (module.null()) {
    const PyExceptionData excData = getErrorObject();
    throw PythonException(excData);
  }
  return module;
}


Var lookupObject(PyObject* module, const QString& name) {
  const QStringList items = name.split('.');
  Var p(module);
  // Var prev;  // (1) cause refcount bug
  QByteArray itemName;
  for (QStringList::ConstIterator it = items.begin(); it != items.end() && !p.null(); ++it) {

    // prev = p; // (2) cause refcount bug
    itemName = (*it).toLocal8Bit();
    if (PyDict_Check(p)) {
      p = Var(PyDict_GetItemString(p, itemName.data()));
    } else {
      // PyObject_GetAttrString returns new reference
      p.newRef(PyObject_GetAttrString(p, itemName.data()));
    }

    if (p.null()) {
      throw PythonException(QString("lookup %1 failed: no item %2").arg(name).arg(itemName.data()));
    }
  }
  return p;
}


Var lookupCallable(PyObject* module, const QString& name) {
  Var p = lookupObject(module, name);

  if (!PyCallable_Check(p)) {
    throw PythonException(QString("PyObject %1 is not callable").arg(name));
  }

  return p;
}


PyObject* call(PyObject* callable, const arguments& args) {
  assert(callable);
  if (!PyCallable_Check(callable)) {
    throw PythonException(QString("PyObject %1 is not callable").arg(Var(callable).toString()));
  }

  PyObject* result = NULL;
  Var argsTuple;
  const int argsCount = args.size();
  if (argsCount > 0) {
    argsTuple.newRef(PyTuple_New(argsCount));
    for (int i = 0; i < argsCount; i++) {
      // steals reference
      PyTuple_SetItem(argsTuple, i, args.at(i));
    }
  }

  PyErr_Clear();
  result = PyObject_CallObject(callable, argsTuple);
  rethrowPythonException();

  return result;
}


PyObject* call(const char* callable, const arguments& args) {
  return call(lookupCallable(getMainModule(), callable), args);
}


GILLocker::GILLocker() :
  _locked(false) {
  // autolock GIL in scoped_lock style
  lock();
}


GILLocker::~GILLocker() {
  release();
}


void GILLocker::release() {
  if (_locked) {
    assert(Py_IsInitialized());
    PyGILState_Release(_pyGILState);
    _locked = false;
  }
}


void GILLocker::lock() {
  if (!_locked) {
    assert(Py_IsInitialized());
    _pyGILState = PyGILState_Ensure();
    _locked = true;
  }
}


PyObject* getMainModule() {
  PyObject* mainModule = PyImport_AddModule("__main__");
  assert(mainModule);
  return mainModule;
}


PyObject* getMainDict() {
  PyObject* mainDict = PyModule_GetDict(getMainModule());
  assert(mainDict);
  return mainDict;
}

}
