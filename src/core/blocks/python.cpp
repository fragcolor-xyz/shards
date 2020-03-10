#ifndef CB_PYTHON_HPP
#define CB_PYTHON_HPP

#include "shared.hpp"

/*
 * This block will always work.. even if no python is present
 * We always try to dyn link and we provide always interface here
 * no headers required, no linking required
 */

namespace chainblocks {
namespace Python {
inline bool hasLib(const char *lib_name) {
#if _WIN32
  HMODULE mod = GetModuleHandleA(lib_name);
  if (mod == 0) {
    mod = LoadLibraryA(lib_name);
    if (mod == 0)
      return false;
  }
  return true;
#elif defined(__linux__) || defined(__APPLE__)
  void *mod = dlopen(lib_name, RTLD_LOCAL | RTLD_NODELETE);
  if (mod == 0)
    return false;
  dlclose(mod);
  return true;
#else
  return false;
#endif
}

inline void *dlsym(const char *lib_name, const char *sym_name) {
#if _WIN32
  HMODULE mod = GetModuleHandleA(lib_name);
  if (mod == 0) {
    mod = LoadLibraryA(lib_name);
    if (mod == 0)
      return 0;
  }

  return (void *)GetProcAddress(mod, sym_name);
#elif defined(__linux__) || defined(__APPLE__)
  void *mod = dlopen(lib_name, RTLD_LOCAL | RTLD_NODELETE);
  if (mod == 0)
    return 0;
  void *sym = dlsym(mod, sym_name);
  dlclose(mod);
  return sym;
#else
  return nullptr;
#endif
}

struct PyObject;
struct PyTypeObject;

typedef void(__cdecl *PyDealloc)(PyObject *p);

struct PyObject {
  ssize_t refcount;
  PyTypeObject *type;
};

struct PyObjectVarHead : public PyObject {
  ssize_t size;
};

struct PyTypeObject : public PyObjectVarHead {
  const char *name;
  ssize_t basicsize;
  ssize_t itemsize;
  PyDealloc dealloc;
};

using PyObj = std::shared_ptr<PyObject>;

struct Env {
  static inline std::vector<const char *> lib_names = {
      "python38.dll",       "python37.dll",      "python3.dll",
      "python.dll",         "libpython38.so.1",  "libpython38.so",
      "libpython38m.so.1",  "libpython38m.so",   "libpython37.so.1",
      "libpython37.so",     "libpython37m.so.1", "libpython37m.so",
      "libpython3.so.1",    "libpython3.so",     "libpython3m.so.1",
      "libpython3m.so",     "libpython.so",      "libpython.so.1",
      "libpythonm.so",      "libpythonm.so.1",   "libpython38.dylib",
      "libpython38m.dylib", "libpython37.dylib", "libpython37m.dylib",
      "libpython3.dylib",   "libpython3m.dylib", "libpython.dylib",
      "libpythonm.dylib",
  };

  typedef void(__cdecl *Py_Initialize)();
  typedef PyObject *(__cdecl *PyUnicode_DecodeFSDefault)(const char *str);
  typedef PyObject *(__cdecl *PyImport_Import)(PyObject *name);
  typedef PyObject *(__cdecl *PyObject_GetAttrString)(PyObject *obj,
                                                      const char *str);
  typedef int *(__cdecl *PyCallable_Check)(PyObject *obj);
  typedef PyObject *(__cdecl *PyObject_CallObject)(PyObject *obj,
                                                   PyObject *args);
  typedef PyObject *(__cdecl *PyTuple_New)(ssize_t n);
  typedef void *(__cdecl *PyTuple_SetItem)(PyObject *tuple, ssize_t idx,
                                           PyObject *item);

  static inline PyUnicode_DecodeFSDefault _makeStr;
  static inline PyImport_Import _import;
  static inline PyObject_GetAttrString _getAttr;
  static inline PyCallable_Check _callable;
  static inline PyObject_CallObject _call;
  static inline PyTuple_New _tupleNew;
  static inline PyTuple_SetItem _tupleSetItem;

  static PyObj make_pyshared(PyObject *p) {
    std::shared_ptr<PyObject> res(p, [](auto p) {
      p->refcount--;
      if (p->refcount == 0) {
        LOG(TRACE) << "PyObj::Dealloc";
        p->type->dealloc(p);
      }
    });
    return res;
  }

  static bool ensure(void *ptr) {
    if (!ptr) {
      LOG(ERROR) << "Failed to initialize python environment!";
      return false;
    }
    return true;
  }

  static void Init() {
    auto pos = std::find_if(std::begin(lib_names), std::end(lib_names),
                            [](auto &&str) { return hasLib(str); });
    if (pos != std::end(lib_names)) {
      auto idx = std::distance(std::begin(lib_names), pos);
      auto dll = lib_names[idx];

      auto init = (Py_Initialize)dlsym(dll, "Py_Initialize");
      if (!ensure((void *)init))
        return;
      init();

      _makeStr =
          (PyUnicode_DecodeFSDefault)dlsym(dll, "PyUnicode_DecodeFSDefault");
      if (!ensure((void *)_makeStr))
        return;

      _import = (PyImport_Import)dlsym(dll, "PyImport_Import");
      if (!ensure((void *)_import))
        return;

      _getAttr = (PyObject_GetAttrString)dlsym(dll, "PyObject_GetAttrString");
      if (!ensure((void *)_getAttr))
        return;

      _callable = (PyCallable_Check)dlsym(dll, "PyCallable_Check");
      if (!ensure((void *)_callable))
        return;

      _call = (PyObject_CallObject)dlsym(dll, "PyObject_CallObject");
      if (!ensure((void *)_call))
        return;

      _tupleNew = (PyTuple_New)dlsym(dll, "PyTuple_New");
      if (!ensure((void *)_tupleNew))
        return;

      _tupleSetItem = (PyTuple_SetItem)dlsym(dll, "PyTuple_SetItem");
      if (!ensure((void *)_tupleSetItem))
        return;

      LOG(INFO) << "Python found, Py blocks will work!";
    } else {
      LOG(INFO) << "Python not found, Py blocks won't work.";
    }
  }

  static PyObj string(const char *name) {
    return make_pyshared(_makeStr(name));
  }

  static PyObj import(const char *name) {
    auto pyname = string(name);
    return make_pyshared(_import(pyname.get()));
  }

  static PyObj getAttr(const PyObj &obj, const char *attr_name) {
    return make_pyshared(_getAttr(obj.get(), attr_name));
  }

  static bool isCallable(const PyObj &obj) {
    return obj.get() && _callable(obj.get());
  }

  template <typename... Ts> static PyObj call(const PyObj &obj, Ts... vargs) {
    // this trick should allocate a shared single tuple for n kind of call
    constexpr std::size_t n = sizeof...(Ts);
    std::array<PyObj, n> args{vargs...};
    const static auto tuple = _tupleNew(n);
    for (size_t i = 0; i < n; i++) {
      _tupleSetItem(tuple, ssize_t(i), args[i].get());
    }
    return make_pyshared(_call(obj.get(), tuple));
  }

  static PyObj call(const PyObj &obj) {
    return make_pyshared(_call(obj.get(), nullptr));
  }

  static bool ok() { return _ok; }

  static PyObj var2Py(const CBVar &var) {}

  static CBVar py2Var(const PyObj &obj) {}

private:
  static inline bool _ok{false};
};

struct Py {
  void reloadScript() {
    if (!Env::ok()) {
      LOG(ERROR) << "Script: " << _scriptName
                 << " cannot be loaded, no python support.";
      throw CBException("Failed to load python script!");
    }

    _module = Env::import(_scriptName.c_str());

    _inputTypes = Env::getAttr(_module, "inputTypes");
    if (!Env::isCallable(_inputTypes)) {
      LOG(ERROR) << "Script: " << _scriptName << " has no callable inputTypes.";
      throw CBException("Failed to reload python script!");
    }

    _outputTypes = Env::getAttr(_module, "outputTypes");
    if (!Env::isCallable(_outputTypes)) {
      LOG(ERROR) << "Script: " << _scriptName
                 << " has no callable outputTypes.";
      throw CBException("Failed to reload python script!");
    }

    _activate = Env::getAttr(_module, "activate");
    if (!Env::isCallable(_activate)) {
      LOG(ERROR) << "Script: " << _scriptName << " has no callable activate.";
      throw CBException("Failed to reload python script!");
    }

    auto setup = Env::getAttr(_module, "setup");
    if (Env::isCallable(setup)) {
      Env::call(setup);
    }
  }

  CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  CBTypesInfo outputTypes() { return CoreInfo::AnyType; }
  CBVar activate(CBContext *context, const CBVar &input) { return CBVar(); }

private:
  PyObj _module;
  PyObj _inputTypes;
  PyObj _outputTypes;
  PyObj _activate;
  std::string _scriptName;
};

void registerBlocks() {
  Env::Init();
#ifndef NDEBUG
  if (Env::ok()) {
    auto pystr =
        Env::string("Hello Pythons...as much as I dislike this... hello..");
  }
#endif
  REGISTER_CBLOCK("Py", Py);
}
} // namespace Python
} // namespace chainblocks

#endif /* CB_PYTHON_HPP */
