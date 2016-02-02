/**
 * cppy2 -- embed python into your c++ app in 10 minutes
 *
 * (c) 2015 Dennis Shilko
 *
 * Minimalistic library for embedding python 2 in C++ application.
 * No additional dependencies required.
 * Linux, Windows platforms supported
 * Qt5 dependency is optional (QString unicode converters)
 *
 * - Convenient rererence-counted holder for PyObject*
 * - Simple wrapper over init/shutdown of interpreter in 1 line of code
 * - Manage GIL with scoped lock/unlock guards
 * - Translate exceptions from Python to to C++ layer
 * - C++ abstractions for list, dict and numpy.ndarray
 *
 */
#pragma once

// python (undefs surpress warnings from python.h)
#undef _POSIX_C_SOURCE
#undef _XOPEN_SOURCE
#include <Python.h>

// Qt
#ifdef QT_CORE_LIB
 #include <QString>
 #include <QStringList>
 #include <QVector>
#endif

// std
#include <exception>
#include <string>
#include <list>

#include "libdefs.hpp"

namespace cppy2 {

// forward decl
class Var;
class List;
class Dict;
class PyExceptionData;
class PythonException;
class Main;

/** exec python script from text string */
LIB_API Var exec(const char* pythonScript);
LIB_API Var exec(const QString& pythonScript);

/** exec python script file */
LIB_API Var execScriptFile(const QString& path);

/** @return true if python exception occured */
LIB_API bool error();

/** Add to sys.path */
void appendToSysPath(const QStringList& paths);

/** Send ctrl-c */
void interrupt();

/** Set sys.argv */
void setArgv(const std::list<std::string>& argv);

/** Make instance of a class */
Var createClassInstance(const QString& callable);

/**
 * @returns pointer to object of root python module __main__
 * can be reached with PyImport_AddModule("__main__")
 * or PyDict_GetItemString(PyImport_GetModuleDict(), "__main__")
 * either way is right
 */
LIB_API PyObject* getMainModule();
LIB_API PyObject* getMainDict();

/**
 * writes python error output to object
 * @param clearError - if true function will reset python error status before exit
 * @return python exception info and traceback in object
 */
LIB_API PyExceptionData getErrorObject(bool clearError = false);

/** throws c++ exception if python exception occured */
LIB_API void rethrowPythonException();

/** import python module into given context */
LIB_API Var import(const char* moduleName, PyObject* globals = NULL, PyObject* locals = NULL);

/** call python callable object and return result */
typedef QList<Var> arguments;
LIB_API PyObject* call(PyObject* callable, const arguments& args = arguments());
LIB_API PyObject* call(const char* callable, const arguments& args = arguments());

/** get reference to an object in python's namespace */
LIB_API Var lookupObject(PyObject* module, const QString& name);
LIB_API Var lookupCallable(PyObject* module, const QString& name);

/**
 * Tiny wrapper over CPython interpreter instance
 * to manage init/shutdown and expose api in most simple way
 */
class PythonVM {

public:
  PythonVM(const std::string& programName = "");
  ~PythonVM();
};


struct PyExceptionData {
  QString type;
  QString reason;
  QStringList trace;

  explicit PyExceptionData(const QString& reason = QString()) throw() : reason(reason) {}
  explicit PyExceptionData(const QString& type, const QString& reason, const QStringList& trace) throw() : type(type), reason(reason), trace(trace) {}

  bool isEmpty() const throw() {
    return type.isEmpty() && reason.isEmpty();
  }

  QString toString() const {
    return isEmpty() ? QString() : type + "\n" + reason + "\n" + trace.join("\n");
  }
};

class PythonException : public std::exception
{
public:
  PythonException(const PyExceptionData& info_) : info(info_) {}
  PythonException(const QString& reason) : info(reason) {}
  ~PythonException() throw() {}

  const char* what() const throw() {
    return info.toString().toLocal8Bit();
  }
  const PyExceptionData info;
};


/**
 * Setters / getters for access and manipulation with python vars and namespaces
 */
LIB_API PyObject* convert(const char* value);
LIB_API PyObject* convert(const QString& value);
LIB_API PyObject* convert(const std::wstring& value);
LIB_API PyObject* convert(const int& value);
LIB_API PyObject* convert(const double& value);

template<typename T>
PyObject* convert(const QVector<T>& value) {
  PyObject* o = PyList_New(value.size());
  assert(o);

  for (size_t i = 0; i < value.size(); ++i) {
    PyObject* item = convert(value[i]);
    assert(item);
    int r = PyList_SetItem(o, i, item);
    assert(r == 0);
  }
  return o;
}

#if HAVE_MATRIX_CONTAINER
template<typename T>
PyObject* varCreator(const Matrix<T>& value) {
  PyObject* o = PyList_New(value.rows());
  assert(o);

  for (size_t r = 0; r < value.rows(); ++r) {
    PyObject* row = PyList_New(value.rows());
    int res = PyList_SetItem(o, r, row);
    assert(res == 0);

    for (size_t c = 0; c < value.cols(); ++c) {
      PyObject* item = varCreator(value[r][c]);
      int r = PyList_SetItem(row, c, item);
      assert(r == 0);
    }
  }
  return o;
}
#endif

LIB_API void extract(PyObject* o, QString& value);
LIB_API void extract(PyObject* o, int& value);
LIB_API void extract(PyObject* o, double& value);

template<typename T>
void extract(PyObject* o, QVector<T>& value) {
  assert(o);
  value.reset(0);

  if (PyList_Check(o)) {
    value.reset(PyList_Size(o));
    for (int i = 0; i < PyList_Size(o); i++) {
      PyObject* item = PyList_GetItem(o, i);
      T itemValue;
      extract(item, itemValue);
      value[i] = itemValue;
    }
  } else {throw PythonException("variable is not a list");
  }
}


#if HAVE_MATRIX_CONTAINER
template<typename T>
void varGetter(PyObject* o, Matrix<T>& value) {
  assert(o);
  value.reset(0, 0);

  if (!PyList_Check(o)) {
    throw PyException("getMatrix(): variable is not a list");
  }

  if (PyList_Size(o) == 0) {
    value.reset(0, 0);
    return;
  }

  PyObject* row = PyList_GetItem(o, 0);
  assert(row);
  int rowSize = PyList_Size(o);
  int colSize = PyList_Size(row);
  value.reset(rowSize, colSize);

  for (int r = 0; r < rowSize; r++) {
    row = PyList_GetItem(o, r);
    if (!PyList_Check(row)) {
      throw PyException("getMatrix(): list item is not a list");
    }

    if (colSize != PyList_Size(row)) {
      throw PyException("getMatrix(): matrix rows have different size");
    }

    for (int c = 0; c < colSize; c++) {
      PyObject* item = PyList_GetItem(row, c);
      T itemValue;
      varGetter(item, itemValue);
      value[r][c] = itemValue;
    }
  }
}
#endif

/**
 * In python everything is a variable object instance.
 * Base wrapper for PyObject with reference counter
 */
class LIB_API Var {
public:

  /**
   * Python's basic object types
   * Used mostly for internal checks
   */
  enum Type {
    UNKNOWN,
    INT,
    LONG,
    BOOL,
    FLOAT,
    STRING,
    LIST,
    DICT,
    TUPLE,
    NUMPY_NDARRAY,
    MODULE
  };

  /** Construct empty */
  Var() : _o(NULL) {}

  /** Construct holder for given PyObject */
  explicit Var(PyObject* object) : _o(NULL) {
    reset(object);
  }

  /** Construct holder for given PyObject */
  Var(const Var& other) : _o(NULL) {
    reset(other.data());
  }

  /**
   * Construct holder for object parent[name]
   */
  explicit Var(const char* name, const PyObject* parent) : _o(NULL) {
    assert(name && parent);
    reset(PyDict_GetItemString(const_cast<PyObject*>(parent), name));
    assert(_o);
  }

  /**
   * Construct holder for object parent[name]
   */
  explicit Var(const std::wstring& name, const PyObject* parent) : _o(NULL) {
    assert(parent);
    reset(PyDict_GetItem(const_cast<PyObject*>(parent), Var::from(convert(name))));
    assert(_o);
  }

  ~Var() {
    decref();
  }

  /**
   * Getter for object with @b name in context of @b this
   */
  Var var(const char* name) const {
    assert(_o && "to get child object this must have parent");
    return Var(name, _o);
  }

  Var var(const std::wstring& name) const {
    assert(_o && "to get child object this must have parent");
    return Var(name, _o);
  }


  /**
   * Hold PyObject
   * reference increased
   * Used for borrowed objects
   * @param[in] o - pointer to PyObject
   */
  void reset(PyObject* o) {
    if (o != _o) {
      decref();
      _o = o;
      Py_XINCREF(_o);
    }
  }

  /**
   * Hold PyObject without increase of reference
   * Become owner of given object
   * Used mostly for owned objects
   * @param[in] o - pointer to PyObject
   */
  void newRef(PyObject* o) {
    if (o != _o) {
      decref();
      _o = o;
    }
  }

  /** static alias for newRef() */
  static Var from(PyObject* o) {
    Var v;
    v.newRef(o);
    return v;
  }

  /**
   * Get pointer to contained PyObject
   */
  PyObject* data() const {
    return _o;
  }

  /**
   * Check if contained pointer is NULL or PyObject is None
   */
  bool none() const {
    return (null() || (_o == Py_None));
  }

  /**
   * Check if contained pointer is NULL
   */
  bool null() const {
    return (_o == NULL);
  }

  /**
   * Decrement reference counter
   */
  void decref() {
    assert(!_o || (_o && (Py_REFCNT(_o) > 0)));
    Py_XDECREF(_o);
  }

  /**
   * Get text representation of PyObject type
   */
  const char* typeName() const {
    assert(_o);
    return _o->ob_type->tp_name;
  }

  /**
   * PyObject access operators
   */
  PyObject* operator->() const {
    return data();
  }

  PyObject& operator*() const {
    return *data();
  }

  operator PyObject*() const {
    return data();
  }


  /**
   * Make python object and inject into this
   */
  template<typename T>
  void injectVar(const QString& varName, const T& value) {
    PyObject* o = convert(value);
    inject(varName, o);
  }

  /**
   * inject object into this
   */
  void inject(const QString& varName, PyObject* o) {
    int r = PyDict_SetItemString(*this, varName.toLocal8Bit().constData(), o);
    r = r; // surpress compiler warning in release build
    assert(r == 0);
  }


  /**
   * Get PyObject with @b varName in @b this context convert data to @b value
   */
  template<typename T>
  void getVar(const QString& varName, T& value) const {
    PyObject* o = PyDict_GetItemString(*this, varName.toLocal8Bit().constData());
    assert(o);
    extract(o, value);
  }

  template<typename T>
  void getList(const QString& varName, QVector<T>& value) const {
    PyObject* o = PyDict_GetItemString(*this, varName.toLocal8Bit().constData());
    assert(o);
    extract(o, value);
  }

#if HAVE_MATRIX_CONTAINER
  template<typename T>
  void getMatrix(const QString& varName, Matrix<T>& value) const {
    PyObject* o = PyDict_GetItemString(*this, varName.toLocal8Bit().constData());
    assert(o);
    varGetter(o, value);
  }
#endif

  /**
   * Get text representation. Equal to python str() or repr()
   */
  static QString toString(PyObject* val);
  QString toString() const;

  /**
   * Get type
   */
  Type type() const;

protected:
  PyObject* _o;

};

/**
 * Adapter for python list type
 */
class LIB_API List : public Var {
public:
  List(const char* name, PyObject* parent) : Var(name, parent) {
    _validate();
  }

  List(PyObject* obj = NULL) : Var(obj) {
    _validate();
  }

  void reset(PyObject* o) {
    Var::reset(o);
    _validate();
  }

  size_t size() {
    const Py_ssize_t size = PyList_Size(_o);
    if (size == -1) {
      throw PythonException("invalid python list object");
    }
    return size;
  }

  Var operator[](const size_t i) {
    if (i >= size()) {
      throw PythonException("List index of of bounds");
    }
    return Var(PyList_GetItem(_o, i));
  }

  void remove(const size_t i) {
    const int result = PySequence_DelItem(_o, i);
    if (result == -1) {
      throw PythonException(QString("PySequence_DelItem index=%1 failed on list object").arg(i));
    }
  }

  bool contains(PyObject* element) {
    const int result = PySequence_Contains(_o, element);
    if (result == -1) {
      throw PythonException("PySequence_Contains failed on list object");
    }
    return result;
  }

  void append(PyObject* element) {
    const int result = PyList_Append(_o, element);
    if (result == -1) {
      throw PythonException("PyList_Append failed on list object");
    }
  }

  void insert(const size_t index, PyObject* element) {
    const int result = PyList_Insert(_o, index, element);
    if (result == -1) {
      throw PythonException("PyList_Insert failed on list object");
    }
  }

private:
  void _validate() const {
    if (_o) {
      assert(type() == LIST);
    }
  }

};


/**
 * Adapter for python dict type
 */
class LIB_API Dict : public Var {
public:
  Dict(const char* name, PyObject* parent) : Var(name, parent) {}


  Dict(PyObject* o) : Var(o) {
    assert(type() == DICT);
  }

  /**
   * Accessors to nested variables
   */
  Dict dict(const char* name) {
    assert(Var(name, _o).type() == DICT);
    return Dict(name, _o);
  }

  Dict dict(const QString& name) { return dict(name.toLocal8Bit().constData()); }


  List list(const char* name) const {
    assert(Var(name, _o).type() == LIST);
    return List(name, _o);
  }

  Dict moduledict(const char* name) {
    assert(Var(name, _o).type() == MODULE);
    return Dict(PyModule_GetDict(Var(name, _o)));
  }

  Dict moduledict(const QString& name) { return moduledict(name.toLocal8Bit().constData()); }


  /** @} */

  bool contains(const char* name) const {
    assert(type() == DICT);
    Var key;
    key.newRef(convert(name));
    return PyDict_Contains(_o, key.data());
  }

  void clear() {
    PyDict_Clear(_o);
  }

};


/**
 * Adapter for python root '__main__' namespace dict
 */
class LIB_API Main : public Dict {
public:
  Main() : Dict(getMainDict()) {}
};


/**
 * GIL state scoped-lock
 * can be used recursively (like recursive mutex)
 */
class LIB_API GILLocker {
public:
  GILLocker();
  ~GILLocker();
private:
  void lock();
  void release();
  bool _locked;
  PyGILState_STATE _pyGILState;
};


/**
 * @brief The Scoped GIL unlocker
 */
class ScopedGILRelease {
public:
  ScopedGILRelease() {
    _threadState = PyEval_SaveThread();
  }

  ~ScopedGILRelease() {
    PyEval_RestoreThread(_threadState);
  }
private:
  PyThreadState* _threadState;
};


/**
 * @brief The Scoped GIL locker
 */
class ScopedGILLock {
public:
  ScopedGILLock() {
    _state = PyGILState_Ensure();
  }

  ~ScopedGILLock() {
    PyGILState_Release(_state);
  }
private:
  PyGILState_STATE _state;
};


} // namespace
